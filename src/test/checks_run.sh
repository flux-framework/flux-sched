#!/bin/bash
#
#  Test runner script meant to be executed inside of a docker container
#
#  Usage: checks_run.sh [OPTIONS...]
#
#  Where OPTIONS are passed directly to ./configure
#
#  The script is otherwise influenced by the following environment variables:
#
#  JOBS=N        Argument for make's -j option, default=2
#  PROJECT       Flux project, e.g. flux-core
#  COVERAGE      Run with --enable-code-coverage, `make check-code-coverage`
#  TEST_INSTALL  Run `make check` against installed flux-core
#  CPPCHECK      Run cppcheck if set to "t"
#  DISTCHECK     Run `make distcheck` if set
#  RECHECK       Run `make recheck` if `make check` fails the first time
#  PRELOAD       Set as LD_PRELOAD for make and tests
#  POISON        Install poison libflux and flux(1) in image
#  INCEPTION     Run tests under a flux instance with prove(1)
#  chain_lint    Run sharness with --chain-lint if chain_lint=t
#  SANITIZE      Build with ASan/UBSan and run the tests under them if set
#
#  And, obviously, some crucial variables that configure itself cares about:
#
#  CC, CXX, LDFLAGS, CFLAGS, etc.
#

# if make is old, and scl is here, and devtoolset is available and not turned
# on, re-exec ourself with it active to get a newer make
if make --version | grep 'GNU Make 4' 2>&1 > /dev/null ; then
  MAKE="make --no-print-directory"
else
  MAKE="make" #use this if all else fails
  if test "X$X_SCLS" = "X" ; then
    if scl -l | grep devtoolset-7 2>&1 >/dev/null ; then
      echo  bash "$0" "$@" | scl enable devtoolset-7 -
      exit
    fi
  fi
fi

if test -n "$CHECK_RUN_SOURCE_ENV" ; then
	echo Sourcing $CHECK_RUN_SOURCE_ENV
	source "$CHECK_RUN_SOURCE_ENV"
fi

# source check_group and check_time functions:
. src/test/checks-lib.sh

ARGS="$@"
JOBS=${JOBS:-2}
export CMAKE_BUILD_PARALLEL_LEVEL=$JOBS
export CTEST_PARALLEL_LEVEL=$JOBS
export CTEST_OUTPUT_ON_FAILURE=1
export VERBOSE=1
MAKECMDS="${MAKE} -j ${JOBS}"
CHECKCMDS="${MAKE} -j ${JOBS} ${DISTCHECK:+dist}check"

# Add non-standard path for libfaketime to LD_LIBRARY_PATH:
export LD_LIBRARY_PATH="/usr/lib/x86_64-linux-gnu/faketime"

# Force git to update the shallow clone and include tags so git-describe works
checks_group "git fetch tags" "git fetch --unshallow --tags" \
 git fetch --unshallow --tags || true

checks_group_start "build setup"
ulimit -c unlimited

# Manually update ccache symlinks (XXX: Is this really necessary?)
test -x /usr/sbin/update-ccache-symlinks && \
    sudo /usr/sbin/update-ccache-symlinks
export PATH=/usr/lib/ccache:$PATH

# Ensure ccache dir exists
mkdir -p $HOME/.ccache

# clang+ccache requires second cpp pass:
if echo "$CC" | grep -q "clang"; then
    CCACHE_CPP=1
fi

if test "$PROJECT" = "flux-core"; then
  # Ensure ci builds libev such that libfaketime will work:
  # (force libev to *not* use syscall interface for clock_gettime())
  export CPPFLAGS="$CPPFLAGS -DEV_USE_CLOCK_SYSCALL=0 -DEV_USE_MONOTONIC=1"

  # Ensure we always use internal <flux/core.h> by placing a dummy file
  #  in the same path as ZMQ_FLAGS:
  sudo sh -c "mkdir -p /usr/include/flux \
    && echo '#error Non-build-tree flux/core.h!' > /usr/include/flux/core.h"
fi

# Enable coverage for $CC-coverage build
# We can't use distcheck here, it doesn't play well with coverage testing:
if test "$COVERAGE" = "t"; then
	# usercustomize.py must go under USER_SITE, so determine that path:
	USER_SITE=$(python3 -c 'import site; print(site.USER_SITE)')
	mkdir -p ${USER_SITE}

	# Setup environment for Python coverage
	# This file will be loaded by all python scripts run by the
	# current user, but only activate coverage if COVERAGE_PROCESS_START
	# is set in the environment.
	#
	cat <<-EOF >${USER_SITE}/usercustomize.py
	try:
	    import coverage
	    coverage.process_startup()
	except ImportError:
	    pass
	EOF

	# Add Python coverage config:
	cat <<-EOF >coverage.rc
	[run]
	data_file = $(pwd)/.coverage
	include = $(pwd)/src/*
	parallel = True
	relative_files = True
	EOF

	rm -f .coverage .coverage*

	ARGS="$ARGS --enable-code-coverage"
	CHECKCMDS="\
	export ENABLE_USER_SITE=1 && \
	export COVERAGE_PROCESS_START=$(pwd)/coverage.rc && \
	${MAKE} -j $JOBS check-code-coverage && \
	lcov -l flux*-coverage.info && \
	coverage combine .coverage* && \
	(coverage xml || :) &&
	(chmod 444 coverage.xml || :) &&
	(coverage report || :)"

# Use make install for installonly
elif test "$INSTALL_ONLY" = "t"; then
    ARGS="$ARGS --prefix=/usr --sysconfdir=/etc"
    CHECKCMDS="sudo make install"

# Use make install for T_INSTALL:
elif test "$TEST_INSTALL" = "t"; then
    ARGS="$ARGS --prefix=/usr --sysconfdir=/etc"
    CHECKCMDS="sudo make install && \
              FLUX_TEST_INSTALLED_PATH=/usr/bin ${MAKE} -j $JOBS check"

# Run checks under prove(1):
elif test "$INCEPTION" = "t"; then
	CHECKCMDS="make -j ${JOBS} check-prep && \
	           cd t && ../src/cmd/flux start -s1 ./prove-under-flux.sh && \
	           cd .."
fi

# Sanitizer build: build with ASan + UBSan and run the whole ctest suite under
# them, in two passes.
#
# flux-sched's libraries, modules and test binaries are sanitized, but the flux
# broker that dlopen()s sched-fluxion-*.so is not.  For the ASan runtime to work
# inside a shared object loaded by an uninstrumented process it must already be
# in that process, so the live tests need the runtime preloaded; without it the
# broker segfaults on the first fluxion module load.  FLUX_LOAD_WITH_DEEPBIND=0
# additionally clears RTLD_DEEPBIND from flux-core's dlopen() flags (needs
# flux-core >= v0.64.0) -- the sanitizer runtimes refuse to run under
# RTLD_DEEPBIND and abort the broker otherwise.
#
# The preload cannot simply be applied to the whole suite, because it also
# instruments unrelated system binaries the tests shell out to.  Notably
# coreutils split(1), which t3022/t3023 use, calls aligned_alloc() with a size
# that is not a multiple of the alignment: glibc allows that, but ASan enforces
# the stricter C11 rule and aborts with "invalid alignment requested in
# aligned_alloc", so split(1) dies with "memory exhausted".
#
# Scoping the preload to the broker via the FLUX_TEST_WRAP hook in
# t/sharness.d/flux-sharness.sh is not enough either: that hook only wraps the
# `flux start` that test_under_flux() runs, and several tests (t1005, t4010)
# invoke `flux broker` directly.
#
# So: run the tests that need no broker (the C++ unit tests and the
# resource-query t3xxx suites, which are instrumented executables that link the
# runtime themselves) without the preload, then run the live tests with it.
if test "$SANITIZE" = "t"; then
	# This block replaces CHECKCMDS wholesale, so it cannot be combined
	# with the modes above that build their own.
	if test -n "${COVERAGE}${INSTALL_ONLY}${TEST_INSTALL}${INCEPTION}${DISTCHECK}"; then
		printf "::error::SANITIZE cannot be combined with COVERAGE, INSTALL_ONLY, TEST_INSTALL, INCEPTION or DISTCHECK\n"
		exit 1
	fi
	ARGS="$ARGS --enable-sanitize-address --enable-sanitize-undefined"

	# gcc names the shared runtime libasan.so; clang names it
	# libclang_rt.asan-<arch>.so or libclang_rt.asan.so.  Both compilers
	# -print-file-name the full path when the library exists, and echo the
	# bare name when it does not.  Ask for the names of clang first: clang
	# also finds the gcc runtime, but the preload must be the runtime that
	# the sanitized modules link.
	for name in libclang_rt.asan-$(uname -m).so \
		    libclang_rt.asan.so \
		    libasan.so; do
		ASAN_LIB=$(${CC:-gcc} -print-file-name=$name)
		test -e "$ASAN_LIB" && break
		ASAN_LIB=
	done
	if test -z "$ASAN_LIB"; then
		printf "::error::cannot locate the ASan runtime for ${CC:-gcc}\n"
		exit 1
	fi
	export FLUX_LOAD_WITH_DEEPBIND=0

	# UBSan suppressions.  Written outside the (bind-mounted) source
	# tree so the build leaves no stray file behind.  See the comment
	# for why the entry is here; keep the list short and justified.
	UBSAN_SUPP=${TMPDIR:-/tmp}/flux-sched-ubsan.supp
	cat <<-EOF >${UBSAN_SUPP}
	# resource/reapi/test/reapi_cli_test_cxx.cpp builds out-of-bounds
	# match_op_t values on purpose to check that match_op_valid ()
	# rejects them, so -fsanitize=enum necessarily fires there.
	enum:reapi_cli_test_cxx.cpp
	EOF

	# LeakSanitizer is off: flux-sched has pre-existing leaks in the
	# reapi CLI bindings (the options json_t in
	# resource_query_t::set_resource_ctx_params () and the strdup()ed
	# buffers the C binding hands back) that the unit tests do not free.
	# Turn this back on once those are fixed.
	#
	# The ODR check is off: each flux broker module defines the global
	# mod_name (the MOD_NAME macro of flux-core), and the ASan runtime of
	# clang reports an ODR violation when the broker loads a second
	# module.  That is the module protocol of flux-core, not an error.
	export ASAN_OPTIONS="detect_leaks=0:detect_odr_violation=0${ASAN_OPTIONS:+:$ASAN_OPTIONS}"
	export UBSAN_OPTIONS="print_stacktrace=1:suppressions=${UBSAN_SUPP}${UBSAN_OPTIONS:+:$UBSAN_OPTIONS}"

	# Tests that start a broker, and therefore need the preload.
	SANITIZE_LIVE_RE='^(t1[0-9]|t2[0-9]|t4[0-9]|t5[0-9]|t6[0-9]|t8[0-9]|t9[0-9]|t3300|t3301)'

	# The valgrind test (t5000) runs the broker under valgrind, and the
	# preloaded ASan runtime does not come first in that process. The
	# test skips by itself when flux-core is sanitized, but flux-core is
	# not sanitized here, so leave FLUX_ENABLE_VALGRIND_TEST unset (see
	# below) and keep the test out of the live pass.
	SANITIZE_EXCLUDE_RE='^t5000'

	# checks_group evaluates CHECKCMDS with eval two times, which strips
	# the quotes around the regular expression and then parses its
	# parentheses as shell syntax.  Keep the commands in a function, so
	# that CHECKCMDS is one word and the quoting is ordinary.
	sanitize_check() {
		${CMAKE_CTEST_COMMAND:-ctest} --output-on-failure \
		    --output-junit "$(pwd)/test-results.xml" \
		    -E "$SANITIZE_LIVE_RE" &&
		LD_PRELOAD="$ASAN_LIB" ${CMAKE_CTEST_COMMAND:-ctest} \
		    --output-on-failure \
		    --output-junit "$(pwd)/test-results-live.xml" \
		    -R "$SANITIZE_LIVE_RE" -E "$SANITIZE_EXCLUDE_RE"
	}
	CHECKCMDS="sanitize_check"
fi

if test -n "$PRELOAD" ; then
  CHECKCMDS="/usr/bin/env 'LD_PRELOAD=$PRELOAD' ${CHECKCMDS}"
fi

# CI has limited resources, even though number of processors might
#  might appear to be large. Limit session size for testing to 5 to avoid
#  spurious timeouts.
export FLUX_TEST_SIZE_MAX=5

# Invoke MPI tests
# EL7: mpich only available via environment-module:
if test -f /usr/share/Modules/init/bash; then
    . /usr/share/Modules/init/bash && module load mpi
fi
export FLUX_TEST_MPI=t

# Generate logfiles from sharness tests for extra information:
export FLUX_TESTS_LOGFILE=t
export DISTCHECK_CONFIGURE_FLAGS="${ARGS}"

# Force enable valgrind test, except in the sanitizer build (see above)
test "$SANITIZE" = "t" || export FLUX_ENABLE_VALGRIND_TEST=t

if test "$CPPCHECK" = "t"; then
    sh -x src/test/cppcheck.sh
fi

echo "Starting MUNGE"
sudo /sbin/runuser -u munge /usr/sbin/munged

checks_group_end # Setup

checks_group "autogen.sh" ./autogen.sh

if test -n "$BUILD_DIR" ; then
  mkdir -p "$BUILD_DIR"
  cd "$BUILD_DIR"
fi

pwd
checks_group "configure ${ARGS}"  /usr/src/configure ${ARGS} \
	|| (printf "::error::configure failed\n"; cat config.log; exit 1)
checks_group "make clean..." make clean

if test "$POISON" = "t" -a "$PROJECT" = "flux-core"; then
  checks_group "Installing poison libflux..." \
    bash src/test/docker/poison-libflux.sh
fi

if test "$DISTCHECK" != "t"; then
  checks_group "${MAKECMDS}" eval ${MAKECMDS} \
	|| (printf "::error::${MAKECMDS} failed\n"; exit 1)
fi
checks_group "${CHECKCMDS}" eval "${CHECKCMDS}"
RC=$?

if test "$RECHECK" = "t" -a $RC -ne 0; then
  #
  # `make recheck` is not recursive, only perform it if at least some tests
  #   under ./t were run (and presumably failed), in ctest it is recursive, so
	#   just run it
  #
  if test -s t/t0000-sharness.trs -o -e ./CTestTestfile.cmake; then
    cd t
    printf "::warning::make check failed, trying recheck in ./t\n"
    checks_group "make recheck" ${MAKE} -j ${JOBS} recheck
    RC=$?
    cd
   else
      printf "::warning::recheck requested but no tests in ./t were run\n"
   fi
fi

exit $RC

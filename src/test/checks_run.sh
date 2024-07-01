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
#
#  And, obviously, some crucial variables that configure itself cares about:
#
#  CC, CXX, LDFLAGS, CFLAGS, etc.
#

# if make is old, and scl is here, and devtoolset is available and not turned
# on, re-exec ourself with it active to get a newer make
if make --version | grep 'GNU Make 4' 2>&1 > /dev/null ; then
  MAKE="make --output-sync=line --no-print-directory"
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

# Force enable valgrind test
export FLUX_ENABLE_VALGRIND_TEST=t

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

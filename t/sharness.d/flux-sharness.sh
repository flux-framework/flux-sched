
#
#  project-local sharness code for Flux
#

#
#  Unset variables important to Flux
#
unset FLUX_CONFIG
unset FLUX_MODULE_PATH
unset FLUX_CMBD_PATH

#
#  FLUX_PREFIX identifies the prefix for the flux we will be testing
#
FLUX_PREFIX=`pkg-config --variable=prefix flux-core`

if test -z "${FLUX_PREFIX}" ; then
    FLUX_PREFIX=/usr
fi

#
#  Extra functions for Flux testsuite
#
run_timeout() {
    perl -e 'alarm shift @ARGV; exec @ARGV' "$@"
}

#
#  Reinvoke a test file under a flux comms instance
#
#  Usage: test_under_flux <size> <flux-sched root dir>
#
test_under_flux() {
    size=$1
    tdir=$2
    log_file="$TEST_NAME.broker.log"
    if test -n "$TEST_UNDER_FLUX_ACTIVE" ; then
        cleanup rm "${SHARNESS_TEST_DIRECTORY:-..}/$log_file"
        unset TEST_UNDER_FLUX_ACTIVE
        return
    fi
    quiet="-o -q,-Slog-filename=${log_file}"
    if test "$verbose" = "t" -o -n "$FLUX_TESTS_DEBUG" ; then
        flags="${flags} --verbose"
        quiet=""
    fi
    if test "$debug" = "t" -o -n "$FLUX_TESTS_DEBUG" ; then
        flags="${flags} --debug"
    fi
    if test -n "$logfile" -o -n "$FLUX_TESTS_LOGFILE" ; then
        flags="${flags} --logfile"
    fi
    if test -n "$SHARNESS_TEST_DIRECTORY"; then
        cd $SHARNESS_TEST_DIRECTORY
    fi

    TEST_UNDER_FLUX_ACTIVE=t \
    TERM=${ORIGINAL_TERM} \
    LUA_PATH="${tdir}/rdl/?.lua;${FLUX_PREFIX}/share/lua/5.1/?.lua;${LUA_PATH};;" \
    LUA_CPATH="${tdir}/rdl/.libs/?.so;${FLUX_PREFIX}/lib64/lua/5.1/?.so;${LUA_CPATH};;" \
    FLUX_MODULE_PATH="${tdir}/sched" \
      exec flux start --size=${size} ${quiet} "sh $0 ${flags}"
}

mock_bootstrap_instance() {
    if test -z "${TEST_UNDER_FLUX_ACTIVE}"; then
        unset FLUX_URI
    fi
}

#
#  Execute arguments $2-N on rank or ranks specified in arg $1
#   using the flux-exec utility
#
test_on_rank() {
    test "$#" -ge 2 ||
        error "test_on_rank expects at least two parameters"
    test -n "$TEST_UNDER_FLUX_ACTIVE"  ||
        error "test_on_rank: test_under_flux not active ($TEST_UNDER_FLUX_ACTIVE)"

    ranks=$1; shift;
    flux exec --rank=${ranks} "$@"
}


#
#  Export some extra variables to test scripts specific to Flux
#   testsuite
#
#  Add path to flux(1) command to PATH
#
if test -n "$FLUX_TEST_INSTALLED_PATH"; then
    PATH=$FLUX_TEST_INSTALLED_PATH:$PATH
    fluxbin=$FLUX_TEST_INSTALLED_PATH/flux
else # normal case, use $FLUX_PREFIX
    PATH=$FLUX_PREFIX/bin:$PATH
    fluxbin=$FLUX_PREFIX/bin/flux
fi
export PATH

if ! test -x ${fluxbin}; then
    echo >&2 "Failed to find a flux binary in ${fluxbin}."
    echo >&2 "Do you need to run make?"
    return 1
fi

#  Export a shorter name for this test
TEST_NAME=$SHARNESS_TEST_NAME
export TEST_NAME

#  Test requirements for testsuite
if ! lua -e 'require "flux.posix"'; then
    error "failed to find lua posix module in path"
fi
if ! lua -e 'require "flux.hostlist"'; then
    error "failed to find lua hostlist module in path"
fi


# vi: ts=4 sw=4 expandtab

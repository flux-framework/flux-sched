#!/bin/sh

test_description='Run broker under valgrind with a small workload'

# Append --logfile option if FLUX_TESTS_LOGFILE is set in environment:
test -n "$FLUX_TESTS_LOGFILE" && set -- "$@" --logfile
. `dirname $0`/sharness.sh

if ! which valgrind >/dev/null; then
    skip_all='skipping valgrind tests since no valgrind executable found'
    test_done
fi
export FLUX_PMI_SINGLETON=1 # avoid finding leaks in slurm libpmi.so

VALGRIND=`which valgrind`
VALGRIND_SUPPRESSIONS=${SHARNESS_TEST_SRCDIR}/valgrind/valgrind.supp
VALGRIND_WORKLOAD=${SHARNESS_TEST_SRCDIR}/valgrind/valgrind-workload.sh

# broker run under valgrind may need extra retries in flux_open():
FLUX_LOCAL_CONNECTOR_RETRY_COUNT=10

# Broker under last component of FLUX_EXEC_PATH:
BROKER=$(PATH=${FLUX_EXEC_PATH} /usr/bin/which flux-broker)
test_expect_success 'found executable flux-broker' '
	echo "found broker at ${BROKER}" >&2 &&
	test -x "$BROKER"
'
# Do not run test by default unless valgrind wrapped dlclose is found in
#  flux-broker, since this has been known to introduce false positives
#  (flux-framework/flux-core#1097). However, allow run to be forced on
#  the cmdline with -d, --debug.
#
have_valgrind_h() {
    nm --defined-only ${BROKER} | grep -q ZZ_Za_dlclose
}
if ! have_valgrind_h && test "$debug" = ""; then
    skip_all='Skipping valgrind test because flux-broker does not wrap dlclose. Use -d to force.'
    test_done
fi


test_expect_success 'valgrind reports no new errors on single broker run' '
	flux ${VALGRIND} \
		--tool=memcheck \
		--leak-check=full \
		--gen-suppressions=all \
		--trace-children=no \
		--child-silent-after-fork=yes \
		--num-callers=30 \
		--leak-resolution=med \
		--error-exitcode=1 \
		--suppressions=$VALGRIND_SUPPRESSIONS \
		${BROKER} --shutdown-grace=4 ${VALGRIND_WORKLOAD} 10
'
test_done

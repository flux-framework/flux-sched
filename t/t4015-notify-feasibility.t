#!/bin/sh
#set -x

# Adapted from t2317

test_description='Test the functionality of match satisfiability after module restart'

. `dirname $0`/sharness.sh

conf_base=${SHARNESS_TEST_SRCDIR}/conf.d
notify_base=`readlink -e ${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/satisfiability`

SIZE=4
export FLUX_URI_RESOLVE_LOCAL=t
export FLUX_SCHED_MODULE=none

test_under_flux $SIZE full --test-exit-mode=leader

force_down () {
	flux python -c "import flux; flux.Flux().rpc(\"resource.monitor-force-down\", {\"ranks\":\"$1\"}).get()"
}

# If force-down is not supported then this version of flux-core does not
# support shrink, so skip all tests:
if ! force_down "" 2>/dev/null ; then
       skip_all='resource.monitor-force-down failed, skipping all tests'
	test_done
fi

test_expect_success 'loading non-load-file resource module works' '
    load_resource &&
    test -z "$(flux dmesg -c | grep -q err)"
'

test_expect_success 'loading feasibility from non-load-file resource module works' '
    load_feasibility &&
    test -z "$(flux dmesg -c | grep -q err)"
'

test_expect_success 'a job on all ranks is satisfiable' '
    flux ion-resource -v match satisfiability ${notify_base}/shrink4.yaml
'

test_expect_success 'disconnect rank 3' '
	flux overlay disconnect 3
'

test_expect_success 'there are now only 3 nodes' '
    flux resource list -s all &&
    test $(flux resource list -s all -no {nnodes}) -eq 3
'

test_expect_success 'a 4 node job is now unsatisfiable' '
    test_must_fail flux ion-resource match satisfiability ${notify_base}/shrink4.yaml
'

test_expect_success 'but a 3 node job is satisfiable' '
    flux ion-resource match satisfiability ${notify_base}/shrink3.yaml
'

test_expect_success 'a 4 node job is unsatisfiable after feasibility restart' '
    reload_feasibility &&
    test_must_fail flux ion-resource match satisfiability ${notify_base}/shrink4.yaml
'

test_expect_success 'removing resource works and removes feasibility' '
    remove_resource &&
    flux dmesg -c | grep -q "exiting due to sched-fluxion-resource.notify failure"
'

test_done

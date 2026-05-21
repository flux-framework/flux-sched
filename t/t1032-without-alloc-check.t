#!/bin/sh

# Adapted from t1024

test_description='
Check that match without-allocating does not book resources when a previous
allocation exceeds its walltime
'

. `dirname $0`/sharness.sh

hwloc_basepath=`readlink -e ${SHARNESS_TEST_SRCDIR}/data/hwloc-data`
# 1 brokers, each (exclusively) have: 1 node, 2 sockets, 16 cores (8 per socket)
excl_1N1B="${hwloc_basepath}/001N/exclusive/01-brokers"
excl_4N4B="${hwloc_basepath}/004N/exclusive/04-brokers-sierra2"

export FLUX_SCHED_MODULE=none
test_under_flux 4

if ! flux jobtap load alloc-check.so; then
    skip_all='this test requires the alloc-check.so plugin from flux core'
    test_done
fi

test_expect_success 'load test resources' '
    load_test_resources ${excl_1N1B}
'
test_expect_success 'load fluxion modules' '
	load_resource &&
	load_qmanager_sync
'

# Test for flux-framework/flux-sched#1043
#
test_expect_success 'configure epilog with delay' '
	flux config load <<-EOT &&
	[job-manager.epilog]
	per-rank = true
	command = [ "sleep", "2" ]
	EOT
	flux jobtap load perilog.so
'
test_expect_success 'submit a node-exclusive job that exceeds its time limit' '
	JOBID_WITH_EPILOG=$(flux submit -N1 -x -t1s sleep 30)
'
test_expect_success 'MWOA fails while the previous epilog is still running' '
	>jobspec.json flux run --dry-run -N1 -x hostname &&
	flux job wait-event --timeout=5s $JOBID_WITH_EPILOG epilog-start &&
	test_expect_code 16 flux ion-resource match without_allocating jobspec.json &&
	flux job wait-event --timeout=3s $JOBID_WITH_EPILOG epilog-finish
'
test_expect_success 'submit another node-exclusive job that exceeds its time limit' '
	JOBID_WITH_EPILOG=$(flux submit -N1 -x -t1s sleep 30)
'
test_expect_success 'MWOAF fails while the previous epilog is still running' '
	flux job wait-event --timeout=5s $JOBID_WITH_EPILOG epilog-start &&
	test_expect_code 16 flux ion-resource match without_allocating_future jobspec.json &&
	flux job wait-event --timeout=3s $JOBID_WITH_EPILOG epilog-finish
'
test_expect_success 'clean up' '
	flux cancel --all &&
	flux queue idle &&
	(flux resource undrain 0 || true)
'
test_expect_success 'remove fluxion modules' '
	remove_qmanager &&
	remove_resource
'

# Test for future match optimism
#
# On two nodes, if one has a currently overrunning job and the other has an
# active job, without_allocating_future should successfully match at the end of
# the allocated job's span, optimistically expecting that the overrunning job
# will have finished its epilog by then and that the allocated job will not
# overrun.
#
test_expect_success 'load test resources' '
    load_test_resources ${excl_4N4B}
'
test_expect_success 'load fluxion modules' '
    load_resource &&
    load_qmanager
'
test_expect_success 'on two nodes, submit one job that overruns and one that does not' '
    >jobspec.json flux run --dry-run -N2 -t1m -x sleep 60 &&
    flux ion-resource match allocate jobspec.json >present_match && #take two nodes out
    JOBID_ACTIVE=$(flux submit -N1 -t30s -x sleep 30) &&    # This job will have an active span
    JOBID_WITH_EPILOG=$(flux submit -N1 -t1s -x sleep 30)   # This job will overrun
'
test_expect_success 'MWOAF and orelse_reserve succeed in the future while an epilog runs' '
    flux job wait-event --timeout=1s $JOBID_WITH_EPILOG start &&
    flux job wait-event --timeout=1s $JOBID_ACTIVE start &&
    flux job wait-event --timeout=5s $JOBID_WITH_EPILOG epilog-start && # Wait for the overrun
    flux ion-resource match without_allocating_future jobspec.json >future_match &&
    flux ion-resource match allocate_orelse_reserve jobspec.json >future_reserve &&
    flux job wait-event --timeout=3s $JOBID_WITH_EPILOG epilog-finish
'
test_expect_success 'The future match and reservation start at the end of the "active" span' '
    present_match_time=$(grep "starttime" present_match | jq -e .execution.starttime) &&
    future_match_time=$(grep "starttime" future_match | jq -e .execution.starttime) &&
    future_reserve_time=$(grep "starttime" future_reserve | jq -e .execution.starttime) &&
    test $future_match_time -ge $(( $present_match_time + 30 )) &&
    test $future_match_time -le $(( $present_match_time + 35 )) &&
    test $future_reserve_time -ge $(( $present_match_time + 30 )) &&
    test $future_reserve_time -le $(( $present_match_time + 35 ))
'
test_expect_success 'clean up' '
    flux cancel --all &&
    flux queue idle &&
    (flux resource undrain 0 || true)
'
test_expect_success 'remove fluxion modules' '
    remove_qmanager &&
    remove_resource
'

test_done

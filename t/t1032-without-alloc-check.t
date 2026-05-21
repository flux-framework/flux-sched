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

export FLUX_SCHED_MODULE=none
test_under_flux 1

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
	JOBID_WITH_EPILOG=$(flux submit -N1 -x hostname)
'
test_expect_success 'MWOA fails while the previous epilog is still running' '
	>jobspec.json flux run --dry-run -N1 -x hostname &&
	flux job wait-event --timeout=5s $JOBID_WITH_EPILOG epilog-start &&
	test_expect_code 16 flux ion-resource match without_allocating jobspec.json &&
	flux job wait-event --timeout=3s $JOBID_WITH_EPILOG epilog-finish
'
test_expect_success 'submit another node-exclusive job that exceeds its time limit' '
	JOBID_WITH_EPILOG=$(flux submit -N1 -x hostname)
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

test_done

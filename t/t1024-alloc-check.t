test_description='Check that fluxion never double books resources'

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
	[job-manager]
	epilog.command = [ "flux", "perilog-run", "epilog", "-e", "sleep,2" ]
	EOT
	flux jobtap load perilog.so
'
# Jobs seem to need to be submitted separately to trigger the issue.
test_expect_success 'submit node-exclusive jobs that exceed their time limit' '
	(for i in $(seq 5); do \
	    flux run -N1 -x -t1s sleep 30 || true; \
	done) 2>joberr
'
test_expect_success 'some jobs received timeout exception' '
	grep "job.exception type=timeout" joberr
'
test_expect_success 'no jobs received alloc-check exception' '
	test_must_fail grep "job.exception type=alloc-check" joberr
'
test_expect_success 'clean up' '
	flux job cancelall -f &&
	flux queue idle &&
	(flux resource undrain 0 || true)
'
test_expect_success 'submit non-exclusive jobs that exceed their time limit' '
	(for i in $(seq 10); do \
	    flux run --ntasks=1 --cores-per-task=8 -t1s sleep 30 || true; \
	done) 2>joberr2
'
test_expect_success 'some jobs received timeout exception' '
	grep "job.exception type=timeout" joberr2
'
test_expect_success 'no jobs received alloc-check exception' '
	test_must_fail grep "job.exception type=alloc-check" joberr2
'
test_expect_success 'clean up' '
	cleanup_active_jobs
'
test_expect_success 'remove fluxion modules' '
	remove_qmanager &&
	remove_resource
'

test_done

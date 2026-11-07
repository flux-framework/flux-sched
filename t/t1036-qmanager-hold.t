#!/bin/sh

test_description='Test holding and releasing a pending job

A job submitted with attributes.system.hold stays pending under the
coschedule policy and is not allocated until the
sched-fluxion-qmanager.release RPC clears the hold. Here the release comes
from a direct RPC call, standing in for the agent that would send it once
whatever the job waits on is ready.
'

. `dirname $0`/sharness.sh

hwloc_basepath=`readlink -e ${SHARNESS_TEST_SRCDIR}/data/hwloc-data`
# 1 broker: 1 node, 2 sockets, 16 cores (8 per socket)
excl_1N1B="${hwloc_basepath}/001N/exclusive/01-brokers"

export FLUX_SCHED_MODULE=none
test_under_flux 1

# Release a held job. Args: <jobid>
qmanager_release() {
	flux python -c "
import flux, sys
h = flux.Flux()
h.rpc('sched-fluxion-qmanager.release',
      {'id': int(sys.argv[1])}).get()
" "$(flux job id --to=dec $1)"
}

test_expect_success 'load test resources' '
	load_test_resources ${excl_1N1B}
'

test_expect_success 'load fluxion with the coschedule policy' '
	load_resource prune-filters=ALL:core subsystems=containment policy=low &&
	load_qmanager_sync queue-policy=coschedule
'

test_expect_success 'a held job is accepted and stays pending (SCHED)' '
	jobid=$(flux submit --setattr=system.hold=1 -n1 sleep 300) &&
	echo $jobid >held.jobid &&
	test_must_fail flux job wait-event -t 3 $jobid alloc &&
	test "$(flux jobs -no {state} $jobid)" = "SCHED"
'

test_expect_success "the held job's resources are not consumed" '
	test $(flux job list --states=running | wc -l) -eq 0
'

test_expect_success "held job does not block the queue: a normal job runs" '
	flux run -n1 sleep 0
'

test_expect_success "releasing a job with a bad id fails" '
	test_must_fail flux python -c "
import flux
flux.Flux().rpc(\"sched-fluxion-qmanager.release\",
                {\"id\": 123456789012345}).get()
"
'

test_expect_success 'releasing the job allocates it on the next loop' '
	jobid=$(cat held.jobid) &&
	qmanager_release $jobid &&
	flux job wait-event -t 30 $jobid alloc &&
	test "$(flux jobs -no {state} $jobid)" != "SCHED"
'

test_expect_success 'a job held then released before matching also runs' '
	jobid=$(flux submit --setattr=system.hold=1 -n1 sleep 0) &&
	test_must_fail flux job wait-event -t 3 $jobid alloc &&
	qmanager_release $jobid &&
	flux job wait-event -t 30 $jobid clean
'

test_expect_success 'a held job can be canceled while held' '
	jobid=$(flux submit --setattr=system.hold=1 -n1 sleep 300) &&
	test_must_fail flux job wait-event -t 3 $jobid alloc &&
	flux cancel $jobid &&
	flux job wait-event -t 30 $jobid clean
'

test_expect_success 'clean up' '
	flux cancel --all &&
	flux queue idle
'

test_expect_success 'remove fluxion modules' '
	remove_qmanager &&
	remove_resource
'

test_done

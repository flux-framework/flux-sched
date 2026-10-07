#!/bin/sh
test_description='Test an exclusive ancestor that has no rank after the planned end

A job holds the cluster exclusively. The cluster has no broker rank. When
the job ends, housekeeping holds rank 0 and releases rank 1. The partial
release must give other jobs access to the released node. It must also keep
the cluster refused for an exclusive request while the job still holds a
node below the cluster. This must hold after the planned end of the job
too, because housekeeping continues after the walltime.

Before the fix, the partial release removed the allocation entry of the job
on the cluster. Only the x_checker span then refused an exclusive request,
and that span ends at the planned end of the job. After that time an
exclusive request for the cluster succeeded, and the new job received every
node of the cluster, also the node that housekeeping held. This is a double
booking.
'

. $(dirname $0)/sharness.sh

export TEST_UNDER_FLUX_CORES_PER_RANK=4
test_under_flux 2 job

TOTAL_NCORES=8
NCORES_PER_RANK=4

# The walltime of the exclusive cluster job in seconds. The test waits for
# this time to pass, so keep it short.
DURATION=10

# How long the task of the exclusive cluster job runs. The job must stay in
# RUN long enough for the test to read the state of the scheduler while the
# job holds every node. The task must also end well before the walltime.
RUNTIME=3

# How long to wait before a job counts as pending. Raise this if a loaded
# machine ever allocates too slowly to be seen here.
PENDING_TIMEOUT=${PENDING_TIMEOUT:-3}

# Usage dmesg_wait PATTERN
# Wait up to 10s for PATTERN to appear in dmesg output
#
dmesg_wait() {
	count=0
	while ! flux dmesg -H | grep "$1" >/dev/null 2>&1; do
		count=$((count+1))
		test $count -eq 100 && return 1 # max 100 * 0.1 sleep = 10s
		sleep 0.1
	done
}

# Usage: hk_wait_for_running count
hk_wait_for_running () {
	count=0
	while test $(flux housekeeping list -no {id} | wc -l) -ne $1; do
		count=$(($count+1));
		test $count -eq 300 && return 1 # max 300 * 0.1s sleep = 30s
		sleep 0.1
	done
}
# Usage: hk_wait_for_allocated_nnodes count
hk_wait_for_allocated_nnodes () {
	count=0
	while test "$(flux housekeeping list -no {allocated.nnodes})" != "$1"; do
		count=$(($count+1));
		test $count -eq 300 && return 1 # max 300 * 0.1s sleep = 30s
		sleep 0.1
	done
}
# Usage: fluxion_free ncores|nnodes
fluxion_free () {
	FLUX_RESOURCE_LIST_RPC=sched.resource-status \
		flux resource list -s free -no {$1}
}
# Usage: fluxion_allocated ncores|nnodes
fluxion_allocated () {
	FLUX_RESOURCE_LIST_RPC=sched.resource-status \
		flux resource list -s allocated -no {$1}
}
# Usage: fluxion_errors FILE
# Save the broker log in FILE. Fail if a fluxion module logged an error.
fluxion_errors () {
	flux logger test-sentinel &&
	dmesg_wait test-sentinel &&
	flux dmesg -H >$1 &&
	test_must_fail grep "sched-fluxion-[a-z]*\.err" $1
}

# Usage: reuse_released_cores ncores ranks
#
# Submit a job that only fits in the cores the partial release freed, and
# prove it really used them. A clean event alone proves nothing: a job that
# the scheduler denies, or that fails to start, also becomes clean. Require
# a start event, an exit status of zero, and the expected rank set.
reuse_released_cores () {
	id=$(flux submit -n$1 --requires=rank:$2 true) || return 1
	flux job wait-event -t 30 $id start &&
	flux job wait-event -t 30 $id finish &&
	flux job wait-event -t 30 $id clean &&
	test "$(flux jobs -no {returncode} $id)" = "0" &&
	test "$(flux jobs -no {ranks} $id)" = "$2" &&
	test "$(flux jobs -no {ncores} $id)" = "$1"
}

# Usage: submit_excl_cluster
#
# Submit the job that asks for the whole cluster exclusively. Print the id.
submit_excl_cluster () {
	flux job submit excl-cluster.json
}

# Usage: job_ranks JOBID
#
# Print the ranks of the job in idset form. The R of the job stays in the
# KVS after the job ends, so this works for an inactive job too.
job_ranks () {
	flux job info $1 R | flux R decode --ranks
}

# Usage: held_ranks JOBID
#
# Print the ranks that the scheduler still holds for the job, in idset
# form. This asks the resource module directly, so it also reports the
# state while housekeeping runs, when the job itself is inactive.
held_ranks () {
	flux ion-resource find jobid-alloc=$(flux job id --to=dec $1) \
		| grep '"version"' | flux R decode --ranks
}

# Usage: job_expiration JOBID
# Print the planned end of the job as a whole number of seconds.
job_expiration () {
	flux job info $1 R | jq -r .execution.expiration | awk '{printf "%d", $1}'
}

# Usage: wait_past_expiration JOBID
#
# Wait until the planned end of the job passes. Add two seconds of margin,
# so that the clock of the test is certainly beyond the planned end. If the
# time has already passed, do not wait.
wait_past_expiration () {
	expiration=$(job_expiration $1) &&
	target=$(($expiration + 2)) &&
	now=$(date +%s) &&
	if test $now -lt $target; then
		sleep $(($target - $now))
	fi &&
	test $(date +%s) -gt $expiration
}

# Usage: excl_cluster_stays_pending
#
# Submit the exclusive cluster job while housekeeping still holds rank 0.
# The scheduler must not allocate it. An alloc event is the bug. The
# absence of an alloc event is not enough on its own: a job that the
# scheduler denies, or a job that takes any other exception, has no alloc
# event either, and would pass while it proves nothing. Thus the job must
# also still be in SCHED, which is only possible while it waits for
# resources. Read the state before the cancel, because a cancel moves the
# job to INACTIVE.
excl_cluster_stays_pending () {
	id=$(submit_excl_cluster) || return 1
	test_must_fail flux job wait-event -t $PENDING_TIMEOUT $id alloc
	rc=$?
	state=$(flux jobs -no {state} $id)
	if test $rc -eq 0 && test "$state" != "SCHED"; then
		echo "job $id is $state, not SCHED" >&2
		rc=1
	fi
	if test $rc -ne 0; then
		echo "job $id got these ranks: $(job_ranks $id)" >&2
	fi
	flux cancel $id >/dev/null 2>&1
	flux job wait-event -t 30 $id clean >/dev/null 2>&1
	return $rc
}

test_expect_success 'load fluxion modules with a policy that shares nodes' '
	flux module remove -f sched-simple &&
	load_resource match-format=rv1_nosched policy=first &&
	load_qmanager_sync &&
	load_feasibility &&
	flux resource list &&
	FLUX_RESOURCE_LIST_RPC=sched.resource-status flux resource list
'

# Check job manager hello debug message for +partial-ok flag
if flux dmesg | grep +partial-ok; then
    test_set_prereq HAVE_PARTIAL_OK
fi

# The jobspec of this test asks for an exclusive cluster. The jobspec
# validator of flux-core only accepts the canonical form of RFC 14 that
# starts at a node, so disable the validator. Housekeeping must hold rank 0
# after each job, because the test needs a partial release. flux config
# load replaces the whole configuration, so set both tables at one time.
test_expect_success 'disable the jobspec validator and configure housekeeping' '
	flux config load <<-EOF &&
	[ingest.validator]
	disable = true

	[job-manager.housekeeping]
	command = [
	    "sh",
	    "-c",
	    "test \$(flux getattr rank) -eq 0 && sleep inf; exit 0"
	]
	release-after = "0s"
	EOF
	flux module reload job-ingest
'

test_expect_success 'write a jobspec that asks for an exclusive cluster' '
	cat >excl-cluster.json <<-EOF &&
	{
	  "version": 9999,
	  "resources": [
	    {"type": "cluster", "count": 1, "exclusive": true,
	     "with": [
	       {"type": "node", "count": 1,
	        "with": [
	          {"type": "slot", "count": 1, "label": "task",
	           "with": [{"type": "core", "count": 1}]}
	        ]}
	     ]}
	  ],
	  "attributes": {"system": {"duration": $DURATION}},
	  "tasks": [{"command": ["sleep", "$RUNTIME"], "slot": "task",
	             "count": {"per_slot": 1}}]
	}
	EOF
	jq -e .version excl-cluster.json
'

test_expect_success HAVE_PARTIAL_OK 'the exclusive cluster job takes every node' '
	flux dmesg -C &&
	id=$(submit_excl_cluster) &&
	echo $id >job1.id &&
	flux job wait-event -t 30 $id start &&
	test "$(job_ranks $id)" = "0-1" &&
	test "$(held_ranks $id)" = "0-1" &&
	test $(fluxion_allocated ncores) -eq $TOTAL_NCORES
'

test_expect_success HAVE_PARTIAL_OK 'the planned end is the start plus the duration' '
	id=$(cat job1.id) &&
	start=$(flux job info $id R | jq -r .execution.starttime) &&
	end=$(flux job info $id R | jq -r .execution.expiration) &&
	test $(awk -v a=$end -v b=$start "BEGIN{printf \"%d\", a - b}") -eq $DURATION
'

test_expect_success HAVE_PARTIAL_OK 'the job ends, and housekeeping holds rank 0 only' '
	id=$(cat job1.id) &&
	flux job wait-event -t 30 $id clean &&
	test "$(flux jobs -no {returncode} $id)" = "0" &&
	hk_wait_for_running 1 &&
	hk_wait_for_allocated_nnodes 1
'

# The job keeps an allocation entry on the cluster, but the entry refers
# to a span that holds no resources. The allocated count must not include
# the vertices below the cluster because of that entry.
test_expect_success HAVE_PARTIAL_OK 'the scheduler released rank 1 of the job' '
	test "$(held_ranks $(cat job1.id))" = "0" &&
	test $(fluxion_allocated ncores) -eq $NCORES_PER_RANK
'

test_expect_success HAVE_PARTIAL_OK 'a normal job uses the released node' '
	reuse_released_cores $NCORES_PER_RANK 1 &&
	hk_wait_for_running 1
'

test_expect_success HAVE_PARTIAL_OK 'wait until the planned end of the job passes' '
	wait_past_expiration $(cat job1.id) &&
	hk_wait_for_running 1 &&
	test "$(held_ranks $(cat job1.id))" = "0"
'

# Ask the resource module directly first. This probe changes no state, so
# it shows the behaviour of the traverser alone. EBUSY (16) is the expected
# error: the request is satisfiable, but the cluster is not available now.
test_expect_success HAVE_PARTIAL_OK 'a match without allocating fails after the planned end' '
	test_expect_code 16 \
		flux ion-resource match without_allocating excl-cluster.json &&
	hk_wait_for_running 1 &&
	test "$(held_ranks $(cat job1.id))" = "0"
'

test_expect_success HAVE_PARTIAL_OK 'the request is still satisfiable' '
	flux ion-resource match satisfiability excl-cluster.json
'

test_expect_success HAVE_PARTIAL_OK 'a second exclusive cluster job stays pending' '
	excl_cluster_stays_pending &&
	hk_wait_for_running 1 &&
	test "$(held_ranks $(cat job1.id))" = "0"
'

test_expect_success HAVE_PARTIAL_OK 'the released node is still usable' '
	reuse_released_cores $NCORES_PER_RANK 1 &&
	hk_wait_for_running 1
'

test_expect_success HAVE_PARTIAL_OK 'the end of housekeeping frees rank 0' '
	flux housekeeping kill --all &&
	hk_wait_for_running 0 &&
	test $(fluxion_free ncores) -eq $TOTAL_NCORES &&
	test $(fluxion_allocated ncores) -eq 0
'

test_expect_success HAVE_PARTIAL_OK 'the exclusive cluster job runs again on both nodes' '
	id=$(submit_excl_cluster) &&
	flux job wait-event -t 30 $id start &&
	test "$(job_ranks $id)" = "0-1" &&
	test "$(held_ranks $id)" = "0-1" &&
	test $(fluxion_allocated ncores) -eq $TOTAL_NCORES &&
	flux job wait-event -t 30 $id clean &&
	test "$(flux jobs -no {returncode} $id)" = "0" &&
	flux housekeeping kill --all &&
	hk_wait_for_running 0 &&
	test $(fluxion_free ncores) -eq $TOTAL_NCORES
'

test_expect_success HAVE_PARTIAL_OK 'fluxion logged no error' '
	fluxion_errors log.overrun.out
'

test_expect_success 'unload fluxion modules' '
	remove_feasibility &&
	remove_qmanager &&
	remove_resource &&
	flux module load sched-simple
'
test_done

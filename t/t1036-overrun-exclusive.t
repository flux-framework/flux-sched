#!/bin/sh
test_description='Test exclusive requests over resources that a job holds past its planned end

A job that runs past its planned end, for example in housekeeping, still
holds its resources. An exclusive request that includes those resources
must stay refused until the job releases them, while the other resources
stay usable.

The first case is a shared node. A job holds one core on each of two
nodes without exclusivity. When the job ends, housekeeping holds its
core of rank 0 past the planned end. Before the fix, the node had no
allocation entry of the job, so an exclusive request for the node passed
the checks of the match, and the update then allocated every core of
the node to the new job, also the held core. This is a double booking.
The case runs once for each match format, with a reload of the modules
while housekeeping holds the core, so each reader must rebuild the held
core in a way that keeps the refusal.
'

. $(dirname $0)/sharness.sh

export TEST_UNDER_FLUX_CORES_PER_RANK=4
test_under_flux 2 job

TOTAL_NCORES=8
NCORES_PER_RANK=4

# The walltime of the shared job in seconds. The test waits for this
# time to pass, so keep it short.
DURATION=10

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

# Usage: stays_pending JOBSPEC
#
# Submit the job in JOBSPEC while housekeeping still holds a resource that
# the job needs. The scheduler must not allocate it. An alloc event is the
# bug. The absence of an alloc event is not enough on its own: a job that
# the scheduler denies, or a job that takes any other exception, has no
# alloc event either, and would pass while it proves nothing. Thus the job
# must also still be in SCHED, which is only possible while it waits for
# resources. Read the state before the cancel, because a cancel moves the
# job to INACTIVE.
stays_pending () {
	id=$(flux job submit $1) || return 1
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

# The job manager logs the +partial-ok flag when the scheduler says hello.
# Every supported flux-core offers partial release, so a missing flag is a
# fault of the test environment and not a reason to skip. The whole test
# depends on partial release, so check the flag here and fail if it is
# absent. Do this check before the first flux dmesg -C, because a clear
# removes the hello line from the log.
test_expect_success 'the job manager offers partial release' '
	flux dmesg | grep -q "+partial-ok"
'

# Housekeeping must hold rank 0 after each job and release the other
# ranks, because the test needs a partial release.
test_expect_success 'configure housekeeping to hold rank 0' '
	flux config load <<-EOF
	[job-manager.housekeeping]
	command = [
	    "sh",
	    "-c",
	    "test \$(flux getattr rank) -eq 0 && sleep inf; exit 0"
	]
	release-after = "0s"
	EOF
'

# Usage: shared_node_overrun LABEL
#
# The caller sets up the modules, so the section runs for each reader that
# rebuilds the state on a reload.
shared_node_overrun () {
	label=$1
	# The log of this run. Each run starts with a clear of the broker log,
	# so each run needs a file of its own.
	errlog=log.$1.out
	# The two-node job runs until the test cancels it. Give it a long
	# walltime, so that a slow check cannot race the end of the job.
	test_expect_success "$label: write the exclusive node jobspecs" '
		flux submit --dry-run -N2 --exclusive -t 5m sleep inf \
			>excl-2nodes.json &&
		flux submit --dry-run -N1 --exclusive --requires=rank:0 -t 10s \
			true >excl-node0.json &&
		jq -e ".resources[0].exclusive" excl-2nodes.json &&
		jq -e ".resources[0].exclusive" excl-node0.json
	'

	test_expect_success "$label: a shared job ends, and housekeeping holds a core of rank 0" '
		flux dmesg -C &&
		id=$(flux submit -N2 -n2 -c1 -t ${DURATION}s true) &&
		echo $id >shared.id &&
		flux job wait-event -t 30 $id start &&
		test "$(job_ranks $id)" = "0-1" &&
		flux job wait-event -t 30 $id clean &&
		test "$(flux jobs -no {returncode} $id)" = "0" &&
		hk_wait_for_running 1 &&
		hk_wait_for_allocated_nnodes 1 &&
		test "$(held_ranks $id)" = "0" &&
		test $(fluxion_allocated ncores) -eq 1
	'

	test_expect_success "$label: the released node is usable" '
		reuse_released_cores $NCORES_PER_RANK 1 &&
		hk_wait_for_running 1
	'

	test_expect_success "$label: reload the modules while housekeeping holds the core" '
		remove_feasibility &&
		remove_qmanager &&
		reload_resource match-format=$fmt policy=first &&
		load_qmanager_sync &&
		load_feasibility &&
		hk_wait_for_running 1 &&
		test "$(held_ranks $(cat shared.id))" = "0" &&
		test $(fluxion_allocated ncores) -eq 1
	'

	test_expect_success "$label: wait until the planned end of the shared job passes" '
		wait_past_expiration $(cat shared.id) &&
		hk_wait_for_running 1 &&
		test "$(held_ranks $(cat shared.id))" = "0"
	'

	test_expect_success "$label: an exclusive match for the held node fails after the planned end" '
		test_expect_code 16 \
			flux ion-resource match without_allocating excl-2nodes.json &&
		test_expect_code 16 \
			flux ion-resource match without_allocating excl-node0.json &&
		flux ion-resource match satisfiability excl-2nodes.json &&
		flux ion-resource match satisfiability excl-node0.json &&
		test "$(held_ranks $(cat shared.id))" = "0" &&
		test $(fluxion_allocated ncores) -eq 1
	'

	test_expect_success "$label: an exclusive job that needs the held node stays pending" '
		stays_pending excl-2nodes.json &&
		stays_pending excl-node0.json &&
		hk_wait_for_running 1 &&
		test "$(held_ranks $(cat shared.id))" = "0" &&
		test $(fluxion_allocated ncores) -eq 1
	'

	test_expect_success "$label: an exclusive job gets the other node" '
		id=$(flux submit -N1 --exclusive -t 10s true) &&
		flux job wait-event -t 30 $id start &&
		test "$(job_ranks $id)" = "1" &&
		flux job wait-event -t 30 $id clean &&
		test "$(flux jobs -no {returncode} $id)" = "0" &&
		hk_wait_for_running 1 &&
		test "$(held_ranks $(cat shared.id))" = "0"
	'

	# Each job on rank 0 leaves a housekeeping task of its own behind,
	# so the count of tasks grows by one here.
	test_expect_success "$label: the free cores of the held node stay usable" '
		reuse_released_cores $(($NCORES_PER_RANK - 1)) 0 &&
		hk_wait_for_running 2 &&
		test "$(held_ranks $(cat shared.id))" = "0"
	'

	# Keep this job across the release of the held core, so that the next
	# step sees this very job run. The step that follows cancels it, and
	# also removes it if a part of that step fails. A job left in SCHED
	# here would take both nodes in the next run of this section.
	test_expect_success "$label: an exclusive two-node job waits for the held core" '
		id=$(flux job submit excl-2nodes.json) &&
		echo $id >pending.id &&
		test_must_fail flux job wait-event -t $PENDING_TIMEOUT $id alloc &&
		test "$(flux jobs -no {state} $id)" = "SCHED"
	'

	test_expect_success "$label: the end of housekeeping frees the held core" '
		test_when_finished "flux cancel \$(cat pending.id) >/dev/null 2>&1; true" &&
		flux housekeeping kill --all &&
		id=$(cat pending.id) &&
		flux job wait-event -t 30 $id alloc &&
		flux job wait-event -t 30 $id start &&
		test "$(job_ranks $id)" = "0-1" &&
		test $(fluxion_allocated ncores) -eq $TOTAL_NCORES &&
		flux cancel $id &&
		flux job wait-event -t 30 $id clean &&
		hk_wait_for_running 1 &&
		flux housekeeping kill --all &&
		hk_wait_for_running 0 &&
		test $(fluxion_free ncores) -eq $TOTAL_NCORES &&
		test $(fluxion_allocated ncores) -eq 0
	'

	test_expect_success "$label: fluxion logged no error" '
		fluxion_errors $errlog
	'
}

fmt=rv1_nosched
shared_node_overrun "$fmt"

for fmt in rv1 rv1_shorthand; do
	test_expect_success "reload fluxion modules with match-format=$fmt" '
		remove_feasibility &&
		remove_qmanager &&
		reload_resource match-format=$fmt policy=first &&
		load_qmanager_sync &&
		load_feasibility
	'
	shared_node_overrun "$fmt"
done

test_expect_success 'unload fluxion modules' '
	remove_feasibility &&
	remove_qmanager &&
	remove_resource &&
	flux module load sched-simple
'
test_done

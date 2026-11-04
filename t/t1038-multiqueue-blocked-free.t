#!/bin/sh
test_description='Test that the end of a job unblocks jobs of every queue

A backfill policy puts a job in the blocked list of its queue when the
scheduler can neither allocate nor reserve it. A job that holds every node
and has no time limit causes this, because nothing can be reserved behind
it. When that job ends, every blocked job of every queue must return to the
pending list, so the jobs that it blocked start.

Before the fix, a .free RPC reconsidered the blocked jobs of the queue of
the freed job only. A job of queue batch that held the whole cluster
therefore left the jobs of queue debug blocked after it ended. Those jobs
stayed in SCHED on an idle cluster until an unrelated event of queue debug,
or a change of resource status, reconsidered them.

The test proves the fix for a final free, for a free of a queue whose
policy differs from the policy of the blocked queue, and for the partial
free that housekeeping sends when it releases one node and holds another.
'

. $(dirname $0)/sharness.sh

export TEST_UNDER_FLUX_CORES_PER_RANK=2
test_under_flux 2 job

TOTAL_NCORES=4

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

# Usage: wait_free_cores count
#
# Wait until fluxion reports count free cores. A free is asynchronous, so
# poll the count instead of reading it one time.
wait_free_cores () {
	count=0
	while test "$(fluxion_free ncores)" != "$1"; do
		count=$(($count+1))
		test $count -eq 300 && return 1 # max 300 * 0.1s sleep = 30s
		sleep 0.1
	done
}

# Usage: blocked_ids QUEUE
#
# Print the f58plain id of every blocked job of QUEUE, one per line. A job
# is in this list only while a backfill policy holds it out of the pending
# list, so the list is the direct report of the defect.
blocked_ids () {
	flux module stats sched-fluxion-qmanager |
		jq -r ".queues.$1.pending_queues.blocked[]"
}

# Usage: wait_blocked QUEUE count
wait_blocked () {
	count=0
	while test $(blocked_ids $1 | wc -l) -ne $2; do
		count=$(($count+1))
		test $count -eq 300 && return 1 # max 300 * 0.1s sleep = 30s
		sleep 0.1
	done
}

# Usage: is_blocked QUEUE JOBID
is_blocked () {
	blocked_ids $1 | grep -qx $(flux job id --to=f58plain $2)
}

# Usage: base_config POLICY
#
# Write the configuration to stdout. POLICY is the policy line of the
# [sched-fluxion-qmanager] table.
base_config () {
	cat <<-EOF
	[queues.batch]
	[queues.debug]

	[policy.jobspec.defaults.system]
	queue = "batch"

	[sched-fluxion-qmanager]
	$1

	[sched-fluxion-resource]
	match-policy = "first"
	match-format = "rv1_nosched"
	EOF
}

# Usage: load_config POLICY
#
# flux config load replaces the whole configuration, so every call must
# write every key that the test needs.
load_config () {
	base_config "$1" | flux config load
}

# Usage: load_config_hk POLICY
#
# The same configuration, plus a housekeeping command that holds rank 0 and
# releases rank 1 at once. The release of rank 1 is a partial free, which
# must unblock jobs of other queues like a final free does.
load_config_hk () {
	{
		base_config "$1"
		cat <<-EOF
		[job-manager.housekeeping]
		command = [
		    "sh",
		    "-c",
		    "test \$(flux getattr rank) -eq 0 && sleep inf; exit 0"
		]
		release-after = "0s"
		EOF
	} | flux config load
}

# Usage: blocked_free_test LABEL
#
# A batch job holds every node, two debug jobs become blocked, and the end
# of the batch job must start them. LABEL is only a name for the output.
blocked_free_test () {
	label=$1
	test_expect_success "$label: clear the log" '
		flux dmesg -C
	'
	# The job has no time limit, so it holds every node to the end of
	# the time horizon of the scheduler. No later job can be reserved
	# behind it, which is what sends a later job to the blocked list.
	test_expect_success "$label: a batch job with no time limit holds every node" '
		A=$(flux submit --queue=batch -N2 -n4 sleep inf) &&
		flux job wait-event -t 30 $A start &&
		echo $A >A.id &&
		test $(fluxion_allocated ncores) -eq $TOTAL_NCORES
	'
	test_expect_success "$label: two debug jobs become blocked" '
		B1=$(flux submit --queue=debug -n1 sleep inf) &&
		B2=$(flux submit --queue=debug -n1 sleep inf) &&
		echo $B1 >B1.id &&
		echo $B2 >B2.id &&
		wait_blocked debug 2 &&
		is_blocked debug $B1 &&
		is_blocked debug $B2
	'
	# Nothing else may happen between the cancel and the waits. A new
	# submission, a drain, or a change of urgency in queue debug also
	# reconsiders the blocked jobs of that queue, and would hide the
	# defect. The free of the batch job alone must start them.
	test_expect_success "$label: the end of the batch job starts the debug jobs" '
		flux cancel $(cat A.id) &&
		flux job wait-event -t 30 $(cat A.id) clean &&
		flux job wait-event -t 10 $(cat B1.id) start &&
		flux job wait-event -t 10 $(cat B2.id) start
	'
	test_expect_success "$label: no debug job stays blocked" '
		wait_blocked debug 0
	'
	test_expect_success "$label: fluxion logged no error" '
		fluxion_errors log.$label.out
	'
	test_expect_success "$label: the debug jobs end" '
		flux cancel $(cat B1.id) $(cat B2.id) &&
		flux job wait-event -t 30 $(cat B1.id) clean &&
		flux job wait-event -t 30 $(cat B2.id) clean &&
		wait_free_cores $TOTAL_NCORES
	'
}

# Usage: partial_free_test LABEL
#
# The same defect, driven by a partial free. Housekeeping releases rank 1
# and holds rank 0, so the job manager sends a non-final free. That free
# must also unblock the job of the other queue.
partial_free_test () {
	label=$1
	test_expect_success "$label: clear the log" '
		flux dmesg -C
	'
	test_expect_success "$label: a batch job with no time limit holds every node" '
		A=$(flux submit --queue=batch -N2 -n4 sleep inf) &&
		flux job wait-event -t 30 $A start &&
		echo $A >A.id &&
		test $(fluxion_allocated ncores) -eq $TOTAL_NCORES
	'
	test_expect_success "$label: a debug job becomes blocked" '
		B=$(flux submit --queue=debug -n1 sleep inf) &&
		echo $B >B.id &&
		wait_blocked debug 1 &&
		is_blocked debug $B
	'
	test_expect_success "$label: housekeeping releases rank 1 and holds rank 0" '
		flux cancel $(cat A.id) &&
		flux job wait-event -t 30 $(cat A.id) clean &&
		hk_wait_for_running 1 &&
		hk_wait_for_allocated_nnodes 1
	'
	# The job starts on the released rank while housekeeping still holds
	# rank 0. Only the partial free can have woken it: the batch job is
	# not free yet, and nothing else touched queue debug.
	test_expect_success "$label: the debug job starts on the released rank" '
		flux job wait-event -t 10 $(cat B.id) start &&
		test "$(flux jobs -no {ranks} $(cat B.id))" = "1" &&
		hk_wait_for_running 1
	'
	test_expect_success "$label: no debug job stays blocked" '
		wait_blocked debug 0
	'
	test_expect_success "$label: fluxion logged no error" '
		fluxion_errors log.$label.out
	'
	test_expect_success "$label: the debug job and housekeeping end" '
		flux cancel $(cat B.id) &&
		flux job wait-event -t 30 $(cat B.id) clean &&
		flux housekeeping kill --all &&
		hk_wait_for_running 0 &&
		wait_free_cores $TOTAL_NCORES
	'
}

test_expect_success 'configure two queues with the easy policy' '
	load_config "queue-policy = \"easy\"" &&
	flux queue start --all
'

test_expect_success 'load fluxion modules' '
	flux module remove -f sched-simple &&
	load_resource &&
	load_qmanager_sync &&
	flux resource list &&
	FLUX_RESOURCE_LIST_RPC=sched.resource-status flux resource list
'

# The job manager logs the +partial-ok flag when the scheduler says hello.
# Every supported flux-core offers partial release, so a missing flag is a
# fault of the test environment and not a reason to skip. The partial pass
# below depends on partial release, so check the flag here and fail if it
# is absent. Do this check before the first flux dmesg -C, because a clear
# removes the hello line from the log.
test_expect_success 'the job manager offers partial release' '
	flux dmesg | grep -q "+partial-ok"
'

blocked_free_test easy

# The queue that frees and the queue of the blocked jobs need not share a
# policy. Queue batch uses fcfs, which never blocks a job of its own, and
# queue debug uses conservative, which reserves every pending job.
test_expect_success 'configure batch with fcfs and debug with conservative' '
	load_config "queue-policy-per-queue = \"batch:fcfs debug:conservative\"" &&
	flux queue start --all &&
	reload_qmanager_sync
'

blocked_free_test fcfs-conservative

# This pass must come last, because the housekeeping command holds rank 0
# until the test kills it.
test_expect_success 'configure housekeeping that holds rank 0' '
	load_config_hk "queue-policy = \"easy\"" &&
	flux queue start --all &&
	reload_qmanager_sync
'

partial_free_test partial

# The sharness cleanup checks the list of loaded modules, so put the
# scheduler of flux-core back.
test_expect_success 'unload fluxion modules' '
	remove_qmanager &&
	remove_resource &&
	flux module load sched-simple
'
test_done

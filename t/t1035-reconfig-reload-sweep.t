#!/bin/sh

test_description='Resources held across a reconfigure are not double allocated

Regression test for flux-core#7794. A queue configuration change followed
by a reload of the resource and fluxion modules must not leave the
scheduler believing that resources held by a running job or by an active
housekeeping task are free. The reload sequence mirrors the site
reconfigure script in use when the issue was first seen: stop all started
queues with --nocheckpoint, unload feasibility/qmanager/fluxion-resource
and resource, reload configuration, then load the modules back in reverse
order and restart the queues.

The resource set is one node per rank with per-rank properties, so queues
can select node subsets:

    pall     ranks 0-3    overlaps everything, left stopped
    pbatch   ranks 0-3    started, runs the jobs that hold resources
    pdat     ranks 1-2    added and removed by the reconfigure under test

Rank 1 is always held when the reconfigure happens, and is in both pbatch
and pdat, so the held node is one whose vertex gains and loses a property
across the reload.

Rank 1 rather than rank 0 also matters for reproducing the bug. The match
writer produced the JGF when the job was allocated. The update traversal
visits the cluster vertex out-edges in insertion order, and the resource
module inserted them in R_lite rank order when it built the graph, so
graph.nodes lists the nodes in ascending rank. Holding rank 1 and freeing
rank 0 therefore places the held node at a larger index than a freed node,
and the old code left the held node unallocated. Holding rank 0 would put
the held node at index 0, so the old code allocated it before exiting and
the bug would not reproduce. If the traversal ever emits vertices in a
different order, the rank to hold changes with it.

The housekeeping-partial and housekeeping-whole scenarios differ only in
whether partial release is involved: the first strands rank 1 out of a
four-node allocation, the second runs a single-node job on rank 1 so the
whole allocation passes to housekeeping. Comparing them separates "the
allocation is in housekeeping" from "the allocation is partially
released".
'

. `dirname $0`/sharness.sh

if test_have_prereq ASAN; then
    skip_all='skipping reconfigure/reload tests under AddressSanitizer'
    test_done
fi

SIZE=4
LAST_RANK=$(($SIZE-1))

# Match formats to exercise. Override to narrow or extend, e.g.
#   MATCH_FORMATS=rv1_shorthand make check TESTS=t1035-reconfig-reload-sweep.t
MATCH_FORMATS=${MATCH_FORMATS:-"rv1 rv1_shorthand rv1_nosched"}

# What is holding resources when the reconfigure happens.
SCENARIOS=${SCENARIOS:-"housekeeping-partial housekeeping-whole running both"}

# How long to wait before concluding a job is not going to be allocated.
# Raise this if a loaded machine ever allocates too slowly to be seen here.
PENDING_TIMEOUT=${PENDING_TIMEOUT:-3}

mkdir -p conf.d

# Two resource sets, identical apart from the pdat property. JGF is
# appended so fluxion has a graph to reconstruct allocations against.
# noverify allows the fake per-rank hostnames.
encode_R () {
	flux R encode --hosts="test[0-$LAST_RANK]" --cores=0-3 \
	    --property=pall:0-$LAST_RANK \
	    --property=pbatch:0-$LAST_RANK \
	    "$@" | flux ion-R encode
}

# Usage: write_sched_conf match-format
#
# Only match-format varies. The other settings are the ones in use on the
# system where the issue was seen and are held fixed.
write_sched_conf () {
	cat >conf.d/sched.toml <<-EOT
	[sched-fluxion-resource]
	match-policy = "firstnodex"
	prune-filters = "ALL:core"
	match-format = "$1"

	[sched-fluxion-qmanager]
	queue-policy = "hybrid"
	EOT
}

# Usage: write_queue_conf [pdat]
write_queue_conf () {
	{
		echo "[policy]"
		echo "jobspec.defaults.system.queue = \"pbatch\""
		echo ""
		echo "[queues.pall]"
		echo "requires = [ \"pall\" ]"
		echo ""
		echo "[queues.pbatch]"
		echo "requires = [ \"pbatch\" ]"
		if test "$1" = "pdat"; then
			echo ""
			echo "[queues.pdat]"
			echo "requires = [ \"pdat\" ]"
		fi
	} >conf.d/queues.toml
}

# Number of nodes the scheduler believes are allocated, as opposed to the
# number the resource module believes are allocated. The difference is the
# whole point of the test.
#
# Ask for the allocated set rather than the free one. "free" is computed
# client-side as up minus allocated at node granularity, so a node with
# any core still held counts as free; "allocated" comes straight from the
# scheduler. The two agree under a node-exclusive match-policy, which is
# what this test configures, but the allocated set is what is actually
# being asserted.
sched_alloc_nnodes () {
	FLUX_RESOURCE_LIST_RPC=sched.resource-status \
	    flux resource list -s allocated -no {nnodes}
}

hk_nnodes () {
	flux housekeeping list -no {nnodes} | awk "{s+=\$1} END {print s+0}"
}

hk_count () {
	flux housekeeping list -no {id} | wc -l
}

active_jobs () {
	flux jobs --filter=pending,running -no {id} | wc -l
}

# Killing or failing a housekeeping task makes it exit non-zero, and the
# job-manager drains nodes whose housekeeping failed. A drained node is
# not schedulable, so it would make a pending job look like correct
# behavior when it is not. Every scenario must therefore start with
# nothing drained.
# Usage: assert_queues [pdat]
#
# The queue set and each queue's scheduling state must survive every
# reconfigure: pall stopped, pbatch started, pdat present only when it is
# configured. A queue that comes back stopped would make every pending-job
# check pass for the wrong reason.
assert_queues () {
	flux queue status >queue-status.out &&
	grep -q "^pall: Scheduling is stopped" queue-status.out &&
	grep -q "^pbatch: Scheduling is started" queue-status.out &&
	if test "$1" = "pdat"; then
		grep -q "^pdat: Scheduling is started" queue-status.out
	else
		test_must_fail grep -q "^pdat:" queue-status.out
	fi
}

assert_modules () {
	flux module list >modules.out &&
	grep -q resource modules.out &&
	grep -q sched-fluxion-resource modules.out &&
	grep -q sched-fluxion-qmanager modules.out &&
	grep -q sched-fluxion-feasibility modules.out
}

# Usage: assert_properties [pdat]
#
# Verify the regenerated R actually took effect. If the reloaded resource
# module prefers resource.R in the KVS over the configured path, the
# property change is silently ignored and the reconfigure under test is a
# no-op.
#
# This checks the queue column rather than the properties column: queues
# are derived from the properties in R, and flux resource list omits a
# property from the properties column when it is also a queue name.
assert_properties () {
	flux resource list -no "{queue}" >rlist-queue.out &&
	if test "$1" = "pdat"; then
		grep -q pdat rlist-queue.out
	else
		test_must_fail grep -q pdat rlist-queue.out
	fi
}

drained_ranks () {
	flux resource status -s drain -no {ranks}
}

undrain_all () {
	ranks=$(drained_ranks)
	test -z "$ranks" || flux resource undrain $ranks
}

# Usage: wait_for_hk_nnodes N
wait_for_hk_nnodes () {
	i=0
	while test $(hk_nnodes) -ne $1; do
		i=$(($i+1))
		test $i -eq 300 && return 1 # 300 * 0.1s = 30s
		sleep 0.1
	done
}

state_is_clean () {
	test $(hk_count) -eq 0 &&
	test $(active_jobs) -eq 0 &&
	test -z "$(drained_ranks)" &&
	test $(sched_alloc_nnodes) -eq 0
}

# Return to an idle instance. Cancelling a running job starts its
# housekeeping task, which sleeps on rank 1, so keep killing housekeeping
# and undraining until everything has actually settled. Undraining inside
# the loop is safe because nothing re-drains once housekeeping is gone.
cleanup_state () {
	flux cancel --all --quiet >/dev/null 2>&1
	i=0
	while ! state_is_clean; do
		flux housekeeping kill --all >/dev/null 2>&1
		undrain_all >/dev/null 2>&1
		i=$(($i+1))
		test $i -eq 300 && return 1
		sleep 0.1
	done
	rm -f held hk-hold
}

# The site reconfigure sequence. The final queue start is best effort
# because the queue list is gathered before the configuration is reloaded,
# so a queue removed by this reconfigure is still in the list. That is
# site behavior and is deliberately preserved.
reconfig () {
	queues=$(flux queue status \
	    | sed -n "s/^\(.*\): Scheduling is started.*/\1/p") &&
	for q in $queues; do
		flux queue stop --nocheckpoint --quiet $q || return 1
	done &&
	flux module remove sched-fluxion-feasibility &&
	flux module remove sched-fluxion-qmanager &&
	flux module remove sched-fluxion-resource &&
	flux module remove resource &&
	flux config reload &&
	flux module load resource &&
	flux module load sched-fluxion-resource &&
	flux module load sched-fluxion-qmanager &&
	flux module load sched-fluxion-feasibility &&
	for q in $queues; do
		flux queue start --quiet --queue=$q ||
		    test_debug "echo queue $q no longer configured"
	done
}

# Usage: setup_state housekeeping|running|both
#
# Leave resources held, and record in the file "held" how many nodes must
# remain unavailable to the scheduler. Rank 1 is held in every case.
setup_state () {
	case $1 in
	    housekeeping-partial)
		# release-after=0 frees every rank but rank 1, whose
		# housekeeping task sleeps while hk-hold exists. Remove
		# the sentinel once it is stranded so that nothing else
		# strands a node later in the scenario. The allocation
		# the job-manager still holds is a partial one.
		touch hk-hold &&
		flux run --queue=pbatch -N$SIZE -n$SIZE true &&
		wait_for_hk_nnodes 1 &&
		rm -f hk-hold &&
		echo 1 >held
		;;
	    housekeeping-whole)
		# A single-node job on rank 1: the whole allocation passes
		# to housekeeping with no partial release involved.
		touch hk-hold &&
		flux run --queue=pbatch -N1 --requires=rank:1 true &&
		wait_for_hk_nnodes 1 &&
		rm -f hk-hold &&
		echo 1 >held
		;;
	    running)
		flux submit --queue=pbatch -N2 --wait-event=start \
		    sleep inf >/dev/null &&
		echo 2 >held
		;;
	    both)
		touch hk-hold &&
		flux run --queue=pbatch -N$SIZE -n$SIZE true &&
		wait_for_hk_nnodes 1 &&
		rm -f hk-hold &&
		flux submit --queue=pbatch -N1 --wait-event=start \
		    sleep inf >/dev/null &&
		echo 2 >held
		;;
	    *)
		return 1
		;;
	esac
}

expected_free () {
	echo $(($SIZE - $(cat held)))
}

# Usage: stays_pending queue nnodes
#
# Submit a job that can only run on nodes that are already held. An alloc
# event is the bug. The job is cancelled either way: if it was allocated,
# leaving it running invalidates every later test in the scenario, and its
# own housekeeping task would collide with the one under test.
#
# This assumes the held nodes have no spare capacity, which is only true
# under the node-exclusive match-policy configured above. Under a policy
# that packs jobs onto partially allocated nodes, this job could be
# allocated legitimately and the check would report a bug that is not one.
#
# The absence of an alloc event is not sufficient on its own: a job that
# took an exception never emits one either, and would pass the check while
# proving nothing about the scheduler. Require the job to still be in SCHED
# as well, which it can only be if it is waiting for resources. The state is
# read before the cancel below, since cancelling moves it to INACTIVE.
stays_pending () {
	id=$(flux submit --queue=$1 -N$2 sleep inf) || return 1
	test_must_fail flux job wait-event -t $PENDING_TIMEOUT $id alloc
	rc=$?
	if test $rc -eq 0 && test "$(flux jobs -no {state} $id)" != "SCHED"; then
		rc=1
	fi
	flux cancel $id >/dev/null 2>&1
	flux job wait-event -t 30 $id clean >/dev/null 2>&1
	return $rc
}

encode_R >R.base &&
encode_R --property=pdat:1-2 >R.pdat &&
cp R.base R || error "failed to generate resource sets"

cat >conf.d/resource.toml <<-EOT
[resource]
noverify = true
path = "$(pwd)/R"
EOT

# Only the job that is meant to strand a node sleeps. Gating on a
# sentinel file keeps probe jobs submitted later in a scenario from
# leaving housekeeping tasks of their own behind on rank 1.
cat >conf.d/housekeeping.toml <<-EOT
[job-manager.housekeeping]
command = [
  "/bin/bash", "-c",
  "test \$(flux getattr rank) -eq 1 && test -e $(pwd)/hk-hold && exec sleep inf; exit 0"
]
release-after = "0"
EOT

write_sched_conf rv1
write_queue_conf

export FLUX_SCHED_MODULE=none
test_under_flux $SIZE full -o,--config-path=$(pwd)/conf.d

test_expect_success 'load fluxion modules' '
	flux module load sched-fluxion-resource &&
	flux module load sched-fluxion-qmanager &&
	flux module load sched-fluxion-feasibility
'
test_expect_success 'all queues are configured' '
	flux queue status >status.out &&
	grep pall status.out &&
	grep pbatch status.out
'
test_expect_success 'pall is stopped and pbatch is started' '
	flux queue stop --queue=pall &&
	flux queue start --queue=pbatch &&
	flux queue status --queue=pall | grep "Scheduling is stopped" &&
	flux queue status --queue=pbatch | grep "Scheduling is started"
'
test_expect_success 'scheduler sees no nodes allocated' '
	test $(sched_alloc_nnodes) -eq 0
'

for fmt in $MATCH_FORMATS; do
    test_expect_success "$fmt: configure match-format" '
	cleanup_state &&
	cp R.base R &&
	write_sched_conf $fmt &&
	write_queue_conf &&
	reconfig &&
	flux queue start --quiet --queue=pbatch &&
	assert_queues &&
	assert_modules &&
	assert_properties &&
	state_is_clean
    '
    for scenario in $SCENARIOS; do
	test_expect_success "$fmt/$scenario: instance starts clean" '
		state_is_clean &&
		assert_queues &&
		assert_modules &&
		assert_properties
	'
	test_expect_success "$fmt/$scenario: hold resources" '
		setup_state $scenario &&
		test $(sched_alloc_nnodes) -eq $(cat held)
	'
	test_expect_success "$fmt/$scenario: add pdat and reconfigure" '
		cp R.pdat R &&
		write_queue_conf pdat &&
		flux dmesg -C &&
		reconfig &&
		flux queue start --quiet --queue=pdat
	'
	test_expect_success "$fmt/$scenario: queues correct after add" '
		assert_queues pdat
	'
	test_expect_success "$fmt/$scenario: modules loaded after add" '
		assert_modules
	'
	test_expect_success "$fmt/$scenario: pdat resources present after add" '
		assert_properties pdat
	'
	test_expect_success "$fmt/$scenario: held nodes still allocated" '
		flux dmesg -H >dmesg-add.out &&
		test_debug "cat dmesg-add.out" &&
		test -z "$(drained_ranks)" &&
		test $(sched_alloc_nnodes) -eq $(cat held)
	'
	test_expect_success "$fmt/$scenario: oversubscribed pbatch job pends" '
		stays_pending pbatch $(($(expected_free) + 1))
	'
	test_expect_success "$fmt/$scenario: pdat job needing rank 1 pends" '
		stays_pending pdat 2
	'
	test_expect_success "$fmt/$scenario: remove pdat and reconfigure" '
		cp R.base R &&
		write_queue_conf &&
		flux dmesg -C &&
		reconfig
	'
	test_expect_success "$fmt/$scenario: queues correct after remove" '
		assert_queues
	'
	test_expect_success "$fmt/$scenario: modules loaded after remove" '
		assert_modules
	'
	test_expect_success "$fmt/$scenario: pdat resources gone after remove" '
		assert_properties
	'
	# No stays_pending check here. Proving a job is not allocated means
	# waiting out PENDING_TIMEOUT, and a passing run always waits the
	# full duration, so each of these costs more than the rest of the
	# scenario combined. The allocated-node count above already covers
	# the remove path: it is the same assertion, taken directly from the
	# scheduler rather than inferred from a job that fails to start. The
	# add path keeps its checks so that the scheduler's reported state is
	# confirmed against real allocation behavior at least once per
	# scenario, which is what a status RPC alone cannot establish.
	test_expect_success "$fmt/$scenario: held nodes still allocated" '
		flux dmesg -H >dmesg-remove.out &&
		test_debug "cat dmesg-remove.out" &&
		test -z "$(drained_ranks)" &&
		test $(sched_alloc_nnodes) -eq $(cat held)
	'
	test_expect_success "$fmt/$scenario: housekeeping frees cleanly" '
		flux dmesg -C &&
		cleanup_state &&
		flux dmesg -H >dmesg-free.out &&
		test_debug "cat dmesg-free.out" &&
		test_must_fail grep -i "free.*fail" dmesg-free.out
	'
    done
done

test_expect_success 'remove fluxion modules' '
	flux module remove sched-fluxion-feasibility &&
	flux module remove sched-fluxion-qmanager &&
	flux module remove sched-fluxion-resource
'

test_done

#!/bin/sh
test_description='Test partial release when jobs share a node

Two single-core jobs share rank 1 with a two-node job. When the two-node
job ends, housekeeping holds rank 0 and releases rank 1. The partial
release must keep the two single-core jobs, make the released core of
rank 1 available, and keep the core of rank 0 until housekeeping ends.
The test does this for each match format, because the reload of a job
that is partially released goes through the reader of the match format.

Each format also replays that state. The modules reload while two tenants
still run on rank 1 and housekeeping still holds rank 0. The reader of the
format must rebuild the partial allocation, so the released core stays
usable and the held core stays unavailable.
'

. $(dirname $0)/sharness.sh

export TEST_UNDER_FLUX_CORES_PER_RANK=4
test_under_flux 2 job

TOTAL_NCORES=8

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

# Usage: replay_reader JOBID
#
# The reader that rebuilds a job on reload comes from the R of the job, not
# from the match-format option. parse_R() picks it from scheduling.writer,
# and falls back to the rv1exec reader when R has no scheduling key. Report
# it, so each replay says which reader rebuilt the partial allocation.
replay_reader () {
	flux job info $1 R | jq -r '
	    if has("scheduling") then (.scheduling.writer // "fluxion:jgf")
	    else "rv1exec" end'
}

# Usage: stays_pending ncores rank
#
# Submit a job that can only run on cores housekeeping still holds. An
# alloc event is the bug. The absence of one is not enough on its own: a
# job the scheduler denies, or one that takes any other exception, never
# emits an alloc event either, and would pass while proving nothing. Also
# require the job to still be in SCHED, which it can only be while it waits
# for resources. Read the state before the cancel, because cancelling moves
# the job to INACTIVE.
stays_pending () {
	id=$(flux submit -n$1 --requires=rank:$2 true) || return 1
	test_must_fail flux job wait-event -t $PENDING_TIMEOUT $id alloc
	rc=$?
	state=$(flux jobs -no {state} $id)
	if test $rc -eq 0 && test "$state" != "SCHED"; then
		echo "job $id is $state, not SCHED" >&2
		rc=1
	fi
	flux cancel $id >/dev/null 2>&1
	flux job wait-event -t 30 $id clean >/dev/null 2>&1
	return $rc
}

test_expect_success 'load fluxion modules with a policy that shares nodes' '
	flux module remove -f sched-simple &&
	load_resource match-format=rv1_nosched policy=first &&
	load_qmanager_sync &&
	flux resource list &&
	FLUX_RESOURCE_LIST_RPC=sched.resource-status flux resource list
'

# Check job manager hello debug message for +partial-ok flag
if flux dmesg | grep +partial-ok; then
    test_set_prereq HAVE_PARTIAL_OK
fi

test_expect_success 'configure housekeeping that holds rank 0' '
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

# Usage: partial_release_shared_node FORMAT
# Do the test sequence with the loaded modules. FORMAT is only a label
# for the output.
partial_release_shared_node () {
	fmt=$1
	test_expect_success HAVE_PARTIAL_OK "$fmt: clear the log" '
		flux dmesg -C
	'
	test_expect_success HAVE_PARTIAL_OK "$fmt: run two single-core jobs on rank 1" '
		jobA=$(flux submit -n1 --requires=rank:1 sleep inf) &&
		jobB=$(flux submit -n1 --requires=rank:1 sleep inf) &&
		flux job wait-event -t 30 $jobA start &&
		flux job wait-event -t 30 $jobB start &&
		echo $jobA >jobA.id &&
		echo $jobB >jobB.id
	'
	test_expect_success HAVE_PARTIAL_OK "$fmt: run a two-node job that shares rank 1" '
		jobD=$(flux submit -N2 -n2 sleep inf) &&
		flux job wait-event -t 30 $jobD start &&
		test $(fluxion_allocated ncores) -eq 4 &&
		flux cancel $jobD &&
		flux job wait-event -t 30 $jobD clean
	'
	test_expect_success HAVE_PARTIAL_OK "$fmt: housekeeping releases rank 1 and holds rank 0" '
		hk_wait_for_running 1 &&
		hk_wait_for_allocated_nnodes 1 &&
		test $(fluxion_allocated ncores) -eq 3
	'
	test_expect_success HAVE_PARTIAL_OK "$fmt: the single-core jobs still run" '
		test $(flux jobs -no {state} $(cat jobA.id)) = RUN &&
		test $(flux jobs -no {state} $(cat jobB.id)) = RUN
	'
	test_expect_success HAVE_PARTIAL_OK "$fmt: a new job can use the released core of rank 1" '
		reuse_released_cores 2 1
	'
	test_expect_success HAVE_PARTIAL_OK "$fmt: a job cannot use the core of rank 0 in housekeeping" '
		hk_wait_for_running 1 &&
		test $(fluxion_allocated ncores) -eq 3 &&
		stays_pending 4 0 &&
		hk_wait_for_running 1 &&
		test $(fluxion_allocated ncores) -eq 3
	'
	test_expect_success HAVE_PARTIAL_OK "$fmt: the end of housekeeping frees rank 0" '
		flux housekeeping kill --all &&
		hk_wait_for_running 0 &&
		test $(fluxion_allocated ncores) -eq 2
	'
	test_expect_success HAVE_PARTIAL_OK "$fmt: the single-core jobs end, and all cores are free" '
		flux cancel $(cat jobA.id) $(cat jobB.id) &&
		flux job wait-event -t 30 $(cat jobA.id) clean &&
		flux job wait-event -t 30 $(cat jobB.id) clean &&
		hk_wait_for_running 0 &&
		test $(fluxion_free ncores) -eq $TOTAL_NCORES
	'
	test_expect_success HAVE_PARTIAL_OK "$fmt: fluxion logged no error" '
		fluxion_errors log.$fmt.out
	'
}

# Usage: reload_active_partial FORMAT
#
# Reload the modules into FORMAT while the state is partially released, so
# the reader of FORMAT has to rebuild it. Two tenants still run on rank 1,
# a third rank-1 core has just been released, and housekeeping still holds
# a core of rank 0. Reloading after every job and housekeeping task ends
# only exercises the writer of the format, not its replay of live state.
reload_active_partial () {
	fmt=$1
	test_expect_success HAVE_PARTIAL_OK "$fmt replay: two tenants share rank 1" '
		flux dmesg -C &&
		jobA=$(flux submit -n1 --requires=rank:1 sleep inf) &&
		jobB=$(flux submit -n1 --requires=rank:1 sleep inf) &&
		flux job wait-event -t 30 $jobA start &&
		flux job wait-event -t 30 $jobB start &&
		echo $jobA >jobA.id &&
		echo $jobB >jobB.id
	'
	test_expect_success HAVE_PARTIAL_OK "$fmt replay: rank 1 is released and rank 0 is held" '
		jobD=$(flux submit -N2 -n2 sleep inf) &&
		flux job wait-event -t 30 $jobD start &&
		flux cancel $jobD &&
		flux job wait-event -t 30 $jobD clean &&
		hk_wait_for_running 1 &&
		hk_wait_for_allocated_nnodes 1 &&
		test $(fluxion_allocated ncores) -eq 3
	'
	test_expect_success HAVE_PARTIAL_OK "$fmt replay: the reader matches the format" '
		reader=$(replay_reader $(cat jobA.id)) &&
		test_debug "echo $fmt replays through the $reader reader" &&
		if test $fmt = rv1_nosched; then
			test "$reader" = "rv1exec"
		else
			test "${reader#fluxion}" != "$reader"
		fi
	'
	test_expect_success HAVE_PARTIAL_OK "$fmt replay: reload into match-format=$fmt" '
		remove_qmanager &&
		reload_resource match-format=$fmt policy=first &&
		load_qmanager_sync &&
		FLUX_RESOURCE_LIST_RPC=sched.resource-status flux resource list
	'
	test_expect_success HAVE_PARTIAL_OK "$fmt replay: the counts survive the reload" '
		test $(fluxion_allocated ncores) -eq 3 &&
		test $(flux jobs -no {state} $(cat jobA.id)) = RUN &&
		test $(flux jobs -no {state} $(cat jobB.id)) = RUN
	'
	test_expect_success HAVE_PARTIAL_OK "$fmt replay: a new job uses the released core" '
		reuse_released_cores 2 1
	'
	test_expect_success HAVE_PARTIAL_OK "$fmt replay: the held core of rank 0 stays held" '
		hk_wait_for_running 1 &&
		test $(fluxion_allocated ncores) -eq 3 &&
		stays_pending 4 0 &&
		hk_wait_for_running 1 &&
		test $(fluxion_allocated ncores) -eq 3
	'
	test_expect_success HAVE_PARTIAL_OK "$fmt replay: everything frees cleanly" '
		flux housekeeping kill --all &&
		hk_wait_for_running 0 &&
		flux cancel $(cat jobA.id) $(cat jobB.id) &&
		flux job wait-event -t 30 $(cat jobA.id) clean &&
		flux job wait-event -t 30 $(cat jobB.id) clean &&
		hk_wait_for_running 0 &&
		test $(fluxion_free ncores) -eq $TOTAL_NCORES
	'
	test_expect_success HAVE_PARTIAL_OK "$fmt replay: fluxion logged no error" '
		fluxion_errors log.replay.$fmt.out
	'
}

partial_release_shared_node rv1_nosched
reload_active_partial rv1_nosched

for fmt in rv1 rv1_shorthand; do
	test_expect_success HAVE_PARTIAL_OK "reload fluxion modules with match-format=$fmt" '
		remove_qmanager &&
		reload_resource match-format=$fmt policy=first &&
		load_qmanager_sync &&
		FLUX_RESOURCE_LIST_RPC=sched.resource-status flux resource list
	'
	partial_release_shared_node $fmt
	reload_active_partial $fmt
done

test_expect_success 'unload fluxion modules' '
	remove_qmanager &&
	remove_resource &&
	flux module load sched-simple
'
test_done

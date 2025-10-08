#!/bin/sh
#
test_description='Ensure fluxion cancels resources across racks correctly'

. `dirname $0`/sharness.sh

if test_have_prereq ASAN; then
    skip_all='skipping issues tests under AddressSanitizer'
    test_done
fi

SIZE=20
test_under_flux ${SIZE}

# Usage: load_config MATCH_POLICY PRUNE_FILTERS
# Example: load_config "low" "ALL:core,ALL:node"
load_config() {
	flux config load <<-EOF &&
	[job-manager.housekeeping]
	command = [
		"sh",
		"-c",
		"test \$(flux getattr rank) -eq 17 && sleep inf; exit 0"
	]
	release-after = "0s"

	[resource]
	noverify = true
	norestrict = true
	scheduling = "${SHARNESS_TEST_SRCDIR}/data/resource/jgfs/power.json"

	[[resource.config]]
	hosts = "node[0-71]"
	cores = "0-35"
	gpus = "0-1"

	[sched-fluxion-resource]
	match-policy = "$1"
	prune-filters = "$2"
	EOF
	return 0
}

# Reload resource and qmanager
reload_flux() {
	remove_qmanager &&
	reload_resource &&
	load_qmanager_sync &&
	flux resource list &&
	FLUX_RESOURCE_LIST_RPC=sched.resource-status flux resource list
}

# Usage: wait_for_allocated_count N
wait_for_allocated_count() {
    retries=5
    while test $retries -ge 0; do
        test $(flux resource list -s allocated -no {nnodes}) -eq $1 && return 0
        retries=$(($retries-1))
        sleep 0.1
    done
    return 1
}

test_expect_success 'successfully loads fluxion with sticky rank in rack0' '
	flux module remove sched-simple &&
	flux module remove resource &&
	load_config "low" "ALL:core,ALL:node" &&
	flux module load resource monitor-force-up &&
	flux module load sched-fluxion-resource &&
	flux module load sched-fluxion-qmanager &&
	flux module unload job-list &&
	flux module list &&
	flux queue start --all --quiet &&
	flux resource list &&
	flux resource status
'

test_expect_success 'submit two jobs; the second allocates 2 nodes in rack1' '
	jobid1=$(flux submit -N17 -n612 --exclusive sleep inf) &&
	jobid2=$(flux submit --flags=waitable -N3 -n108 --exclusive true) &&
	flux job wait-event -vt5 ${jobid2} alloc &&
	flux job wait-event -vt5 ${jobid2} clean &&
	flux cancel ${jobid1}
'

# Check job manager hello debug message for +partial-ok flag
if flux dmesg | grep +partial-ok; then
    test_set_prereq HAVE_PARTIAL_OK
fi

test_expect_success HAVE_PARTIAL_OK 'fluxion shows 1 node allocated' '
	wait_for_allocated_count 1
'

test_expect_success HAVE_PARTIAL_OK 'kill housekeeping' '
	flux housekeeping kill --all
'

# After killing housekeeping the node should be released
test_expect_success HAVE_PARTIAL_OK 'fluxion shows 0 nodes allocated' '
	wait_for_allocated_count 0
'

test_expect_success HAVE_PARTIAL_OK 'reload fluxion modules for lonodex' '
	load_config "lonodex" "ALL:core,ALL:node" &&
	reload_flux
'

test_expect_success 'submit two jobs; the second allocates 2 nodes in rack1' '
	jobid1=$(flux submit -N17 -n612 --exclusive sleep inf) &&
	jobid2=$(flux submit --flags=waitable -N3 -n108 --exclusive true) &&
	flux job wait-event -vt5 ${jobid2} alloc &&
	flux job wait-event -vt5 ${jobid2} clean &&
	flux cancel ${jobid1}
'

# Check job manager hello debug message for +partial-ok flag
if flux dmesg | grep +partial-ok; then
    test_set_prereq HAVE_PARTIAL_OK
fi

test_expect_success HAVE_PARTIAL_OK 'fluxion shows 1 node allocated' '
	wait_for_allocated_count 1
'

test_expect_success HAVE_PARTIAL_OK 'kill housekeeping' '
	flux housekeeping kill --all
'

# After killing housekeeping the node should be released
test_expect_success HAVE_PARTIAL_OK 'fluxion shows 0 nodes allocated' '
	wait_for_allocated_count 0
'

test_expect_success HAVE_PARTIAL_OK 'reload fluxion modules, set up to node pruning filters' '
	load_config "low" "ALL:core,ALL:gpu,ALL:socket,ALL:node,ALL:memory" &&
	reload_flux
'

test_expect_success 'submit two jobs; the second allocates 2 nodes in rack1' '
	jobid1=$(flux submit -N17 -n612 --exclusive sleep inf) &&
	jobid2=$(flux submit --flags=waitable -N3 -n108 --exclusive true) &&
	flux job wait-event -vt5 ${jobid2} alloc &&
	flux job wait-event -vt5 ${jobid2} clean &&
	flux cancel ${jobid1}
'

# Check job manager hello debug message for +partial-ok flag
if flux dmesg | grep +partial-ok; then
    test_set_prereq HAVE_PARTIAL_OK
fi

test_expect_success HAVE_PARTIAL_OK 'fluxion shows 1 node allocated' '
	wait_for_allocated_count 1
'

test_expect_success HAVE_PARTIAL_OK 'kill housekeeping' '
	flux housekeeping kill --all
'

# After killing housekeeping the node should be released
test_expect_success HAVE_PARTIAL_OK 'fluxion shows 0 nodes allocated' '
	wait_for_allocated_count 0
'

test_expect_success HAVE_PARTIAL_OK 'reload fluxion modules for lonodex, set up to node pruning filters' '
	load_config "lonodex" "ALL:core,ALL:gpu,ALL:socket,ALL:node,ALL:memory" &&
	reload_flux
'

test_expect_success 'submit two jobs; the second allocates 2 nodes in rack1' '
	jobid1=$(flux submit -N17 -n612 --exclusive sleep inf) &&
	jobid2=$(flux submit --flags=waitable -N3 -n108 --exclusive true) &&
	flux job wait-event -vt5 ${jobid2} alloc &&
	flux job wait-event -vt5 ${jobid2} clean &&
	flux cancel ${jobid1}
'

# Check job manager hello debug message for +partial-ok flag
if flux dmesg | grep +partial-ok; then
    test_set_prereq HAVE_PARTIAL_OK
fi

test_expect_success HAVE_PARTIAL_OK 'fluxion shows 1 node allocated' '
	wait_for_allocated_count 1
'

test_expect_success HAVE_PARTIAL_OK 'kill housekeeping' '
	flux housekeeping kill --all
'

# After killing housekeeping the node should be released
test_expect_success HAVE_PARTIAL_OK 'fluxion shows 0 nodes allocated' '
	wait_for_allocated_count 0
'

test_expect_success HAVE_PARTIAL_OK 'reload fluxion modules, set leaf pruning filters' '
	load_config "low" "ALL:core,ALL:gpu,ALL:socket,ALL:node,ALL:memory" &&
	reload_flux
'

test_expect_success 'submit two jobs; the second allocates 2 nodes in rack1' '
	jobid1=$(flux submit -N17 -n612 --exclusive sleep inf) &&
	jobid2=$(flux submit --flags=waitable -N3 -n108 --exclusive true) &&
	flux job wait-event -vt5 ${jobid2} alloc &&
	flux job wait-event -vt5 ${jobid2} clean &&
	flux cancel ${jobid1}
'

# Check job manager hello debug message for +partial-ok flag
if flux dmesg | grep +partial-ok; then
    test_set_prereq HAVE_PARTIAL_OK
fi

test_expect_success HAVE_PARTIAL_OK 'fluxion shows 1 node allocated' '
	wait_for_allocated_count 1
'

test_expect_success HAVE_PARTIAL_OK 'kill housekeeping' '
	flux housekeeping kill --all
'

# After killing housekeeping the node should be released
test_expect_success HAVE_PARTIAL_OK 'fluxion shows 0 nodes allocated' '
	wait_for_allocated_count 0
'

test_expect_success HAVE_PARTIAL_OK 'reload fluxion modules for lonodex, set leaf pruning filters' '
	load_config "lonodex" "ALL:core,ALL:gpu,ALL:memory" &&
	reload_flux
'

test_expect_success 'submit two jobs; the second allocates 2 nodes in rack1' '
	jobid1=$(flux submit -N17 -n612 --exclusive sleep inf) &&
	jobid2=$(flux submit --flags=waitable -N3 -n108 --exclusive true) &&
	flux job wait-event -vt5 ${jobid2} alloc &&
	flux job wait-event -vt5 ${jobid2} clean &&
	flux cancel ${jobid1}
'

# Check job manager hello debug message for +partial-ok flag
if flux dmesg | grep +partial-ok; then
    test_set_prereq HAVE_PARTIAL_OK
fi

test_expect_success HAVE_PARTIAL_OK 'fluxion shows 1 node allocated' '
	wait_for_allocated_count 1
'

test_expect_success HAVE_PARTIAL_OK 'kill housekeeping' '
	flux housekeeping kill --all
'

# After killing housekeeping the node should be released
test_expect_success HAVE_PARTIAL_OK 'fluxion shows 0 nodes allocated' '
	wait_for_allocated_count 0
'

test_expect_success 'unload fluxion modules' '
	remove_qmanager &&
	remove_resource &&
	flux module load sched-simple
'

test_done

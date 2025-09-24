#!/bin/sh
test_description='\
Test fluxion handling of partial release driven by housekeeping'

. $(dirname $0)/sharness.sh

export TEST_UNDER_FLUX_CORES_PER_RANK=4
test_under_flux 4 job

TOTAL_NCORES=16

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
        while test $(flux housekeeping list -no {allocated.nnodes}) -ne $1; do
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


test_expect_success 'load fluxion modules' '
	flux module remove -f sched-simple &&
	load_resource match-format=rv1_nosched &&
	load_qmanager_sync &&
	flux resource list &&
	FLUX_RESOURCE_LIST_RPC=sched.resource-status flux resource list
'

# Check job manager hello debug message for +partial-ok flag
if flux dmesg | grep +partial-ok; then
    test_set_prereq HAVE_PARTIAL_OK
fi

test_expect_success 'run a normal job, resources are free' '
	flux run -vvv -xN4 true &&
	test_debug "echo free=\$(fluxion_free ncores)" &&
	test $(fluxion_free ncores) -eq $TOTAL_NCORES
'
test_expect_success 'run 4 single node jobs, resources are free' '
	flux submit -v --cc=1-4 -xN1 --wait true &&
	flux resource list -s free -no "total={ncores}" &&
	test $(flux resource list -s free -no {ncores}) -eq $TOTAL_NCORES
'
test_expect_success 'run 16 single core jobs, resources are free' '
	flux submit -v --cc=1-16 -n1 --wait true &&
	test_debug "echo free=\$(fluxion_free ncores)" &&
	test $(fluxion_free ncores) -eq $TOTAL_NCORES
'
test_expect_success 'clear dmesg buffer' '
	flux dmesg -C
'
test_expect_success 'run a job with unequal core distribution, resources are free' '
	flux run -vvv -n7 -l flux getattr rank &&
	test_debug "flux job info $(flux job last) R | jq" &&
	test_debug "echo free=\$(fluxion_free ncores)" &&
	test $(fluxion_free ncores) -eq $TOTAL_NCORES
'
test_expect_success 'attempt to ensure dmesg buffer synchronized' '
	flux logger test-sentinel &&
	dmesg_wait test-sentinel
'
test_expect_success 'no fluxion errors logged' '
	flux dmesg -H >log.out &&
	test_debug "cat log.out" &&
	test_must_fail grep "free RPC failed to remove all resources" log.out
'
test_expect_success 'clear dmesg buffer' '
	flux dmesg -C
'
test_expect_success 'enable housekeeping with immediate partial release' '
	flux config load <<-EOF
	[job-manager.housekeeping]
	command = ["sleep", "0"]
	release-after = "0s"
	EOF
'
test_expect_success 'run a job across all nodes, wait for housekeeping' '
	flux run -N4 -n4 true &&
	hk_wait_for_running 0
'
test_expect_success 'attempt to ensure dmesg buffer synchronized' '
	flux logger test-sentinel &&
	dmesg_wait test-sentinel
'
test_expect_success 'all resources free' '
	test_debug "echo free=\$(fluxion_free ncores)" &&
	test $(fluxion_free ncores) -eq $TOTAL_NCORES
'
test_expect_success 'no errors from fluxion' '
	flux dmesg -H >log2.out &&
	test_must_fail grep "free RPC failed to remove all resources" log.out
'
test_expect_success HAVE_PARTIAL_OK 'reconfigure housekeeping with sticky node' '
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
test_expect_success HAVE_PARTIAL_OK 'run a job and wait for node to get stuck' '
	flux run -N4 true &&
	hk_wait_for_running 1 &&
	hk_wait_for_allocated_nnodes 1
'
test_expect_success HAVE_PARTIAL_OK 'fluxion shows 1 node allocated' '
	test $(fluxion_allocated nnodes) -eq 1
'
test_expect_success HAVE_PARTIAL_OK 'reload fluxion modules' '
	remove_qmanager &&
	reload_resource match-format=rv1_nosched &&
	load_qmanager_sync &&
	flux resource list &&
	FLUX_RESOURCE_LIST_RPC=sched.resource-status flux resource list
'
test_expect_success HAVE_PARTIAL_OK 'fluxion still shows 1 node allocated' '
	test $(fluxion_allocated nnodes) -eq 1
'
test_expect_success HAVE_PARTIAL_OK 'kill housekeeping' '
	flux housekeeping kill --all
'
test_expect_success HAVE_PARTIAL_OK 'fluxion shows 0 nodes allocated' '
	hk_wait_for_running 0 &&
	test $(fluxion_allocated nnodes) -eq 0
'
test_expect_success HAVE_PARTIAL_OK 'reload fluxion modules with match-format=rv1' '
	remove_qmanager &&
	reload_resource match-format=rv1 &&
	load_qmanager_sync &&
	flux resource list &&
	FLUX_RESOURCE_LIST_RPC=sched.resource-status flux resource list
'
test_expect_success HAVE_PARTIAL_OK 'run a job and wait for node to get stuck' '
	flux run -N4 true &&
	hk_wait_for_running 1 &&
	hk_wait_for_allocated_nnodes 1
'
test_expect_success HAVE_PARTIAL_OK 'fluxion shows 1 nodes allocated' '
	test $(fluxion_allocated nnodes) -eq 1
'
test_expect_success HAVE_PARTIAL_OK 'run a sleep job on one node' '
	flux submit --wait-event=alloc -N1 sleep 3600
'
test_expect_success HAVE_PARTIAL_OK 'fluxion shows 2 nodes allocated' '
	test $(fluxion_allocated nnodes) -eq 2
'
test_expect_success HAVE_PARTIAL_OK 'reload fluxion modules with match-format=rv1' '
	remove_qmanager &&
	reload_resource match-format=rv1 &&
	load_qmanager_sync &&
	flux resource list &&
	FLUX_RESOURCE_LIST_RPC=sched.resource-status flux resource list
'
test_expect_success HAVE_PARTIAL_OK 'fluxion still shows 2 nodes allocated' '
	test $(fluxion_allocated nnodes) -eq 2
'
test_expect_success HAVE_PARTIAL_OK 'kill housekeeping' '
	flux housekeeping kill --all
'
test_expect_success HAVE_PARTIAL_OK 'fluxion shows 1 node allocated' '
	hk_wait_for_running 0 &&
	test $(fluxion_allocated nnodes) -eq 1
'

test_expect_success 'unload fluxion modules' '
	remove_qmanager &&
	remove_resource &&
	flux module load sched-simple
'
test_done

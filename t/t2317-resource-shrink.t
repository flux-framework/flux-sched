#!/bin/sh

test_description='Test resource shrink when not using resource configuration'

. `dirname $0`/sharness.sh

notify_base=`readlink -e ${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/satisfiability`

SIZE=4
test_under_flux $SIZE full --test-exit-mode=leader
export FLUX_URI_RESOLVE_LOCAL=t

# Usage: waitup N
#   where N is a count of online ranks
waitup () {
	run_timeout 5 flux python -c "import flux; print(flux.Flux().rpc(\"resource.monitor-waitup\",{\"up\":$1}).get())"
}
force_down () {
	flux python -c "import flux; flux.Flux().rpc(\"resource.monitor-force-down\", {\"ranks\":\"$1\"}).get()"
}

# Usage: wait_for_node_count N
wait_for_node_count() {
    retries=5
    while test $retries -ge 0; do
        test $(flux resource list -s all -no {nnodes}) -eq $1 && return 0
        retries=$(($retries-1))
        sleep 0.1
    done
    return 1
}

# If force-down is not supported then this version of flux-core does not
# support shrink, so skip all tests:
if ! force_down "" 2>/dev/null ; then
       skip_all='resource.monitor-force-down failed, skipping all tests'
	test_done
fi

test_expect_success 'load fluxion' '
	flux module remove sched-simple &&
	flux module load sched-fluxion-resource &&
	flux module load sched-fluxion-qmanager &&
	flux module load sched-fluxion-feasibility
'
test_expect_success 'submit a resilient job using all ranks' '
	jobid=$(flux alloc --broker-opts=-Sbroker.module-nopanic=1 --bg -xN4 -o exit-timeout=none --conf=tbon.topo=kary:0) &&
	flux proxy $jobid flux overlay status
'
test_expect_success 'a job on all ranks is satisfiable' '
    flux ion-resource -v match satisfiability ${notify_base}/shrink4.yaml
'
test_expect_success 'disconnect rank 3' '
	flux overlay disconnect 3
'
test_expect_success 'there are now only 3 nodes' '
	wait_for_node_count 3
'
test_expect_success 'scheduler reports the same' '
	FLUX_RESOURCE_LIST_RPC=sched.resource-status \
		flux resource list -s all &&
	FLUX_RESOURCE_LIST_RPC=sched.resource-status \
		test $(flux resource list -s all -no {nnodes}) -eq 3
'
test_expect_success 'subinstance shows the same' '
	flux job eventlog -H $jobid &&
	uri=$(flux uri --local $jobid) &&
	(FLUX_URI=$uri waitup 3) &&
	flux proxy $jobid flux resource list -s all &&
	test $(flux proxy $jobid flux resource list -s all -no {nnodes}) -eq 3
'
test_expect_success 'running a 3 node job in subinstance works' '
	flux proxy $jobid flux run -N3 hostname
'
test_expect_success 'a 4 node job no longer runs' '
	test_must_fail flux run -N4 hostname 2>submit.err &&
	test_debug "cat submit.err" &&
	grep "unsatisfiable" submit.err
'
test_expect_success 'a 4 node job is now unsatisfiable' '
    test_must_fail flux ion-resource match satisfiability ${notify_base}/shrink4.yaml
'
test_expect_success 'shutdown subinstance works' '
	flux shutdown $jobid
'
test_expect_success 'there are now 3 nodes free' '
	flux resource list -s free &&
	test $(flux resource list -s free -no {nnodes}) -eq 3
'
test_expect_success 'resource.monitor-force-down rejects invalid idset' '
	test_must_fail force_down foo
'
test_expect_success 'resource.monitor-force-down works' '
	force_down 2 &&
	waitup 2
'
test_expect_success 'there are now only 2 nodes' '
	flux resource list -s all &&
	test $(flux resource list -s all -no {nnodes}) -eq 2 &&
	flux dmesg -HL | tail -3
'
test_expect_success 'scheduler reports the same' '
	FLUX_RESOURCE_LIST_RPC=sched.resource-status \
		flux resource list -s all &&
	FLUX_RESOURCE_LIST_RPC=sched.resource-status \
		test $(flux resource list -s all -no {nnodes}) -eq 2
'
test_expect_success 'a 3 node job no longer runs' '
	test_must_fail flux run -N3 hostname 2>submit2.err &&
	test_debug "cat submit2.err" &&
	grep "unsatisfiable" submit2.err
'
test_expect_success 'a 3 node job is now unsatisfiable' '
    test_must_fail flux ion-resource match satisfiability ${notify_base}/shrink3.yaml
'
test_expect_success 'but a 2 node job runs' '
	flux run -N2 hostname
'
test_expect_success 'and a 2 node job is satisfiable' '
    flux ion-resource match satisfiability ${notify_base}/shrink2.yaml
'
test_expect_success 'unload fluxion' '
	flux module remove sched-fluxion-feasibility &&
	flux module remove sched-fluxion-qmanager &&
	flux module remove sched-fluxion-resource &&
	flux module load sched-simple
'
test_done

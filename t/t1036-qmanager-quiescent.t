#!/bin/sh

test_description='Test qmanager quiescence RPC behavior'

. `dirname $0`/sharness.sh

hwloc_basepath=`readlink -e ${SHARNESS_TEST_SRCDIR}/data/hwloc-data`
# 1 brokers, each (exclusively) have: 1 node, 2 sockets, 16 cores (8 per socket)
excl_1N1B="${hwloc_basepath}/001N/exclusive/01-brokers"

export FLUX_SCHED_MODULE=none
test_under_flux 1

exec_test()     { ${jq} '.attributes.system.exec.test = {}'; }
exec_testattr() {
    ${jq} --arg key "$1" --arg value $2 \
        '.attributes.system.exec.test[$key] = $value'
}

quiescent_rpc_timeout() {
    run_timeout 5 flux python -c \
        "import flux, json; print(json.dumps(flux.Flux().rpc(\"sched.quiescent\").get(), sort_keys=True))"
}

test_expect_success 'quiescent: load test resources' '
    load_test_resources ${excl_1N1B}
'

test_expect_success 'quiescent: generate a node-exclusive long-running jobspec' '
    flux run --dry-run -N1 -n16 -x -t 5m hostname | \
        exec_testattr run_duration 300s > exclusive.json
'

test_expect_success 'quiescent: loading fluxion modules works' '
    flux module load sched-fluxion-resource prune-filters=ALL:core \
subsystems=containment policy=low &&
    load_qmanager_sync queue-policy=easy
'

test_expect_success 'quiescent: idle scheduler responds immediately with no allocations' '
    quiescent_rpc_timeout > quiescent-idle.json &&
    jq -e ".status == 0 and .alloc_current == 0" quiescent-idle.json
'

test_expect_success 'quiescent: running jobs are reported without waiting' '
    jobid1=$(flux job submit exclusive.json) &&
    echo ${jobid1} > jobid1 &&
    flux job wait-event -t 10 $(cat jobid1) start &&
    quiescent_rpc_timeout > quiescent-running.json &&
    jq -e ".status == 0 and .alloc_current == 1" quiescent-running.json
'

test_expect_success 'quiescent: a blocked job does not hold the response' '
    jobid2=$(flux job submit exclusive.json) &&
    echo ${jobid2} > jobid2 &&
    test_must_fail flux job wait-event -t 1 $(cat jobid2) start &&
    quiescent_rpc_timeout > quiescent-blocked.json &&
    jq -e ".status == 0 and .alloc_current == 1" quiescent-blocked.json &&
    test "$(flux jobs -n -o "{state}" $(cat jobid2))" = "SCHED"
'

test_expect_success 'quiescent: allocation count follows the blocked job into RUN' '
    flux cancel $(cat jobid1) &&
    flux job wait-event -t 10 $(cat jobid1) clean &&
    flux job wait-event -t 10 $(cat jobid2) start &&
    quiescent_rpc_timeout > quiescent-handoff.json &&
    jq -e ".status == 0 and .alloc_current == 1" quiescent-handoff.json
'

test_expect_success 'quiescent: clean up jobs and return to zero allocations' '
    flux cancel $(cat jobid2) &&
    flux job wait-event -t 10 $(cat jobid2) clean &&
    quiescent_rpc_timeout > quiescent-final.json &&
    jq -e ".status == 0 and .alloc_current == 0" quiescent-final.json
'

test_expect_success 'quiescent: cleanup active jobs' '
    cleanup_active_jobs
'

test_expect_success 'quiescent: removing fluxion modules' '
    remove_qmanager &&
    remove_resource
'

test_done

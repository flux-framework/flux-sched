#!/bin/sh

test_description='Test Resource Find and Stutus with Fluxion Modules'

. `dirname $0`/sharness.sh

hwloc_basepath=`readlink -e ${SHARNESS_TEST_SRCDIR}/data/hwloc-data`
# 4 brokers, each (exclusively) have:
# 1 node, 2 sockets, 44 cores (22 per socket), 4 gpus (2 per socket)
excl_4N4B="${hwloc_basepath}/004N/exclusive/04-brokers-sierra2"

skip_all_unless_have jq

remove_times() {
    cat ${1} | jq 'del (.execution.starttime) | del (.execution.expiration)'
}

test_under_flux 4

test_expect_success 'find/status: generate jobspecs' '
    flux mini run --dry-run -N 4 -n 4 -c 44 -g 4 -t 1h sleep 3600 > full.json &&
    flux mini run --dry-run -N 1 -n 1 -c 22 -g 2 -t 1h sleep 3600 > c22g2.json
'

validate_list_row() {
    local output=${1}
    local line=${2}
    local n=${3}
    local c=${4}
    local g=${5}
    nnodes=$(cat ${output} | awk "{ if(NR==${line}) print \$2 } ")
    ncores=$(cat ${output} | awk "{ if(NR==${line}) print \$3 } ")
    ngpus=$(cat ${output} | awk "{ if(NR==${line}) print \$4 } ")
    test ${nnodes} = ${n} && test ${ncores} = ${c} && test ${ngpus} = ${g}
}

test_expect_success 'find/status: hwloc reload works' '
    flux hwloc reload ${excl_4N4B} &&
    flux module remove sched-simple &&
    flux module reload resource
'

test_expect_success 'find/status: loading fluxion modules works' '
    load_resource load-allowlist=cluster,node,core,gpu &&
    load_qmanager queue-policy=easy
'

test_expect_success 'find/status: a jobspec requesting all resources can run' '
    jobid1=$(flux job submit full.json) &&
    flux job wait-event -t 10 ${jobid1} start &&
    flux job info ${jobid1} R > full.R.raw.json &&
    remove_times full.R.raw.json > full.R.json
'

test_expect_success 'find/status: find status=up returns the entire system' '
    flux ion-resource find status=up | tail -1 > up.raw.json &&
    remove_times up.raw.json > up.json &&
    diff full.R.json up.json
'

test_expect_success 'find/status: mark down 1 rank and find status=down' '
    flux resource drain 1 &&
    flux ion-resource find status=down | tail -1 > down.json &&
    rank=$(cat down.json | jq " .execution.R_lite[].rank ") &&
    test ${rank} = "\"1\""
'

test_expect_success 'find/status: find status=down and status=up' '
    flux ion-resource find "status=down and status=up" | tail -1 > null.out &&
    null=$(cat null.out) &&
    test ${null} = "null"
'

test_expect_success 'find/status: find status=down or status=up' '
    flux ion-resource find "status=down or status=up" | \
tail -1 > all.raw.json &&
    remove_times all.raw.json > all.json &&
    diff full.R.json all.json
'

test_expect_success 'find/status: find sched-now=allocated' '
    flux ion-resource find "sched-now=allocated" | \
tail -1 > allocated.raw.json &&
    remove_times allocated.raw.json > allocated.json &&
    diff full.R.json allocated.json
'

test_expect_success 'find/status: find sched-now=allocated status=down' '
    flux ion-resource find "sched-now=allocated status=down" |\
tail -1 > alloc_down.json &&
    rank=$(cat down.json | jq " .execution.R_lite[].rank ") &&
    test ${rank} = "\"1\""
'

test_expect_success 'find/status: a 1sock jobspec cannot run' '
    jobid2=$(flux job submit c22g2.json) &&
    test_must_fail flux job wait-event -t 1 ${jobid2} start
'

# Make sure jobid2 doesn't leave a temporary reservation behind
# All reservations must be cleared up at the end of each schedule loop
test_expect_success 'find/status: find status=reserved must be null' '
    flux ion-resource find "sched-future=reserved" | tail -1 > null.out &&
    null=$(cat null.out) &&
    test ${null} = "null"
'

test_expect_success 'find/status: flux ion-resource status works' '
    flux ion-resource status | tail -1 > status.json &&
    cat status.json | jq " .all " > all.key.raw.json &&
    cat status.json | jq " .allocated " > allocated.key.raw.json &&
    cat status.json | jq " .down " > down.key.json &&
    remove_times all.key.raw.json > all.key.json &&
    remove_times allocated.key.raw.json > allocated.key.json &&
    diff full.R.json all.key.json &&
    diff full.R.json allocated.key.json &&
    downrank=$(cat down.key.json | jq " .execution.R_lite[].rank ") &&
    test ${downrank} = "\"1\""

'

test_expect_success 'find/status: flux resource list works' '
    flux resource list > resource.list.out &&
    validate_list_row resource.list.out 2 0 0 0 &&
    validate_list_row resource.list.out 3 4 176 16 &&
    validate_list_row resource.list.out 4 1 44 4
'

test_expect_success 'find/status: cancel jobs' '
    flux job cancel ${jobid1} &&
    flux job cancel ${jobid2} &&
    flux job wait-event -t 10 ${jobid2} clean
'

test_expect_success 'find/status: removing fluxion modules' '
    remove_qmanager &&
    remove_resource
'

# Reload the core scheduler so that rc3 won't hang waiting for
# queue to become idle after jobs are canceled.
test_expect_success 'find/status: load sched-simple module' '
    flux module load sched-simple
'

test_done


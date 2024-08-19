#!/bin/sh

test_description='Test Node Exclusive Scheduling w/ qmanager'

. `dirname $0`/sharness.sh

hwloc_basepath=`readlink -e ${SHARNESS_TEST_SRCDIR}/data/hwloc-data`
# 4 brokers: 1 node, 2 sockets, 44 cores 4 gpus
excl_4N4B="${hwloc_basepath}/004N/exclusive/04-brokers-sierra2"

export FLUX_SCHED_MODULE=none

test_under_flux 4

is_node_exclusive() {
    ID=${1} &&
    RANKS=${2}
    flux job info ${ID} R > R.${ID} &&
    len1=$(jq ".execution.R_lite | length" R.${ID}) &&
    test ${len1} -eq 1 &&
    ranks=$(jq ".execution.R_lite[0].rank" R.${ID}) &&
    cores=$(jq ".execution.R_lite[0].children.core" R.${ID}) &&
    gpus=$(jq ".execution.R_lite[0].children.gpu" R.${ID}) &&
    test ${ranks} = "\"${RANKS}\"" &&
    test ${cores} = "\"0-43\"" &&
    test ${gpus} = "\"0-3\""
}

test_expect_success 'qmanager-nodex: load test resources' '
    load_test_resources ${excl_4N4B}
'

test_expect_success 'qmanager-nodex: loading fluxion modules works (hinodex)' '
    load_resource policy=hinodex &&
    load_qmanager
'

test_expect_success 'qmanager-nodex: submit a one-core jobs (hinodex)' '
    JOBID1=$(flux submit sleep inf) &&
    flux job wait-event -t 10 ${JOBID1} start
'

test_expect_success 'qmanager-nodex: submit a 8-core jobs (hinodex)' '
    JOBID2=$(flux submit -n8 sleep inf) &&
    flux job wait-event -t 10 ${JOBID2} start
'

test_expect_success 'qmanager-nodex: allocated node is exclusive (hinodex)' '
    is_node_exclusive ${JOBID1} 2 &&
    is_node_exclusive ${JOBID2} 0
'

test_expect_success 'qmanager-nodex: free/alloc node count (hinodex)' '
    cat >status.expected1 <<-EOF &&
	2 88 8
	2 88 8
EOF
    flux resource list > resources.out1 &&
    cat resources.out1 | grep -E "(free|alloc)" \
	| awk "{ print \$2,\$3,\$4 }" > status.out1 &&
    test_cmp status.expected1 status.out1
'

test_expect_success 'qmanager-nodex: submit a -N2 -n2 jobs (hinodex)' '
    JOBID3=$(flux submit -N2 -n2 sleep inf) &&
    flux job wait-event -t 10 ${JOBID3} start
'

test_expect_success 'qmanager-nodex: allocated node is exclusive (hinodex)' '
    is_node_exclusive ${JOBID3} "1,3"
'

test_expect_success 'qmanager-nodex: free/alloc node count 2 (hinodex)' '
    cat >status.expected2 <<-EOF &&
	0 0 0
	4 176 16
EOF
    flux resource list > resources.out2 &&
    cat resources.out2 | grep -E "(free|alloc)" \
        | awk "{ print \$2,\$3,\$4 }" > status.out2 &&
    test_cmp status.expected2 status.out2
'

test_expect_success 'qmanager-nodex: cancel all jobs (hinodex)' '
    flux cancel --all &&
    flux queue drain
'

test_expect_success 'qmanager-nodex: removing fluxion modules (hinodex)' '
    remove_resource
'

test_expect_success 'qmanager-nodex: loading fluxion modules works (lonodex)' '
    load_resource policy=lonodex &&
    load_qmanager
'

test_expect_success 'qmanager-nodex: submit a one-core jobs (lonodex)' '
    JOBID1=$(flux submit sleep inf) &&
    flux job wait-event -t 10 ${JOBID1} start
'

test_expect_success 'qmanager-nodex: submit a 8-core jobs (lonodex)' '
    JOBID2=$(flux submit -n8 sleep inf) &&
    flux job wait-event -t 10 ${JOBID2} start
'

test_expect_success 'qmanager-nodex: allocated node is exclusive (lonodex)' '
    is_node_exclusive ${JOBID1} 3 &&
    is_node_exclusive ${JOBID2} 1
'

test_expect_success 'qmanager-nodex: free/alloc node count (lonodex)' '
    cat >status.expected3 <<-EOF &&
	2 88 8
	2 88 8
EOF
    flux resource list > resources.out3 &&
    cat resources.out3 | grep -E "(free|alloc)" \
        | awk "{ print \$2,\$3,\$4 }" > status.out3 &&
    test_cmp status.expected3 status.out3
'

test_expect_success 'qmanager-nodex: submit a -N2 -n2 jobs (lonodex)' '
    JOBID3=$(flux submit -N2 -n2 sleep inf) &&
    flux job wait-event -t 10 ${JOBID3} start
'

test_expect_success 'qmanager-nodex: allocated node is exclusive (lonodex)' '
    is_node_exclusive ${JOBID3} "0,2"
'

test_expect_success 'qmanager-nodex: free/alloc node count 2 (lonodex)' '
    cat >status.expected4 <<-EOF &&
	0 0 0
	4 176 16
EOF
    flux resource list > resources.out4 &&
    cat resources.out4 | grep -E "(free|alloc)" \
        | awk "{ print \$2,\$3,\$4 }" > status.out4 &&
    test_cmp status.expected4 status.out4
'

test_expect_success 'qmanager-nodex: cancel all jobs (lonodex)' '
    flux cancel --all &&
    flux queue drain
'

test_expect_success 'qmanager-nodex: removing fluxion modules (lonodex)' '
    remove_resource
'

test_done

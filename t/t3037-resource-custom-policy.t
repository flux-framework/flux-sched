#!/bin/sh

test_description='Test node-locality-aware scheduling'

. $(dirname $0)/sharness.sh

cmd_dir="${SHARNESS_TEST_SRCDIR}/data/resource/commands/nodex"
exp_dir="${SHARNESS_TEST_SRCDIR}/data/resource/expected/nodex"
grugs="${SHARNESS_TEST_SRCDIR}/data/resource/grugs/small.graphml"
query="../../resource/utilities/resource-query"

# Takes policy and cmd outfile prefix
run_tests_with_policy() {
    pol=$1
    prefix=$2

    cmds001="${cmd_dir}/cmds01.in"
    test001_desc="allocate 7 jobs with node-level constraint (pol=$pol)"
    test_expect_success "${test001_desc}" '
        sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds001} > cmds001 &&
        ${query} -L ${grugs} -S CA -P "${pol}" -t ${prefix}1.R.out < cmds001 &&
        test_cmp ${prefix}1.R.out ${exp_dir}/${prefix}1.R.out
    '

    cmds002="${cmd_dir}/cmds02.in"
    test002_desc="allocate 7 jobs with no node-level constraint (pol=$pol)"
    test_expect_success "${test002_desc}" '
        sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds002} > cmds002 &&
        ${query} -L ${grugs} -S CA -P "${pol}" -t ${prefix}2.R.out < cmds002 &&
        test_cmp ${prefix}2.R.out ${exp_dir}/${prefix}2.R.out
    '

    cmds003="${cmd_dir}/cmds03.in"
    test003_desc="match allocate 7 jobs -- last fails (pol=$pol)"
    test_expect_success "${test003_desc}" '
        sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds003} > cmds003 &&
        ${query} -L ${grugs} -S CA -P "${pol}" -t ${prefix}3.R.out < cmds003 &&
        test_cmp ${prefix}3.R.out ${exp_dir}/${prefix}3.R.out
    '
}

# Selection Policy -- High node first with node exclusivity (-P hinodex)
#     Selection behavior is identical to hinode except that
#     it marks each selected node as exclusive even if the
#     jobspec does not require node exclusivity and
#     that it selects and emits all of the node-local resources
#     for each node where at least one node-local resource is selected.
#
#     For a jobspec with node[1]->slot[1]->core[1], it selects
#     36 cores from the selected node if there is a total of
#     36 cores in that node.
#
#     For a jobspec with slot[18]->core[1], it selects
#     again all 36 cores from the current available highest node.
#

run_tests_with_policy "policy=high node_centric=true node_exclusive=true" 00

#
# Selection Policy -- Low node first with node exclusivity (-P lonodex)
#     Selection behavior is identical to lonode except that
#     it marks each selected node as exclusive even if the
#     jobspec does not require node exclusivity and
#     that it selects and emits all of the node-local resources
#     for each node where at least one node-local resource is selected.
#
#     For a jobspec with node[1]->slot[1]->core[1], it selects
#     36 cores from the selected node if there is a total of
#     36 cores in that node.
#
#     For a jobspec with slot[18]->core[1], it selects
#     again all 36 cores from the current available lowest node.
#

run_tests_with_policy "policy=low node_centric=true node_exclusive=true" 01

run_tests_with_policy "policy=high node_centric=true node_exclusive=true stop_on_1_matches=true" 00

test_done

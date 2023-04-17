#!/bin/sh

test_description='Test node-locality-aware scheduling'

. $(dirname $0)/sharness.sh

cmd_dir="${SHARNESS_TEST_SRCDIR}/data/resource/commands/node_locality"
exp_dir="${SHARNESS_TEST_SRCDIR}/data/resource/expected/node_locality"
grugs="${SHARNESS_TEST_SRCDIR}/data/resource/grugs/small.graphml"
query="../../resource/utilities/resource-query"

#
# Selection Policy -- High node first (-P hinode)
#     Within node-level locality constraint, the resource vertex
#     with higher ID is preferred among its kind
#     (e.g., core1 is preferred over core0 but those in node1
#            is selected first before those in node0 is selected)
#     If a node-level locality constraint is already specified in
#     the jobspec, this policy is identical to high: e.g.,
#         node[2]->slot[2]->core[2].
#     Only when node-level locality constraint is absent, the
#     match behavior is deviated from high: e.g.,
#         slot[2]->core[2].

cmds001="${cmd_dir}/cmds01.in"
test001_desc="match allocate 2 jobspecs with 2 slot: 18 core (pol=hinode)"
test_expect_success "${test001_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds001} > cmds001 &&
    ${query} -L ${grugs} -S CA -P hinode -t 001.R.out < cmds001 &&
    test_cmp 001.R.out ${exp_dir}/001.R.out
'

cmds002="${cmd_dir}/cmds02.in"
test002_desc="match allocate 2 jobspecs with 37 slot: 1 core (pol=hi)"
test_expect_success "${test002_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds002} > cmds002 &&
    ${query} -L ${grugs} -S CA -P hinode -t 002.R.out < cmds002 &&
    test_cmp 002.R.out ${exp_dir}/002.R.out
'

#
# Selection Policy -- Low node first (-P lonode)
#     Within node-level locality constraint, the resource vertex
#     with lower ID is preferred among its kind
#     (e.g., core0 is preferred over core1 but those in node0
#            is selected first before those in node1 is selected)
#     If a node-level locality constraint is already specified in
#     the jobspec, this policy is identical to low: e.g.,
#         node[2]->slot[2]->core[2].
#     Only when node-level locality constraint is absent, the
#     match behavior is deviated from low: e.g.,
#         slot[2]->core[2].

cmds011="${cmd_dir}/cmds01.in"
test011_desc="match allocate 2 jobspecs with 2 slot: 18 core (pol=lonode)"
test_expect_success "${test001_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds011} > cmds011 &&
    ${query} -L ${grugs} -S CA -P lonode -t 011.R.out < cmds011 &&
    test_cmp 011.R.out ${exp_dir}/011.R.out
'

cmds012="${cmd_dir}/cmds02.in"
test012_desc="match allocate 2 jobspecs with 37 slot: 1 core (pol=lonode)"
test_expect_success "${test012_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds012} > cmds012 &&
    ${query} -L ${grugs} -S CA -P lonode -t 012.R.out < cmds012 &&
    test_cmp 012.R.out ${exp_dir}/012.R.out
'

test_done

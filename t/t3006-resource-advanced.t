#!/bin/sh

test_description='Test advanced cases (Burst Buffers and Heterogenous)'

. $(dirname $0)/sharness.sh

cmd_dir="${SHARNESS_TEST_SRCDIR}/data/resource/commands/advanced"
exp_dir="${SHARNESS_TEST_SRCDIR}/data/resource/expected/advanced"
grugs="${SHARNESS_TEST_SRCDIR}/data/resource/grugs/advanced_test.graphml"
disag="${SHARNESS_TEST_SRCDIR}/data/resource/grugs/disaggr.graphml"
query="../../resource/utilities/resource-query"

#
# Selection Policy -- High ID first (-P high)
#     The resource vertex with higher ID is preferred among its kind
#     (e.g., node1 is preferred over node0 if available)
#

cmds001="${cmd_dir}/cmds01.in"
test001_desc="match allocate with advanced critieria including BB (pol=hi)"
test_expect_success "${test001_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds001} > cmds001 &&
    ${query} -G ${grugs} -S CA -P high -t 001.R.out < cmds001 &&
    test_cmp 001.R.out ${exp_dir}/001.R.out
'

#
# Selection Policy -- Low ID first (-P low)
#     The resource vertex with higher ID is preferred among its kind
#     (e.g., node0 is preferred over node1 if available)
#

cmds002="${cmd_dir}/cmds01.in"
test002_desc="match allocate with advanced critieria including BB (pol=low)"
test_expect_success "${test002_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds002} > cmds002 &&
    ${query} -G ${grugs} -S CA -P low -t 002.R.out < cmds002 &&
    test_cmp 002.R.out ${exp_dir}/002.R.out
'

#
# Selection Policy -- High ID first (-P high)
#     The resource vertex with higher ID is preferred among its kind
#     (e.g., node1 is preferred over node0 if available)
#
cmds003="${cmd_dir}/cmds02.in"
test003_desc="match for a large system with disaggregated resources (pol=high)"
test_expect_success LONGTEST "${test003_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds003} > cmds003 &&
    ${query} -G ${disag} -S CA -P high -t 003.R.out -r 400000< cmds003 &&
    test_cmp 003.R.out ${exp_dir}/003.R.out
'

#
# Selection Policy -- Low ID first (-P low)
#     The resource vertex with higher ID is preferred among its kind
#     (e.g., node0 is preferred over node1 if available)
#
cmds004="${cmd_dir}/cmds02.in"
test004_desc="match for a large system with disaggregated resources (pol=low)"
test_expect_success LONGTEST "${test004_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds004} > cmds004 &&
    ${query} -G ${disag} -S CA -P low -t 004.R.out -r 400000 < cmds004 &&
    test_cmp 004.R.out ${exp_dir}/004.R.out
'

test_done

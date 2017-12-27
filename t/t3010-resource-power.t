#!/bin/sh

test_description='Test simple power allocation cases'

. $(dirname $0)/sharness.sh

cmd_dir="${SHARNESS_TEST_SRCDIR}/data/resource/commands/power"
exp_dir="${SHARNESS_TEST_SRCDIR}/data/resource/expected/power"
grugs="${SHARNESS_TEST_SRCDIR}/data/resource/grugs/power.graphml"
query="../../resource/utilities/resource-query"

#
# Selection Policy -- High ID first (-P high)
#     The resource vertex with higher ID is preferred among its kind
#     (e.g., node1 is preferred over node0 if available)
#

cmds001="${cmd_dir}/cmds01.in"
test001_desc="match allocate with simple power configuration (pol=hi)"
test_expect_success "${test001_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds001} > cmds001 &&
    ${query} -G ${grugs} -S PA -P high -t 001.R.out < cmds001 &&
    test_cmp 001.R.out ${exp_dir}/001.R.out
'

#
# Selection Policy -- High ID first (-P low)
#     The resource vertex with higher ID is preferred among its kind
#     (e.g., node0 is preferred over node1 if available)
#

cmds002="${cmd_dir}/cmds01.in"
test002_desc="match allocate with simple power configuration (pol=low)"
test_expect_success "${test002_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds002} > cmds002 &&
    ${query} -G ${grugs} -S PA -P low -t 002.R.out < cmds002 &&
    test_cmp 002.R.out ${exp_dir}/002.R.out
'

test_done

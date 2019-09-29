#!/bin/sh

test_description='Test Scheduling with various prune filter configurations'

. $(dirname $0)/sharness.sh

cmd_dir="${SHARNESS_TEST_SRCDIR}/data/resource/commands/basics"
exp_dir="${SHARNESS_TEST_SRCDIR}/data/resource/expected/basics"
grugs="${SHARNESS_TEST_SRCDIR}/data/resource/grugs/tiny.graphml"
query="../../resource/utilities/resource-query"

#
# Selection Policy -- High ID first (-P high)
#     The resource vertex with higher ID is preferred among its kind
#     (e.g., node1 is preferred over node0 if available)
#

cmds003="${cmd_dir}/cmds03.in"
test003_desc="allocate_orelse_reserve 10 jobspecs works with default filter"
test_expect_success "${test003_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds003} > cmds003 &&
    ${query} -L ${grugs} -S CA -P high -t 003.R.out < cmds003 &&
    test_cmp 003.R.out ${exp_dir}/003.R.out
'

cmds003="${cmd_dir}/cmds03.in"
test003_desc="allocate_orelse_reserve 10 jobspecs works with no additional filter"
test_expect_success "${test003_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds003} > cmds003 &&
    ${query} -L ${grugs} -S CA -P high --prune-filters="" -t 003.R.out < cmds003 &&
    test_cmp 003.R.out ${exp_dir}/003.R.out
'

cmds003="${cmd_dir}/cmds03.in"
test003_desc="allocate_orelse_reserve 10 jobspecs works with ALL:core"
test_expect_success "${test003_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds003} > cmds003 &&
    ${query} -L ${grugs} -S CA -P high --prune-filters="ALL:core" -t 003.R.out < cmds003 &&
    test_cmp 003.R.out ${exp_dir}/003.R.out
'

cmds003="${cmd_dir}/cmds03.in"
test003_desc="allocate_orelse_reserve 10 jobspecs works with ALL:core,ALL:gpu"
test_expect_success "${test003_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds003} > cmds003 &&
    ${query} -L ${grugs} -S CA -P high --prune-filters="ALL:core,ALL:gpu" -t 003.R.out < cmds003 &&
    test_cmp 003.R.out ${exp_dir}/003.R.out
'

cmds003="${cmd_dir}/cmds03.in"
test003_desc="allocate_orelse_reserve 10 jobspecs works with ALL:core,ALL:gpu,ALL:memory"
test_expect_success "${test003_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds003} > cmds003 &&
    ${query} -L ${grugs} -S CA -P high --prune-filters="ALL:core,ALL:gpu" -t 003.R.out < cmds003 &&
    test_cmp 003.R.out ${exp_dir}/003.R.out
'

cmds005="${cmd_dir}/cmds05.in"
test005_desc="match allocate_orelse_reserve 100 jobspecs instead (pol=hi)"
test_expect_success "${test005_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds005} > cmds005 &&
    ${query} -L ${grugs} -S CA -P high -t 005.R.out < cmds005 &&
    test_cmp 005.R.out ${exp_dir}/005.R.out
'

cmds005="${cmd_dir}/cmds05.in"
test005_desc="match allocate_orelse_reserve 100 jobspecs with no additional filter"
test_expect_success "${test005_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds005} > cmds005 &&
    ${query} -L ${grugs} -S CA -P high --prune-filters="" -t 005.R.out < cmds005 &&
    test_cmp 005.R.out ${exp_dir}/005.R.out
'

cmds005="${cmd_dir}/cmds05.in"
test005_desc="match allocate_orelse_reserve 100 jobspecs with ALL:core"
test_expect_success "${test005_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds005} > cmds005 &&
    ${query} -L ${grugs} -S CA -P high --prune-filters="ALL:core" -t 005.R.out < cmds005 &&
    test_cmp 005.R.out ${exp_dir}/005.R.out
'

cmds005="${cmd_dir}/cmds05.in"
test005_desc="match allocate_orelse_reserve 100 jobspecs with ALL:core,ALL:gpu"
test_expect_success "${test005_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds005} > cmds005 &&
    ${query} -L ${grugs} -S CA -P high --prune-filters="ALL:core,ALL:gpu" -t 005.R.out < cmds005 &&
    test_cmp 005.R.out ${exp_dir}/005.R.out
'

cmds005="${cmd_dir}/cmds05.in"
test005_desc="match allocate_orelse_reserve 100 jobspecs with ALL:core,ALL:gpu,ALL:memory"
test_expect_success "${test005_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds005} > cmds005 &&
    ${query} -L ${grugs} -S CA -P high --prune-filters="ALL:core,ALL:gpu,ALL:memory" -t 005.R.out < cmds005 &&
    test_cmp 005.R.out ${exp_dir}/005.R.out
'

test_done

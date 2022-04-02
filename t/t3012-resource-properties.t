#!/bin/sh
# set -x 

test_description='Test Set Property On Tiny Machine Configuration'

. $(dirname $0)/sharness.sh

cmd_dir="${SHARNESS_TEST_SRCDIR}/data/resource/commands/resource_property"
exp_dir="${SHARNESS_TEST_SRCDIR}/data/resource/expected/resource_property"
grugs="${SHARNESS_TEST_SRCDIR}/data/resource/grugs/tiny.graphml"
query="../../resource/utilities/resource-query"

filter_run_variations(){
   sed "s/ (vtx's uniq_id=[[:digit:]])//g" ${1}
}

#
# Selection Policy -- High ID first (-P high)
#     The resource vertex with higher ID is preferred among its kind
#     (e.g., node1 is preferred over node0 if available)
#

cmds001="${cmd_dir}/cmds01.in"
test001_desc="set-property and get-property on the node type resource"
test_expect_success "${test001_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds001} > cmds001 &&
    ${query} -L ${grugs} -S CA -P high -t 001.R.out < cmds001 &&
    test_cmp 001.R.out ${exp_dir}/001.R.out
'

cmds002="${cmd_dir}/cmds02.in"
test002_desc="set-property and get-property on the other resources"
test_expect_success "${test002_desc}" '
   sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds002} > cmds002 &&
   ${query} -L ${grugs} -S CA -P high -t 002.R.out < cmds002 &&
   test_cmp 002.R.out ${exp_dir}/002.R.out
'

cmds003="${cmd_dir}/cmds03.in"
test003_desc="test get-property without setting any properties"
test_expect_success "${test003_desc}" '
   sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds003} > cmds003 &&
   ${query} -L ${grugs} -S CA -P high -t 003.R.out < cmds003 &&
   filter_run_variations 003.R.out > 003.R.out.filt &&
   test_cmp 003.R.out.filt ${exp_dir}/003.R.out
'

cmds004="${cmd_dir}/cmds04.in"
test004_desc="test multiple properties for same resource"
test_expect_success "${test004_desc}" '
   sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds004} > cmds004 &&
   ${query} -L ${grugs} -S CA -P high -t 004.R.out < cmds004 &&
   test_cmp 004.R.out ${exp_dir}/004.R.out
'

cmds005="${cmd_dir}/cmds05.in"
test005_desc="test inserting same property key multiple times"
test_expect_success "${test005_desc}" '
   sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds005} > cmds005 &&
   ${query} -L ${grugs} -S CA -P high -t 005.R.out < cmds005 &&
   test_cmp 005.R.out ${exp_dir}/005.R.out
'

cmds006="${cmd_dir}/cmds06.in"
test006_desc="test incorrect inputs to set-property"
test_expect_success "${test006_desc}" '
   sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds006} > cmds006 &&
   ${query} -L ${grugs} -S CA -P high -t 006.R.out < cmds006 &&
   filter_run_variations 006.R.out > 006.R.out.filt &&
   test_cmp 006.R.out.filt ${exp_dir}/006.R.out
'

test_done

#!/bin/sh

test_description='Test resource graph remove subgraph'

. $(dirname $0)/sharness.sh

cmd_dir="${SHARNESS_TEST_SRCDIR}/data/resource/commands/rq2"
exp_dir="${SHARNESS_TEST_SRCDIR}/data/resource/expected/rq2"
jgf="${SHARNESS_TEST_SRCDIR}/data/resource/jgfs/tiny.json"
query="../../resource/utilities/rq2"

cmds001="${cmd_dir}/cmds01.in"
test001_desc="match allocate test"
test_expect_success "${test001_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds001} > cmds001 &&
    ${query} -L ${jgf} -f jgf -t 001.R.out \
    < cmds001 &&
    test_cmp 001.R.out ${exp_dir}/001.R.out
'

cmds002="${cmd_dir}/cmds02.in"
test002_desc="match allocate_orelse_reserve test"
test_expect_success "${test002_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds002} > cmds002 &&
    ${query} -L ${jgf} -f jgf -t 002.R.out \
    < cmds002 &&
    test_cmp 002.R.out ${exp_dir}/002.R.out
'

cmds003="${cmd_dir}/cmds03.in"
test003_desc="match allocate match options test"
test_expect_success "${test003_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds003} > cmds003 &&
    ${query} -L ${jgf} -f jgf -t 003.R.out \
    < cmds003 &&
    test_cmp 003.R.out ${exp_dir}/003.R.out
'

cmds004="${cmd_dir}/cmds04.in"
test004_desc="rq2 info, find, and cancel test"
test_expect_success "${test004_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds004} > cmds004 &&
    ${query} -L ${jgf} -f jgf -t 004.R.out \
    < cmds004 &&
    test_cmp 004.R.out ${exp_dir}/004.R.out
'


test_done

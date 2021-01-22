#!/bin/sh

test_description='Test resource graph growth'

. $(dirname $0)/sharness.sh

cmd_dir="${SHARNESS_TEST_SRCDIR}/data/resource/commands/elastic"
exp_dir="${SHARNESS_TEST_SRCDIR}/data/resource/expected/elastic"
jgf="${SHARNESS_TEST_SRCDIR}/data/resource/jgfs/elastic/node-test.json"
query="../../resource/utilities/resource-query"

cmds001="${cmd_dir}/cmds01.in"
test001_desc="fully allocate node and grow job with new resources"
test_expect_success "${test001_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds001} > cmds001 &&
    ${query} -L ${jgf} -F jgf -f jgf -S CA -P low -t 001.R.out -r 2000 \
    < cmds001 &&
    test_cmp 001.R.out ${exp_dir}/001.R.out
'

cmds002="${cmd_dir}/cmds02.in"
test002_desc="fully allocate node and grow job from randomized JGF"
test_expect_success "${test002_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds002} > cmds002 &&
    ${query} -L ${jgf} -F jgf -f jgf -S CA -P low -t 002.R.out -r 2000 \
    < cmds002 &&
    test_cmp 002.R.out ${exp_dir}/002.R.out
'

cmds003="${cmd_dir}/cmds03.in"
test003_desc="ensure attach with allocated vertices can't change allocations"
test_expect_success "${test003_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds003} > cmds003 &&
    ${query} -L ${jgf} -F jgf -f jgf -S CA -P low -t 003.R.out -r 2000 \
    < cmds003 2> 003.R.err &&
    test_cmp 003.R.out ${exp_dir}/003.R.out &&
    test_cmp 003.R.err ${exp_dir}/003.R.err
'

cmds004="${cmd_dir}/cmds04.in"
test004_desc="can't grow with a different root in the subgraph"
test_expect_success "${test004_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds004} > cmds004 &&
    ${query} -L ${jgf} -F jgf -f jgf -S CA -P low -t 004.R.out -r 2000 \
    < cmds004 2> 004.R.err &&
    test_cmp 004.R.out ${exp_dir}/004.R.out &&
    test_cmp 004.R.err ${exp_dir}/004.R.err
'

cmds005="${cmd_dir}/cmds05.in"
test005_desc="grow with subset of resource graph doesn't change resource graph"
test_expect_success "${test005_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds005} > cmds005 &&
    ${query} -L ${jgf} -F jgf -f jgf -S CA -P low -t 005.R.out -r 2000 \
    < cmds005 &&
    test_cmp 005.R.out ${exp_dir}/005.R.out
'

cmds006="${cmd_dir}/cmds06.in"
test006_desc="error on invalid argument"
test_expect_success "${test006_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds006} > cmds006 &&
    ${query} -L ${jgf} -F jgf -f jgf -S CA -P low -t 006.R.out \
    < cmds006 2> 006.R.err &&
    test_cmp 006.R.out ${exp_dir}/006.R.out &&
    test_cmp 006.R.err ${exp_dir}/006.R.err
'

cmds007="${cmd_dir}/cmds07.in"
err007="${exp_dir}/007.R.err"
test007_desc="error on nonexistent JGF"
test_expect_success "${test007_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds007} > cmds007 &&
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${err007} > 007.R.err &&
    ${query} -L ${jgf} -F jgf -f jgf -S CA -P low -t 007.R.out \
    < cmds007 2> 007.R.new_err &&
    test_cmp 007.R.out ${exp_dir}/007.R.out &&
    test_cmp 007.R.new_err 007.R.err
'

test_done

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
    ${query} -L ${jgf} -F jgf -f jgf -S CA -P low -t 001.R.out \
    < cmds001 &&
    test_cmp 001.R.out ${exp_dir}/001.R.out
'

cmds002="${cmd_dir}/cmds02.in"
test002_desc="fully allocate node and grow job from randomized JGF"
test_expect_success "${test002_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds002} > cmds002 &&
    ${query} -L ${jgf} -F jgf -f jgf -S CA -P low -t 002.R.out \
    < cmds002 &&
    test_cmp 002.R.out ${exp_dir}/002.R.out
'

cmds003="${cmd_dir}/cmds03.in"
test003_desc="attach with allocated vertices doesn't affect planner"
test_expect_success "${test003_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds003} > cmds003 &&
    ${query} -L ${jgf} -F jgf -f jgf -S CA -P low -t 003.R.out \
    < cmds003 &&
    test_cmp 003.R.out ${exp_dir}/003.R.out
'

cmds004="${cmd_dir}/cmds04.in"
test004_desc="attach with allocated, reserved vertices doesn't affect planner"
test_expect_success "${test004_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds004} > cmds004 &&
    ${query} -L ${jgf} -F jgf -f jgf -S CA -P low -t 004.R.out \
    < cmds004 &&
    test_cmp 004.R.out ${exp_dir}/004.R.out
'

cmds005="${cmd_dir}/cmds05.in"
test005_desc="can't grow with a different root in the subgraph"
test_expect_success "${test005_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds005} > cmds005 &&
    ${query} -L ${jgf} -F jgf -f jgf -S CA -P low -t 005.R.out \
    < cmds005 2> 005.R.err &&
    test_cmp 005.R.out ${exp_dir}/005.R.out &&
    test_cmp 005.R.err ${exp_dir}/005.R.err
'

cmds006="${cmd_dir}/cmds06.in"
test006_desc="grow with subset of resource graph doesn't change resource graph"
test_expect_success "${test006_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds006} > cmds006 &&
    ${query} -L ${jgf} -F jgf -f jgf -S CA -P low -t 006.R.out \
    < cmds006 &&
    test_cmp 006.R.out ${exp_dir}/006.R.out
'

cmds007="${cmd_dir}/cmds07.in"
test007_desc="error on invalid argument"
test_expect_success "${test007_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds007} > cmds007 &&
    ${query} -L ${jgf} -F jgf -f jgf -S CA -P low -t 007.R.out \
    < cmds007 2> 007.R.err &&
    test_cmp 007.R.out ${exp_dir}/007.R.out &&
    test_cmp 007.R.err ${exp_dir}/007.R.err
'

cmds008="${cmd_dir}/cmds08.in"
err008="${exp_dir}/008.R.err"
test008_desc="error on nonexistent JGF"
test_expect_success "${test008_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds008} > cmds008 &&
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${err008} > 008.R.err &&
    ${query} -L ${jgf} -F jgf -f jgf -S CA -P low -t 008.R.out \
    < cmds008 2> 008.R.new_err &&
    test_cmp 008.R.out ${exp_dir}/008.R.out &&
    test_cmp 008.R.new_err 008.R.err
'

cmds009="${cmd_dir}/cmds09.in"
test009_desc="ensure proper resource graph reinitialization"
test_expect_success "${test009_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds009} > cmds009 &&
    ${query} -L ${jgf} -F jgf -f jgf -S CA -P low -t 009.R.out \
    < cmds009 &&
    test_cmp 009.R.out ${exp_dir}/009.R.out
'

cmds010="${cmd_dir}/cmds10.in"
test010_desc="grow existing job to occupy all resources; can't occupy more than exist"
test_expect_success "${test010_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds010} > cmds010 &&
    ${query} -L ${jgf} -F jgf -f jgf -S CA -P low -t 010.R.out \
    < cmds010 &&
    test_cmp 010.R.out ${exp_dir}/010.R.out
'

cmds011="${cmd_dir}/cmds11.in"
test011_desc="grow existing job four times, grow graph and grow allocation onto new resources; can't occupy more than exist"
test_expect_success "${test010_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds011} > cmds011 &&
    ${query} -L ${jgf} -F jgf -f jgf -S CA -P low -t 011.R.out \
    < cmds011 &&
    test_cmp 011.R.out ${exp_dir}/011.R.out
'

test_done

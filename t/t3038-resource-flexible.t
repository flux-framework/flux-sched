#!/bin/sh

test_description='Test Flexible Scheduling On Medium Machine Configuration in JGF'

. $(dirname $0)/sharness.sh

cmd_dir="${SHARNESS_TEST_SRCDIR}/data/resource/commands/flexible"
exp_dir="${SHARNESS_TEST_SRCDIR}/data/resource/expected/flexible"
jgf="${SHARNESS_TEST_SRCDIR}/data/resource/jgfs/tiny.json"
query="../../resource/utilities/resource-query"

cmds001="${cmd_dir}/cmds01.in"
test001_desc="JGF: allocate 9 jobspecs with flexible scheduling"
test_expect_success "${test001_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds001} > cmds001 &&
    ${query} -L ${jgf} -f jgf -S CA -P high -T flexible -t 001.R.out < cmds001 &&
    test_cmp 001.R.out ${exp_dir}/001.R.out
'

cmds002="${cmd_dir}/cmds02.in"
test002_desc="JGF: allocate jobspecs with valid and invalid or_slots"
test_expect_success "${test002_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds002} > cmds002 &&
    ${query} -L ${jgf} -f jgf -S CA -P high -T flexible -t 002.R.out < cmds002 &&
    test_cmp 002.R.out ${exp_dir}/002.R.out
'

cmds003="${cmd_dir}/cmds03.in"
test003_desc="JGF: allocate jobspecs with pristine jobspec"
test_expect_success "${test003_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds003} > cmds003 &&
    ${query} -L ${jgf} -f jgf -S CA -P high -T flexible -t 003.R.out < cmds003 &&
    test_cmp 003.R.out ${exp_dir}/003.R.out
'

cmds004="${cmd_dir}/cmds04.in"
test004_desc="JGF: try to allocate jobspec with single slot"
test_expect_success "${test004_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds004} > cmds004 &&
    ${query} -L ${jgf} -f jgf -S CA -P high -T flexible -t 004.R.out < cmds004 &&
    test_cmp 004.R.out ${exp_dir}/004.R.out
'

cmds005="${cmd_dir}/cmds05.in"
test005_desc="JGF: try to allocate jobspec with or_slot count > 1"
test_expect_success "${test005_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds005} > cmds005 &&
    ${query} -L ${jgf} -f jgf -S CA -P high -T flexible -t 005.R.out < cmds005 &&
    test_cmp 005.R.out ${exp_dir}/005.R.out
'

cmds006="${cmd_dir}/cmds06.in"
test006_desc="JGF: try to allocate pristine jobspec with resources that don't exist"
test_expect_success "${test006_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds006} > cmds006 &&
    ${query} -L ${jgf} -f jgf -S CA -P high -T flexible < cmds006 2>  006.R.out &&
    test_cmp 006.R.out ${exp_dir}/006.R.out
'

cmds007="${cmd_dir}/cmds07.in"
test007_desc="JGF: try to allocate jobspec with resource and or_slot siblings"
test_expect_success "${test007_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds007} > cmds007 &&
    ${query} -L ${jgf} -f jgf -S CA -P high -T flexible -t 007.R.out < cmds007 &&
    test_cmp 007.R.out ${exp_dir}/007.R.out
'

cmds008="${cmd_dir}/cmds08.in"
test008_desc="JGF: try to allocate jobspec with identical or_slot configurations"
test_expect_success "${test008_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds008} > cmds008 &&
    ${query} -L ${jgf} -f jgf -S CA -P high -T flexible -t 008.R.out < cmds008 &&
    test_cmp 008.R.out ${exp_dir}/008.R.out
'

cmds009="${cmd_dir}/cmds09.in"
test009_desc="JGF: allocate 9 jobspecs with flexible scheduling using first match"
test_expect_success "${test009_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds009} > cmds009 &&
    ${query} -L ${jgf} -f jgf -S CA -P first -T flexible -t 009.R.out < cmds009 &&
    test_cmp 009.R.out ${exp_dir}/009.R.out
'

cmds010="${cmd_dir}/cmds10.in"
test010_desc="JGF: allocate pristine jobspec with xor slots"
test_expect_success "${test009_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds010} > cmds010 &&
    ${query} -L ${jgf} -f jgf -S CA -P first -T flexible -t 010.R.out < cmds010 &&
    test_cmp 010.R.out ${exp_dir}/010.R.out
'

cmds011="${cmd_dir}/cmds11.in"
test011_desc="JGF: allocate_orelse_reserve pristine jobspec with xor slots"
test_expect_success "${test011_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds011} > cmds011 &&
    ${query} -L ${jgf} -f jgf -S CA -P first -T flexible -t 011.R.out < cmds011 &&
    test_cmp 011.R.out ${exp_dir}/011.R.out
'

cmds012="${cmd_dir}/cmds12.in"
test012_desc="JGF: allocate jobspecs with xor slots"
test_expect_success "${test012_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds012} > cmds012 &&
    ${query} -L ${jgf} -f jgf -S CA -P first -T flexible -t 012.R.out < cmds012 &&
    test_cmp 012.R.out ${exp_dir}/012.R.out
'

test_done

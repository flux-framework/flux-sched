#!/bin/sh

test_description='Test resource graph remove subgraph'

. $(dirname $0)/sharness.sh

cmd_dir="${SHARNESS_TEST_SRCDIR}/data/resource/commands/remove"
exp_dir="${SHARNESS_TEST_SRCDIR}/data/resource/expected/remove"
jgf="${SHARNESS_TEST_SRCDIR}/data/resource/jgfs/tiny.json"
jgf_ranks="${SHARNESS_TEST_SRCDIR}/data/resource/jgfs/elastic/tiny-partial-cancel.json"
query="../../resource/utilities/resource-query"

cmds001="${cmd_dir}/cmds01.in"
test001_desc="remove a single socket"
test_expect_success "${test001_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds001} > cmds001 &&
    ${query} -L ${jgf} -f jgf -t 001.R.out \
    < cmds001 &&
    test_cmp 001.R.out ${exp_dir}/001.R.out
'

cmds002="${cmd_dir}/cmds02.in"
test002_desc="remove several cpus"
test_expect_success "${test002_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds002} > cmds002 &&
    ${query} -L ${jgf} -f jgf -t 002.R.out \
    < cmds002 &&
    test_cmp 002.R.out ${exp_dir}/002.R.out
'

cmds003="${cmd_dir}/cmds03.in"
test003_desc="remove non existent resource"
test_expect_success "${test003_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds003} > cmds003 &&
    ${query} -L ${jgf} -f jgf -t 003.R.out \
    < cmds003 &&
    test_cmp 003.R.out ${exp_dir}/003.R.out
'

cmds004="${cmd_dir}/cmds04.in"
test004_desc="remove resources while allocation exists"
test_expect_success "${test004_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds004} > cmds004 &&
    ${query} -L ${jgf} -f jgf -t 004.R.out \
    < cmds004 &&
    test_cmp 004.R.out ${exp_dir}/004.R.out
'

cmds005="${cmd_dir}/cmds05.in"
test005_desc="remove resources while allocation exists"
test_expect_success "${test005_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds005} > cmds005 &&
    ${query} -L ${jgf} -f jgf -t 005.R.out -P low \
    < cmds005 &&
    test_cmp 005.R.out ${exp_dir}/005.R.out
'

cmds006="${cmd_dir}/cmds06.in"
test006_desc="remove resources then fully allocate remaining graph"
test_expect_success "${test006_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds006} > cmds006 &&
    ${query} -L ${jgf} -f jgf -t 006.R.out -P low \
    < cmds006 &&
    test_cmp 006.R.out ${exp_dir}/006.R.out
'

cmds007="${cmd_dir}/cmds07.in"
test007_desc="alloc resources, remove resources, then fully allocate remaining graph"
test_expect_success "${test007_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds007} > cmds007 &&
    ${query} -L ${jgf} -f jgf -F jgf -t 007.R.out -P low --prune-filters="ALL:core,ALL:gpu,ALL:node,ALL:memory" \
    < cmds007 &&
    test_cmp 007.R.out ${exp_dir}/007.R.out
'

cmds008="${cmd_dir}/cmds08.in"
test008_desc="fully alloc resources, remove resources, then fully allocate remaining graph with ranks in JGF"
test_expect_success "${test008_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds008} > cmds008 &&
    ${query} -L ${jgf_ranks} -f jgf -F jgf -t 008.R.out -P low --prune-filters="ALL:core,ALL:gpu,ALL:node,ALL:memory" \
    < cmds008 &&
    test_cmp 008.R.out ${exp_dir}/008.R.out
'

cmds009="${cmd_dir}/cmds09.in"
test009_desc="partial alloc resources, remove resources, then fully allocate remaining graph with ranks in JGF"
test_expect_success "${test009_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds009} > cmds009 &&
    ${query} -L ${jgf_ranks} -f jgf -F jgf -t 009.R.out -P low --prune-filters="ALL:core,ALL:gpu,ALL:node,ALL:memory" \
    < cmds009 &&
    test_cmp 009.R.out ${exp_dir}/009.R.out
'

cmds010="${cmd_dir}/cmds10.in"
test010_desc="partial alloc resources, remove resources, then fully allocate remaining graph with ranks in JGF; ALL:core"
test_expect_success "${test010_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds010} > cmds010 &&
    ${query} -L ${jgf_ranks} -f jgf -F jgf -t 010.R.out -P low --prune-filters="ALL:core" \
    < cmds010 &&
    test_cmp 010.R.out ${exp_dir}/010.R.out
'

cmds011="${cmd_dir}/cmds11.in"
test011_desc="remove rank without allocations and then allocate remaining graph"
test_expect_success "${test011_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds011} > cmds011 &&
    ${query} -L ${jgf_ranks} -f jgf -F jgf -t 011.R.out -P low --prune-filters="ALL:core,ALL:gpu,ALL:node,ALL:memory" \
    < cmds011 &&
    test_cmp 011.R.out ${exp_dir}/011.R.out
'

test_done

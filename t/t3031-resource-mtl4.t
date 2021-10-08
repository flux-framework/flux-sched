#!/bin/sh

test_description='Test multi-tiered storage level4: mtl3 + lustre constraints'

. $(dirname $0)/sharness.sh

cmd_dir="${SHARNESS_TEST_SRCDIR}/data/resource/commands/mt-storage/L4"
exp_dir="${SHARNESS_TEST_SRCDIR}/data/resource/expected/mt-storage/L4"
grug_dir="${SHARNESS_TEST_SRCDIR}/data/resource/grugs/mt-storage"
big_grug="${grug_dir}/mtl4-big.graphml"
minimal_grug="${grug_dir}/mtl4-minimal.graphml"
query="../../resource/utilities/resource-query"

cmds001="${cmd_dir}/cmds01.in"
test001_desc="match allocate works on an oversized system"
test_expect_success "${test001_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds001} > cmds001 &&
    ${query} -L ${big_grug} -S CA -P first -t 001.R.out < cmds001 &&
    test_cmp 001.R.out ${exp_dir}/001.R.out
'

test002_desc="match allocate works on a minimally sized system"
test_expect_success "${test002_desc}" '
    ${query} -L ${minimal_grug} -S CA -P first -t 002.R.out < cmds001 &&
    test_cmp 002.R.out ${exp_dir}/002.R.out
'

test_done

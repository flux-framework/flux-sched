#!/bin/sh

test_description='Test multi-tiered storage level0: ssd built into node'

. $(dirname $0)/sharness.sh

cmd_dir="${SHARNESS_TEST_SRCDIR}/data/resource/commands/mt-storage/L0"
exp_dir="${SHARNESS_TEST_SRCDIR}/data/resource/expected/mt-storage/L0"
grugs="${SHARNESS_TEST_SRCDIR}/data/resource/grugs/mt-storage/mtl0.graphml"
query="../../resource/utilities/resource-query"

cmds001="${cmd_dir}/cmds01.in"
test001_desc="match allocate 9 jobs with 1TB L0 ssds - last one fails (pol=hi)"
test_expect_success "${test001_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds001} > cmds001 &&
    ${query} -L ${grugs} -S CA -P high -t 001.R.out < cmds001 &&
    test_cmp 001.R.out ${exp_dir}/001.R.out
'

cmds002="${cmd_dir}/cmds02.in"
test002_desc="match allocate_orelse_reserve works with MTS L0 (pol=hi)"
test_expect_success "${test002_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds002} > cmds002 &&
    ${query} -L ${grugs} -S CA -P high -t 002.R.out < cmds002 &&
    test_cmp 002.R.out ${exp_dir}/002.R.out
'

cmds003="${cmd_dir}/cmds01.in"
test003_desc="match allocate 9 jobs with 1TB L0 ssds - last one fails (pol=low)"
test_expect_success "${test003_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds003} > cmds003 &&
    ${query} -L ${grugs} -S CA -P low -t 003.R.out < cmds003 &&
    test_cmp 003.R.out ${exp_dir}/003.R.out
'

cmds004="${cmd_dir}/cmds02.in"
test004_desc="match allocate_orelse_reserve works with MTS L0 (pol=low)"
test_expect_success "${test004_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds004} > cmds004 &&
    ${query} -L ${grugs} -S CA -P low -t 004.R.out < cmds004 &&
    test_cmp 004.R.out ${exp_dir}/004.R.out
'

cmds005="${cmd_dir}/cmds-ssd-constrained-alloc.in"
test005_desc="match allocate 5 jobs with 2TB L0 ssds - last one fails (pol=hi)"
test_expect_success "${test005_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds005} > cmds005 &&
    ${query} -L ${grugs} -S CA -P high -t 005.R.out < cmds005 &&
    test_cmp 005.R.out ${exp_dir}/005.R.out
'

cmds006="${cmd_dir}/cmds-ssd-constrained-alloc-orelse.in"
test006_desc="match allocate_orelse_reserve works with MTS L0 (pol=hi)"
test_expect_success "${test006_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds006} > cmds006 &&
    ${query} -L ${grugs} -S CA -P high -t 006.R.out < cmds006 &&
    test_cmp 006.R.out ${exp_dir}/006.R.out
'

cmds007="${cmd_dir}/cmds-ssd-constrained-alloc.in"
test007_desc="match allocate 5 jobs with 2TB L0 ssds - last one fails (pol=low)"
test_expect_success "${test007_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds007} > cmds007 &&
    ${query} -L ${grugs} -S CA -P low -t 007.R.out < cmds007 &&
    test_cmp 007.R.out ${exp_dir}/007.R.out
'

cmds008="${cmd_dir}/cmds-ssd-constrained-alloc-orelse.in"
test008_desc="match allocate_orelse_reserve works with MTS L0 (pol=low)"
test_expect_success "${test008_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds008} > cmds008 &&
    ${query} -L ${grugs} -S CA -P low -t 008.R.out < cmds008 &&
    test_cmp 008.R.out ${exp_dir}/008.R.out
'

test_done

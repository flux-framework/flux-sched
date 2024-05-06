#!/bin/sh

test_description='Test Scheduling Correctness on Vertex Granularity'

. $(dirname $0)/sharness.sh

cmd_dir="${SHARNESS_TEST_SRCDIR}/data/resource/commands/sibling"
exp_dir="${SHARNESS_TEST_SRCDIR}/data/resource/expected/sibling"
xml="${SHARNESS_TEST_SRCDIR}/data/hwloc-data/001N/amd_gpu/corona11.xml"
query="../../src/resource/utilities/resource-query"

cmds001="${cmd_dir}/cmds01.in"
test001_desc="sibling (under slot) each requesting 1 socket cannot be matched"
test_expect_success "${test001_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds001} > cmds001 &&
    ${query} -L ${xml} -f hwloc -W node,socket,core -S CA -P high \
    -t 001.R.out < cmds001 &&
    test_cmp 001.R.out ${exp_dir}/001.R.out
'

cmds002="${cmd_dir}/cmds02.in"
test002_desc="sibling each requesting 1 socket cannot be matched"
test_expect_success "${test002_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds002} > cmds002 &&
    ${query} -L ${xml} -f hwloc -W node,socket,core -S CA -P high \
    -t 002.R.out < cmds002 &&
    test_cmp 002.R.out ${exp_dir}/002.R.out
'

cmds003="${cmd_dir}/cmds03.in"
test003_desc="sibling each requesting 1 core cannot be matched"
test_expect_success "${test003_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds003} > cmds003 &&
    ${query} -L ${xml} -f hwloc -W node,socket,core -S CA -P high \
    -t 003.R.out < cmds003 &&
    test_cmp 003.R.out ${exp_dir}/003.R.out
'

test_done

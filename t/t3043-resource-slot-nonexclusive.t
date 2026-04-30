#!/bin/sh

test_description='Test that resources with explicit exclusive:false under slots can be shared'

. $(dirname $0)/sharness.sh

cmd_dir="${SHARNESS_TEST_SRCDIR}/data/resource/commands/slot-nonexclusive"
exp_dir="${SHARNESS_TEST_SRCDIR}/data/resource/expected/slot-nonexclusive"
jgf="${SHARNESS_TEST_SRCDIR}/data/resource/jgfs/rabbit.json"
query="../../resource/utilities/resource-query"

#
# Test that SSDs with explicit exclusive:false can be matched
#

cmds001="${cmd_dir}/cmds-ssd-shared-test.in"
test001_desc="match shared SSDs under slot multiple times"
test_expect_success "${test001_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds001} > cmds001 &&
    ${query} -L ${jgf} -f jgf -t 001.R.out < cmds001 &&
    test_cmp 001.R.out ${exp_dir}/ssd-shared-test.R.out
'

#
# Test that exclusive SSDs limit allocations
#

cmds002="${cmd_dir}/cmds-ssd-test.in"
test002_desc="exclusive SSDs limit allocations to available chassis"
test_expect_success "${test002_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds002} > cmds002 &&
    ${query} -L ${jgf} -f jgf -t 002.R.out < cmds002 &&
    test_cmp 002.R.out ${exp_dir}/ssd-test.R.out
'

#
# Test allocation with exact SSD count request (chassis with exactly 2 SSDs, exclusive: false)
#

cmds003="${cmd_dir}/cmds03.in"
test003_desc="allocate chassis with exact SSD count (shared)"
test_expect_success "${test003_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds003} > cmds003 &&
    ${query} -L ${jgf} -f jgf -t 003.R.out < cmds003 &&
    test_cmp 003.R.out ${exp_dir}/03.R.out
'

#
# Test impossible request (999 SSDs)
#

cmds004="${cmd_dir}/cmds04.in"
test004_desc="impossible SSD request fails immediately"
test_expect_success "${test004_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds004} > cmds004 &&
    ${query} -L ${jgf} -f jgf -t 004.R.out < cmds004 &&
    test_cmp 004.R.out ${exp_dir}/04.R.out
'

#
# Test ssd-limit-test-exclusive (should allocate only 2 due to chassis limit)
#

cmds005="${cmd_dir}/cmds05.in"
test005_desc="exclusive SSDs: chassis limit determines max allocations"
test_expect_success "${test005_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds005} > cmds005 &&
    ${query} -L ${jgf} -f jgf -t 005.R.out < cmds005 &&
    test_cmp 005.R.out ${exp_dir}/05.R.out
'

#
# Test ssd-limit-test-shared (should also allocate only 2 due to chassis limit)
#

cmds006="${cmd_dir}/cmds06.in"
test006_desc="shared SSDs: chassis limit still constrains allocations"
test_expect_success "${test006_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds006} > cmds006 &&
    ${query} -L ${jgf} -f jgf -t 006.R.out < cmds006 &&
    test_cmp 006.R.out ${exp_dir}/06.R.out
'

#
# Test mixed resources under slot (cores, GPU, memory with exclusive: false)
#

cmds007="${cmd_dir}/cmds07.in"
test007_desc="multiple shared resource types under slot"
test_expect_success "${test007_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds007} > cmds007 &&
    ${query} -L ${jgf} -f jgf -t 007.R.out < cmds007 &&
    test_cmp 007.R.out ${exp_dir}/07.R.out
'

#
# Test shared GPU under slot
#

cmds008="${cmd_dir}/cmds08.in"
test008_desc="shared GPU under slot allows multiple allocations"
test_expect_success "${test008_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds008} > cmds008 &&
    ${query} -L ${jgf} -f jgf -t 008.R.out < cmds008 &&
    test_cmp 008.R.out ${exp_dir}/08.R.out
'

#
# Test cores under slot (implicit exclusive)
#

cmds009="${cmd_dir}/cmds09.in"
test009_desc="cores under slot remain exclusive by default"
test_expect_success "${test009_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds009} > cmds009 &&
    ${query} -L ${jgf} -f jgf -t 009.R.out < cmds009 &&
    test_cmp 009.R.out ${exp_dir}/09.R.out
'

#
# Test shared GPU without slot specification
#

cmds010="${cmd_dir}/cmds10.in"
test010_desc="shared GPU without slot requires task.slot field"
test_expect_success "${test010_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds010} > cmds010 &&
    ${query} -L ${jgf} -f jgf -t 010.R.out < cmds010 &&
    test_cmp 010.R.out ${exp_dir}/10.R.out
'

#
# Test exclusive SSDs with chassis exhaustion (using ssd-limit-test-exclusive.yaml)
#

cmds011="${cmd_dir}/cmds11.in"
test011_desc="exclusive SSDs limited by chassis availability"
test_expect_success "${test011_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds011} > cmds011 &&
    ${query} -L ${jgf} -f jgf -t 011.R.out < cmds011 &&
    test_cmp 011.R.out ${exp_dir}/11.R.out
'

#
# Test shared SSDs with chassis exhaustion (using ssd-limit-test-shared.yaml)
#

cmds012="${cmd_dir}/cmds12.in"
test012_desc="shared SSDs limited by chassis, not SSD exhaustion"
test_expect_success "${test012_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds012} > cmds012 &&
    ${query} -L ${jgf} -f jgf -t 012.R.out < cmds012 &&
    test_cmp 012.R.out ${exp_dir}/12.R.out
'

test_done

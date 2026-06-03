#!/bin/sh

test_description='Test that resources with explicit exclusive:false under slots can be shared'

. $(dirname $0)/sharness.sh

cmd_dir="${SHARNESS_TEST_SRCDIR}/data/resource/commands/slot-nonexclusive"
exp_dir="${SHARNESS_TEST_SRCDIR}/data/resource/expected/slot-nonexclusive"
jgf="${SHARNESS_TEST_SRCDIR}/data/resource/jgfs/rabbit.json"
tiny_jgf="${SHARNESS_TEST_SRCDIR}/data/resource/jgfs/tiny.json"
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

#
# Test allocating 3 slots with only 2 SSD vertices (verifies bug fix)
#

cmds013="${cmd_dir}/cmds13.in"
test013_desc="allocate 3 slots with shared resources from 2 SSD vertices"
test_expect_success "${test013_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds013} > cmds013 &&
    ${query} -L ${jgf} -f jgf -t 013.R.out < cmds013 &&
    test_cmp 013.R.out ${exp_dir}/13.R.out
'

#
# Test that non-exclusive resources are properly tracked to prevent over-allocation
#

cmds014="${cmd_dir}/cmds-nonexclusive-tracking.in"
test014_desc="non-exclusive SSDs properly tracked, preventing over-allocation"
test_expect_success "${test014_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds014} > cmds014 &&
    ${query} -L ${jgf} -f jgf -t 014.R.out < cmds014 &&
    test_cmp 014.R.out ${exp_dir}/nonexclusive-tracking.R.out
'

#
# Test allocate_orelse_reserve with pooled non-exclusive resources (SSDs)
#

cmds015="${cmd_dir}/cmds-reserve-pooled.in"
test015_desc="allocate_orelse_reserve: pooled non-exclusive resources (SSDs)"
test_expect_success "${test015_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds015} > cmds015 &&
    ${query} -L ${jgf} -f jgf -t 015.R.out < cmds015 &&
    test_cmp 015.R.out ${exp_dir}/reserve-pooled.R.out
'

#
# Test allocate_orelse_reserve with mixed exclusive/non-exclusive resources
#

cmds016="${cmd_dir}/cmds-reserve-mixed.in"
test016_desc="allocate_orelse_reserve: mixed exclusive/non-exclusive resources"
test_expect_success "${test016_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds016} > cmds016 &&
    ${query} -L ${jgf} -f jgf -t 016.R.out < cmds016 &&
    test_cmp 016.R.out ${exp_dir}/reserve-mixed.R.out
'

#
# Test non-pooled non-exclusive resources (GPUs without units)
#

cmds017="${cmd_dir}/cmds17.in"
test017_desc="non-pooled non-exclusive GPUs: allocation tracking"
test_expect_success "${test017_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds017} > cmds017 &&
    ${query} -L ${tiny_jgf} -f jgf -t 017.R.out < cmds017 &&
    test_cmp 017.R.out ${exp_dir}/17.R.out
'

#
# Test that resource-query correctly surfaces validator errors
# (regression test for resource-query/reapi errno propagation)
#
cmds018="${cmd_dir}/cmds-exclusivity-conflict.in"
test018_desc="reject jobspec with exclusive:false child under explicit exclusive:true parent"
test_expect_success "${test018_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds018} > cmds018 &&
    ${query} -L ${jgf} -f jgf -t 018.R.out < cmds018 2> 018.R.err &&
    test_cmp 018.R.out ${exp_dir}/exclusivity-conflict.R.out &&
    grep "Resource cannot explicitly set exclusive: false when an ancestor resource has explicitly set exclusive: true" 018.R.err
'

cmds019="${cmd_dir}/cmds-exclusivity-conflict-toplevel.in"
test019_desc="reject jobspec with top-level exclusive:true and immediate child exclusive:false"
test_expect_success "${test019_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds019} > cmds019 &&
    ${query} -L ${jgf} -f jgf -t 019.R.out < cmds019 2> 019.R.err &&
    test_cmp 019.R.out ${exp_dir}/exclusivity-conflict-toplevel.R.out &&
    grep "Resource cannot explicitly set exclusive: false when an ancestor resource has explicitly set exclusive: true" 019.R.err
'

#
# Pooled multi-slot: capacity sharing across slots of one job. Each slot binds
# a pooled SSD by capacity; slots spread across distinct SSDs first and pack
# (share a vertex) only when SSD vertices run out.
#

cmds020="${cmd_dir}/cmds-pooled-multislot-spread.in"
test020_desc="pooled multi-slot spreads one SSD per slot when vertices suffice"
test_expect_success "${test020_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds020} > cmds020 &&
    ${query} -L ${jgf} -f jgf -t 020.R.out < cmds020 &&
    test_cmp 020.R.out ${exp_dir}/pooled-multislot-spread.R.out
'

cmds021="${cmd_dir}/cmds-pooled-multislot-pack.in"
test021_desc="pooled multi-slot packs slots onto shared SSDs under scarcity"
test_expect_success "${test021_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds021} > cmds021 &&
    ${query} -L ${jgf} -f jgf -t 021.R.out < cmds021 &&
    test_cmp 021.R.out ${exp_dir}/pooled-multislot-pack.R.out
'

#
# Node-only slot anchoring. A single-slot request for a bare node matches; the
# count>1 cases document a known mixed-depth node-anchoring limitation (see
# ISSUE-slot-node-anchoring-mixed-depth) and are expected to NOT match on
# rabbit. Flip these to expect-match if that limitation is fixed.
#

cmds022="${cmd_dir}/cmds-node-slot-single.in"
test022_desc="single slot with a bare node matches (cluster-direct node)"
test_expect_success "${test022_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds022} > cmds022 &&
    ${query} -L ${jgf} -f jgf -t 022.R.out < cmds022 &&
    test_cmp 022.R.out ${exp_dir}/node-slot-single.R.out
'

cmds023="${cmd_dir}/cmds-node-slot-multi.in"
test023_desc="KNOWN LIMITATION: count:7 bare-node slot does not match on rabbit"
test_expect_success "${test023_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds023} > cmds023 &&
    ${query} -L ${jgf} -f jgf -t 023.R.out < cmds023 &&
    test_cmp 023.R.out ${exp_dir}/node-slot-multi.R.out
'

cmds024="${cmd_dir}/cmds-node-slot-multi-direct-core.in"
test024_desc="KNOWN LIMITATION: count:7 bare-node slot, core direct, no match"
test_expect_success "${test024_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds024} > cmds024 &&
    ${query} -L ${jgf} -f jgf -t 024.R.out < cmds024 &&
    test_cmp 024.R.out ${exp_dir}/node-slot-multi-direct-core.R.out
'

cmds025="${cmd_dir}/cmds-node-slot-multi-exclusive.in"
test025_desc="KNOWN LIMITATION: count:7 bare-node exclusive slot, no match"
test_expect_success "${test025_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds025} > cmds025 &&
    ${query} -L ${jgf} -f jgf -t 025.R.out < cmds025 &&
    test_cmp 025.R.out ${exp_dir}/node-slot-multi-exclusive.R.out
'

#
# Repeated pooled single-slot allocation: verifies the per-SSD floor
# (floor(capacity/per_slot)) and that match-output indentation is stable across
# successive allocations.
#

cmds026="${cmd_dir}/cmds-pooled-slot-single-500.in"
test026_desc="pooled 500 GB slot: 1 alloc per SSD (4 succeed, 5th fails); stable output"
test_expect_success "${test026_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds026} > cmds026 &&
    ${query} -L ${jgf} -f jgf -t 026.R.out < cmds026 &&
    test_cmp 026.R.out ${exp_dir}/pooled-slot-single-500.R.out
'

cmds027="${cmd_dir}/cmds-pooled-slot-half.in"
test027_desc="pooled ~half (396 GB) slot: 2 allocs per SSD (8 succeed, 9th fails)"
test_expect_success "${test027_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds027} > cmds027 &&
    ${query} -L ${jgf} -f jgf -t 027.R.out < cmds027 &&
    test_cmp 027.R.out ${exp_dir}/pooled-slot-half.R.out
'

test_done

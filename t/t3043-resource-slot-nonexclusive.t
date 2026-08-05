#!/bin/sh

test_description='Test that resources with explicit exclusive:false under slots can be shared'

. $(dirname $0)/sharness.sh

cmd_dir="${SHARNESS_TEST_SRCDIR}/data/resource/commands/slot-nonexclusive"
exp_dir="${SHARNESS_TEST_SRCDIR}/data/resource/expected/slot-nonexclusive"
jgf="${SHARNESS_TEST_SRCDIR}/data/resource/jgfs/rabbit.json"
jgf_4ssd="${SHARNESS_TEST_SRCDIR}/data/resource/jgfs/rabbit-4ssd.json"
jgf_cap2="${SHARNESS_TEST_SRCDIR}/data/resource/jgfs/rabbit-cap2.json"
tiny_jgf="${SHARNESS_TEST_SRCDIR}/data/resource/jgfs/tiny.json"
query="../../resource/utilities/resource-query"

#
# Test that SSDs with explicit exclusive:false can be matched
#

cmds001="${cmd_dir}/cmds-ssd-shared-test.in"
test001_desc="match shared SSDs under slot multiple times"
test_expect_success "${test001_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds001} > cmds001 &&
    ${query} -L ${jgf} -f jgf -P first -t 001.R.out < cmds001 &&
    test_cmp 001.R.out ${exp_dir}/ssd-shared-test.R.out
'

#
# Test that exclusive SSDs limit allocations
#

cmds002="${cmd_dir}/cmds-ssd-test.in"
test002_desc="exclusive SSDs limit allocations to available chassis"
test_expect_success "${test002_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds002} > cmds002 &&
    ${query} -L ${jgf} -f jgf -P first -t 002.R.out < cmds002 &&
    test_cmp 002.R.out ${exp_dir}/ssd-test.R.out
'

#
# Test allocation with exact SSD count request (chassis with exactly 2 SSDs, exclusive: false)
#

cmds003="${cmd_dir}/cmds03.in"
test003_desc="allocate chassis with exact SSD count (shared)"
test_expect_success "${test003_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds003} > cmds003 &&
    ${query} -L ${jgf} -f jgf -P first -t 003.R.out < cmds003 &&
    test_cmp 003.R.out ${exp_dir}/03.R.out
'

#
# Test impossible pooled request (999 GiB exceeds every single SSD's 793 GiB)
#

cmds004="${cmd_dir}/cmds04.in"
test004_desc="pooled draw larger than any single SSD fails immediately"
test_expect_success "${test004_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds004} > cmds004 &&
    ${query} -L ${jgf} -f jgf -P first -t 004.R.out < cmds004 &&
    test_cmp 004.R.out ${exp_dir}/04.R.out
'

#
# Test ssd-limit-test-exclusive (should allocate only 2 due to chassis limit)
#

cmds005="${cmd_dir}/cmds05.in"
test005_desc="exclusive SSDs: chassis limit determines max allocations"
test_expect_success "${test005_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds005} > cmds005 &&
    ${query} -L ${jgf} -f jgf -P first -t 005.R.out < cmds005 &&
    test_cmp 005.R.out ${exp_dir}/05.R.out
'

#
# Test ssd-limit-test-shared (should also allocate only 2 due to chassis limit)
#

cmds006="${cmd_dir}/cmds06.in"
test006_desc="shared SSDs: chassis limit still constrains allocations"
test_expect_success "${test006_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds006} > cmds006 &&
    ${query} -L ${jgf} -f jgf -P first -t 006.R.out < cmds006 &&
    test_cmp 006.R.out ${exp_dir}/06.R.out
'

#
# Non-pooled (unit-less) LEAF resources cannot be requested with
# exclusive:false: sharing them has no tracking semantics (the vertex
# would be absent from R and invisibly multi-booked), so the traverser
# rejects the request with a diagnostic instead of failing silently.
# This test pins the EXPLICIT exclusive:false flavor. The jobspec shape
# is structurally matchable on tiny.json, so the rejection -- not a
# shape mismatch -- is what fails the match.
#

cmds007="${cmd_dir}/cmds07.in"
test007_desc="reject explicit exclusive:false on a non-pooled leaf (gpu)"
test_expect_success "${test007_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds007} > cmds007 &&
    ${query} -L ${tiny_jgf} -f jgf -P first -t 007.R.out < cmds007 2> 007.R.err &&
    test_cmp 007.R.out ${exp_dir}/07.R.out &&
    grep "non-pooled resource type .gpu. cannot be allocated with exclusive: false" 007.R.err
'

#
# Test cores under slot (implicit exclusive)
#

cmds008="${cmd_dir}/cmds08.in"
test008_desc="cores under slot remain exclusive by default"
test_expect_success "${test008_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds008} > cmds008 &&
    ${query} -L ${jgf} -f jgf -P first -t 008.R.out < cmds008 &&
    test_cmp 008.R.out ${exp_dir}/08.R.out
'

#
# Test exclusive SSDs with chassis exhaustion (using ssd-limit-test-exclusive.yaml)
#

cmds009="${cmd_dir}/cmds09.in"
test009_desc="exclusive SSDs limited by chassis availability"
test_expect_success "${test009_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds009} > cmds009 &&
    ${query} -L ${jgf} -f jgf -P first -t 009.R.out < cmds009 &&
    test_cmp 009.R.out ${exp_dir}/09.R.out
'

#
# Test shared SSDs with chassis exhaustion (using ssd-limit-test-shared.yaml)
#

cmds010="${cmd_dir}/cmds10.in"
test010_desc="shared SSDs limited by chassis, not SSD exhaustion"
test_expect_success "${test010_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds010} > cmds010 &&
    ${query} -L ${jgf} -f jgf -P first -t 010.R.out < cmds010 &&
    test_cmp 010.R.out ${exp_dir}/10.R.out
'

#
# Test allocating 3 slots with only 2 SSD vertices (verifies bug fix)
#

cmds011="${cmd_dir}/cmds11.in"
test011_desc="allocate 3 slots with shared resources from 2 SSD vertices"
test_expect_success "${test011_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds011} > cmds011 &&
    ${query} -L ${jgf} -f jgf -P first -t 011.R.out < cmds011 &&
    test_cmp 011.R.out ${exp_dir}/11.R.out
'

#
# Test that pooled non-exclusive SSD usage is tracked by capacity: repeated
# 1 GiB requests keep succeeding against the same shared vertices
#

cmds012="${cmd_dir}/cmds-nonexclusive-tracking.in"
test012_desc="pooled non-exclusive SSDs tracked by capacity across allocations"
test_expect_success "${test012_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds012} > cmds012 &&
    ${query} -L ${jgf} -f jgf -P first -t 012.R.out < cmds012 &&
    test_cmp 012.R.out ${exp_dir}/nonexclusive-tracking.R.out
'

#
# Test allocate_orelse_reserve with pooled non-exclusive resources (SSDs):
# ample pooled capacity means the orelse_reserve request allocates now
#

cmds013="${cmd_dir}/cmds-reserve-pooled.in"
test013_desc="allocate_orelse_reserve: ample pooled capacity allocates now"
test_expect_success "${test013_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds013} > cmds013 &&
    ${query} -L ${jgf} -f jgf -P first -t 013.R.out < cmds013 &&
    test_cmp 013.R.out ${exp_dir}/reserve-pooled.R.out
'

#
# Test allocate_orelse_reserve with mixed exclusive/non-exclusive resources
#

cmds014="${cmd_dir}/cmds-reserve-mixed.in"
test014_desc="allocate_orelse_reserve: mixed exclusive/non-exclusive resources"
test_expect_success "${test014_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds014} > cmds014 &&
    ${query} -L ${jgf} -f jgf -P first -t 014.R.out < cmds014 &&
    test_cmp 014.R.out ${exp_dir}/reserve-mixed.R.out
'

#
# Test that resource-query correctly surfaces validator errors
# (regression test for resource-query/reapi errno propagation)
#
cmds015="${cmd_dir}/cmds-exclusivity-conflict.in"
test015_desc="reject jobspec with exclusive:false child under explicit exclusive:true parent"
test_expect_success "${test015_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds015} > cmds015 &&
    ${query} -L ${jgf} -f jgf -P first -t 015.R.out < cmds015 2> 015.R.err &&
    test_cmp 015.R.out ${exp_dir}/exclusivity-conflict.R.out &&
    grep "Resource cannot explicitly set exclusive: false when an ancestor resource has explicitly set exclusive: true" 015.R.err
'

cmds016="${cmd_dir}/cmds-exclusivity-conflict-toplevel.in"
test016_desc="reject jobspec with top-level exclusive:true and immediate child exclusive:false"
test_expect_success "${test016_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds016} > cmds016 &&
    ${query} -L ${jgf} -f jgf -P first -t 016.R.out < cmds016 2> 016.R.err &&
    test_cmp 016.R.out ${exp_dir}/exclusivity-conflict-toplevel.R.out &&
    grep "Resource cannot explicitly set exclusive: false when an ancestor resource has explicitly set exclusive: true" 016.R.err
'

#
# Pooled multi-slot: capacity sharing across slots of one job. Each slot binds
# ONE pooled SSD by capacity, and a vertex backs additional slots while its
# capacity lasts. Slot-to-vertex assignment follows the match policy's egroup
# order (for the default `first` policy, the first discovered SSD serves all
# slots it can hold), so slots pack onto a shared vertex rather than spreading.
#

cmds017="${cmd_dir}/cmds-pooled-multislot-share.in"
test017_desc="pooled multi-slot: 2 slots share one SSD's capacity"
test_expect_success "${test017_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds017} > cmds017 &&
    ${query} -L ${jgf} -f jgf -P first -t 017.R.out < cmds017 &&
    test_cmp 017.R.out ${exp_dir}/pooled-multislot-share.R.out
'

cmds018="${cmd_dir}/cmds-pooled-multislot-pack.in"
test018_desc="pooled multi-slot: 7 slots pack onto one SSD, more slots than vertices"
test_expect_success "${test018_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds018} > cmds018 &&
    ${query} -L ${jgf} -f jgf -P first -t 018.R.out < cmds018 &&
    test_cmp 018.R.out ${exp_dir}/pooled-multislot-pack.R.out
'

#
# Node-only slot anchoring. A single-slot request for a bare node matches; the
# count>1 cases document a known mixed-depth node-anchoring limitation (see
# ISSUE-slot-node-anchoring-mixed-depth) and are expected to NOT match on
# rabbit. Flip these to expect-match if that limitation is fixed.
#

cmds019="${cmd_dir}/cmds-node-slot-single.in"
test019_desc="single slot with a bare node matches (cluster-direct node)"
test_expect_success "${test019_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds019} > cmds019 &&
    ${query} -L ${jgf} -f jgf -P first -t 019.R.out < cmds019 &&
    test_cmp 019.R.out ${exp_dir}/node-slot-single.R.out
'

cmds020="${cmd_dir}/cmds-node-slot-multi.in"
test020_desc="KNOWN LIMITATION: count:7 bare-node slot does not match on rabbit"
test_expect_success "${test020_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds020} > cmds020 &&
    ${query} -L ${jgf} -f jgf -P first -t 020.R.out < cmds020 &&
    test_cmp 020.R.out ${exp_dir}/node-slot-multi.R.out
'

cmds021="${cmd_dir}/cmds-node-slot-multi-direct-core.in"
test021_desc="KNOWN LIMITATION: count:7 bare-node slot, core direct, no match"
test_expect_success "${test021_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds021} > cmds021 &&
    ${query} -L ${jgf} -f jgf -P first -t 021.R.out < cmds021 &&
    test_cmp 021.R.out ${exp_dir}/node-slot-multi-direct-core.R.out
'

cmds022="${cmd_dir}/cmds-node-slot-multi-exclusive.in"
test022_desc="KNOWN LIMITATION: count:7 bare-node exclusive slot, no match"
test_expect_success "${test022_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds022} > cmds022 &&
    ${query} -L ${jgf} -f jgf -P first -t 022.R.out < cmds022 &&
    test_cmp 022.R.out ${exp_dir}/node-slot-multi-exclusive.R.out
'

#
# Repeated pooled single-slot allocation: verifies the per-SSD floor
# (floor(capacity/per_slot)) and that match-output indentation is stable across
# successive allocations.
#

cmds023="${cmd_dir}/cmds-pooled-slot-single-500.in"
test023_desc="pooled 500 GiB slot: 1 alloc per SSD (4 succeed, 5th fails); stable output"
test_expect_success "${test023_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds023} > cmds023 &&
    ${query} -L ${jgf} -f jgf -P first -t 023.R.out < cmds023 &&
    test_cmp 023.R.out ${exp_dir}/pooled-slot-single-500.R.out
'

cmds024="${cmd_dir}/cmds-pooled-slot-half.in"
test024_desc="pooled ~half (396 GiB) slot: 2 allocs per SSD (8 succeed, 9th fails)"
test_expect_success "${test024_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds024} > cmds024 &&
    ${query} -L ${jgf} -f jgf -P first -t 024.R.out < cmds024 &&
    test_cmp 024.R.out ${exp_dir}/pooled-slot-half.R.out
'

#
# INHERITED non-exclusivity also rejects at a non-pooled leaf: the socket is
# explicitly shared and its unit-less children (gpu, core) say nothing about
# exclusivity, so they inherit exclusive:false and both leaves are rejected.
# The socket itself -- a non-leaf traversal waypoint -- remains a valid
# shared request (as in the node-slot tests).
#

cmds025="${cmd_dir}/cmds-gpu-shared-test.in"
test025_desc="reject inherited exclusive:false on non-pooled leaves (gpu, core)"
test_expect_success "${test025_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds025} > cmds025 &&
    ${query} -L ${tiny_jgf} -f jgf -P first -t 025.R.out < cmds025 2> 025.R.err &&
    test_cmp 025.R.out ${exp_dir}/gpu-shared-test.R.out &&
    grep "non-pooled resource type .gpu. cannot be allocated with exclusive: false" 025.R.err &&
    grep "non-pooled resource type .core. cannot be allocated with exclusive: false" 025.R.err
'

#
# Pooled slot accounting must be match-policy agnostic: a statically exploring
# policy (high) must record the same shared capacity draw-down as the default
# dynamic (first) policy -- 7x5 GiB slots pack onto one SSD (35 GiB total when
# one chassis holds all 7 nodes), and 500 GiB slots exhaust the four 793 GiB
# SSDs after 4 allocations.
#

test026_desc="pooled multi-slot slot accounting matches under static (high) policy"
test_expect_success "${test026_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds018} > cmds026 &&
    ${query} -L ${jgf} -f jgf -P high -t 026.R.out < cmds026 &&
    test_cmp 026.R.out ${exp_dir}/pooled-multislot-pack-high.R.out
'

test027_desc="pooled slot exhaustion matches under static (high) policy"
test_expect_success "${test027_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds023} > cmds027 &&
    ${query} -L ${jgf} -f jgf -P high -t 027.R.out < cmds027 &&
    test_cmp 027.R.out ${exp_dir}/pooled-slot-single-500-high.R.out
'

test028_desc="pooled multi-slot slot accounting matches under low policy"
test_expect_success "${test028_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds018} > cmds028 &&
    ${query} -L ${jgf} -f jgf -P low -t 028.R.out < cmds028 &&
    test_cmp 028.R.out ${exp_dir}/pooled-multislot-pack-low.R.out
'

test029_desc="pooled slot exhaustion matches under low policy"
test_expect_success "${test029_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds023} > cmds029 &&
    ${query} -L ${jgf} -f jgf -P low -t 029.R.out < cmds029 &&
    test_cmp 029.R.out ${exp_dir}/pooled-slot-single-500-low.R.out
'

#
# Parse-time rejections: duplicate same-type siblings under a slot, and
# ranged/stepped counts on a pooled (non-exclusive, unit-bearing) request.
#

cmds030="${cmd_dir}/cmds-dup-siblings.in"
test030_desc="reject jobspec with duplicate same-type siblings under a slot"
test_expect_success "${test030_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds030} > cmds030 &&
    ${query} -L ${jgf} -f jgf -P first -t 030.R.out < cmds030 2> 030.R.err &&
    test_cmp 030.R.out ${exp_dir}/dup-siblings.R.out &&
    grep "occurs more than once among siblings under a slot" 030.R.err
'

cmds031="${cmd_dir}/cmds-elastic-pooled.in"
test031_desc="reject pooled (unit-bearing) requests with ranged or stepped counts"
test_expect_success "${test031_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds031} > cmds031 &&
    ${query} -L ${jgf} -f jgf -P first -t 031.R.out < cmds031 2> 031.R.err &&
    test_cmp 031.R.out ${exp_dir}/elastic-pooled.R.out &&
    test $(grep -c "requires a fixed count" 031.R.err) -eq 3
'

#
# Vertex-authoritative pooling: a shared request with NO unit still draws from
# the pooled (unit-bearing) graph vertex by capacity. 500-count draws exhaust
# each 793 GiB SSD after one draw (4 succeed, 5th fails), proving the request
# is pooled -- and NOT satisfied by fragments across vertices.
#

cmds032="${cmd_dir}/cmds-pooled-nounit.in"
test032_desc="unit-less shared request draws from pooled vertex capacity"
test_expect_success "${test032_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds032} > cmds032 &&
    ${query} -L ${jgf} -f jgf -P first -t 032.R.out < cmds032 &&
    test_cmp 032.R.out ${exp_dir}/pooled-nounit.R.out
'

#
# Elastic guard at match time: a ranged count on a unit-less shared request
# becomes pooled only against the graph (vertex carries the unit), so the
# fixed-count requirement is enforced by the traverser, not the parser.
#

cmds033="${cmd_dir}/cmds-elastic-nounit.in"
test033_desc="match-time rejection of ranged count on unit-less pooled request"
test_expect_success "${test033_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds033} > cmds033 &&
    ${query} -L ${jgf} -f jgf -P first -t 033.R.out < cmds033 2> 033.R.err &&
    test_cmp 033.R.out ${exp_dir}/elastic-nounit.R.out &&
    grep "pooled request for type .ssd. requires a fixed count" 033.R.err
'

#
# Unit agreement: a shared request whose unit (GB) differs from the vertex
# unit (GiB) must not match -- pooled draws never convert units.
#

cmds034="${cmd_dir}/cmds-unit-mismatch.in"
test034_desc="shared request with mismatched unit does not match pooled vertex"
test_expect_success "${test034_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds034} > cmds034 &&
    ${query} -L ${jgf} -f jgf -P first -t 034.R.out < cmds034 &&
    test_cmp 034.R.out ${exp_dir}/unit-mismatch.R.out
'

#
# Cancel returns pooled capacity: after the SSD pool is exhausted (4 x 500 GiB
# draws; 5th fails), canceling job 1 lets a repeat request allocate.
#

cmds035="${cmd_dir}/cmds-cancel-reuse.in"
test035_desc="canceled job returns pooled capacity for reuse"
test_expect_success "${test035_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds035} > cmds035 &&
    ${query} -L ${jgf} -f jgf -P first -t 035.R.out < cmds035 &&
    test_cmp 035.R.out ${exp_dir}/cancel-reuse.R.out
'

cmds036="${cmd_dir}/cmds-cancel-reserve.in"
test036_desc="exhausted pool: orelse_reserve reserves; after cancel it allocates"
test_expect_success "${test036_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds036} > cmds036 &&
    ${query} -L ${jgf} -f jgf -P first -t 036.R.out < cmds036 &&
    test_cmp 036.R.out ${exp_dir}/cancel-reserve.R.out
'

#
# Exclusivity re-establishment: exclusive:true below an exclusive:false
# ancestor makes that subtree exclusive again -- allocations share the SSD
# pool but must take different (exclusive) nodes.
#

cmds037="${cmd_dir}/cmds-excl-reestablish.in"
test037_desc="exclusive:true below exclusive:false ancestor re-establishes exclusivity"
test_expect_success "${test037_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds037} > cmds037 &&
    ${query} -L ${jgf} -f jgf -P first -t 037.R.out < cmds037 &&
    test_cmp 037.R.out ${exp_dir}/excl-reestablish.R.out
'

#
# JGF reconstruction of pooled draws (update allocate): a valid fragment with
# a partial-size pooled vertex (500 of 793 GiB) is accepted and charges the
# pool (only 3 more 500 GiB draws fit); corrupted fragments are rejected.
#

pooled500="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/slot-nonexclusive/pooled-slot-single-500.yaml"

test038_desc="valid pooled R fragment reconstructs and charges shared capacity"
test_expect_success "${test038_desc}" '
    cat >cmds038_gen <<-EOF &&
	match allocate ${pooled500}
	quit
	EOF
    ${query} -L ${jgf} -f jgf -P first -F jgf -t 038.gen.out < cmds038_gen &&
    grep -v INFO 038.gen.out > 038.good.json &&
    cat >cmds038 <<-EOF &&
	update allocate jgf 038.good.json 1 0 3600
	match allocate ${pooled500}
	match allocate ${pooled500}
	match allocate ${pooled500}
	match allocate ${pooled500}
	quit
	EOF
    ${query} -L ${jgf} -f jgf -P first -t 038.R.out < cmds038 &&
    test_cmp 038.R.out ${exp_dir}/reconstruct-charge.R.out
'

jq_size_inflate='(.graph.nodes[] | select(.metadata.type=="ssd") | .metadata.size) = 9999'
jq_size_zero='(.graph.nodes[] | select(.metadata.type=="ssd") | .metadata.size) = 0'
jq_unit_gb='(.graph.nodes[] | select(.metadata.type=="ssd") | .metadata.unit) = "GB"'
jq_unit_del='.graph.nodes[] |= (if .metadata.type=="ssd" then del(.metadata.unit) else . end)'
jq_excl_true='(.graph.nodes[] | select(.metadata.type=="ssd") | .metadata.exclusive) = true'

test039_desc="malformed pooled R fragments are rejected on reconstruction"
test_expect_success "${test039_desc}" '
    cat >cmds039_gen <<-EOF &&
	match allocate ${pooled500}
	quit
	EOF
    ${query} -L ${jgf} -f jgf -P first -F jgf -t 039.gen.out < cmds039_gen &&
    grep -v INFO 039.gen.out > 039.good.json &&
    jq -c "${jq_size_inflate}" 039.good.json > 039.size.json &&
    jq -c "${jq_unit_gb}" 039.good.json > 039.unit.json &&
    jq -c "${jq_unit_del}" 039.good.json > 039.nounit.json &&
    jq -c "${jq_size_zero}" 039.good.json > 039.zero.json &&
    jq -c "${jq_excl_true}" 039.good.json > 039.excl.json &&
    cat >cmds039 <<-EOF &&
	update allocate jgf 039.size.json 1 0 3600
	update allocate jgf 039.unit.json 2 0 3600
	update allocate jgf 039.nounit.json 3 0 3600
	update allocate jgf 039.zero.json 4 0 3600
	update allocate jgf 039.excl.json 5 0 3600
	quit
	EOF
    ${query} -L ${jgf} -f jgf -P first -t 039.R.out < cmds039 2> 039.R.err &&
    test_cmp 039.R.out ${exp_dir}/reconstruct-malformed.R.out &&
    test $(grep -c "inconsistent input vertex" 039.R.err) -eq 5
'

#
# Pooled shares are per-vertex whole-chunk fits, not aggregate capacity:
# three 500 GiB slots against ONE chassis with four 793 GiB SSDs need three
# distinct vertices (floor(793/500)=1 each), but two SSDs' summed capacity
# (1586 GiB) already "covers" 3x500. A tally that divides the aggregate
# fabricates 3 shares from two vertices and stops dynamic exploration
# before a third SSD is discovered, failing a satisfiable request -- and
# with a single chassis there is no sibling subtree to mask the undercount
# (unlike the 2x2 rabbit topology). The second allocation must fail: the
# remainders (293 GiB x3 + 793 GiB) fit only one more whole 500 GiB chunk.
#

cmds040="${cmd_dir}/cmds-pooled-3slots-500.in"
test040_desc="pooled slots count per-vertex fits: 3x500 GiB on one 4-SSD chassis (first)"
test_expect_success "${test040_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds040} > cmds040 &&
    ${query} -L ${jgf_4ssd} -f jgf -P first -t 040.R.out < cmds040 &&
    test_cmp 040.R.out ${exp_dir}/pooled-3slots-500-first.R.out
'

test041_desc="pooled slots count per-vertex fits: 3x500 GiB on one 4-SSD chassis (high)"
test_expect_success "${test041_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds040} > cmds041 &&
    ${query} -L ${jgf_4ssd} -f jgf -P high -t 041.R.out < cmds041 &&
    test_cmp 041.R.out ${exp_dir}/pooled-3slots-500-high.R.out
'

#
# INHERITED pooled classification: the chassis under the slot is explicitly
# shareable and its ssd child says nothing, so the ssd inherits
# exclusive:false and must pool exactly as if it carried the field itself
# ([300:s] capacity draws, not whole exclusive vertices). floor(793/300)=2
# draws per SSD and 2 slots per job: 4 jobs exhaust the four SSDs, the 5th
# fails. The accounting must be identical under dynamic (first) and static
# (high/low) policies.
#

cmds042="${cmd_dir}/cmds-pooled-inherited.in"
test042_desc="ancestor-inherited exclusive:false pools the ssd (first)"
test_expect_success "${test042_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds042} > cmds042 &&
    ${query} -L ${jgf} -f jgf -P first -t 042.R.out < cmds042 &&
    test_cmp 042.R.out ${exp_dir}/pooled-inherited-first.R.out
'

test043_desc="ancestor-inherited exclusive:false pools the ssd (high)"
test_expect_success "${test043_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds042} > cmds043 &&
    ${query} -L ${jgf} -f jgf -P high -t 043.R.out < cmds043 &&
    test_cmp 043.R.out ${exp_dir}/pooled-inherited-high.R.out
'

test044_desc="ancestor-inherited exclusive:false pools the ssd (low)"
test_expect_success "${test044_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds042} > cmds044 &&
    ${query} -L ${jgf} -f jgf -P low -t 044.R.out < cmds044 &&
    test_cmp 044.R.out ${exp_dir}/pooled-inherited-low.R.out
'

#
# Full-pool reconstruction boundary: a pooled R fragment drawing the ENTIRE
# pool (793 of 793 GiB) is legal and must charge the whole pool -- but it is
# still a pooled record, so the unit rules apply at full size too (a wrong
# or missing unit is rejected, never treated as an exact-size exclusive
# match). A fragment size of 2^32+2 must be rejected in the int64 domain:
# narrowing it to unsigned int would wrap to 2 and falsely match as a tiny
# valid draw. The 1 GiB probe (JOBID=5) pins the full charge: it must NOT
# land on the updated ssd (residual 0), which a partial charge would allow.
#

test045_desc="full-pool R fragment charges all capacity; unit/size corruption rejected"
test_expect_success "${test045_desc}" '
    cat >cmds045_gen <<-EOF &&
	match allocate ${pooled500}
	quit
	EOF
    ${query} -L ${jgf} -f jgf -P first -F jgf -t 045.gen.out < cmds045_gen &&
    grep -v INFO 045.gen.out > 045.good.json &&
    jq -c "(.graph.nodes[] | select(.metadata.type==\"ssd\") | .metadata.size) = 793" \
        045.good.json > 045.full.json &&
    jq -c "${jq_unit_gb}" 045.full.json > 045.unit.json &&
    jq -c "${jq_unit_del}" 045.full.json > 045.nounit.json &&
    jq -c "(.graph.nodes[] | select(.metadata.type==\"ssd\") | .metadata.size) = 4294967298" \
        045.good.json > 045.narrow.json &&
    tiny="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/slot-nonexclusive/pooled-tiny-draw.yaml" &&
    cat >cmds045 <<-EOF &&
	update allocate jgf 045.full.json 1 0 3600
	update allocate jgf 045.unit.json 2 0 3600
	update allocate jgf 045.nounit.json 3 0 3600
	update allocate jgf 045.narrow.json 4 0 3600
	match allocate ${tiny}
	match allocate ${pooled500}
	match allocate ${pooled500}
	match allocate ${pooled500}
	match allocate ${pooled500}
	quit
	EOF
    ${query} -L ${jgf} -f jgf -P first -t 045.R.out < cmds045 2> 045.R.err &&
    test_cmp 045.R.out ${exp_dir}/reconstruct-fullsize.R.out &&
    test $(grep -c "inconsistent input vertex" 045.R.err) -eq 3
'

#
# Materialization regression: a count:1 slot drawing 1 GiB from a 793 GiB
# pool. Candidate slots are capped by the requested slot count, not by the
# 793 chunks the pool could theoretically back, so this matches promptly
# and draws exactly [1:s].
#

cmds046="${cmd_dir}/cmds-pooled-tiny-draw.in"
test046_desc="pooled 1 GiB draw from a 793 GiB pool: slot candidates capped by request"
test_expect_success "${test046_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds046} > cmds046 &&
    ${query} -L ${jgf} -f jgf -P first -t 046.R.out < cmds046 &&
    test_cmp 046.R.out ${exp_dir}/pooled-tiny-draw.R.out
'

#
# A slot is itself an exclusive allocation container: exclusive:false ON the
# slot element is ignored and does not propagate to unspecified children.
# The ssd below inherits the slot default (exclusive), so each slot claims a
# whole SSD ([793:x]): 2 jobs x 2 slots consume all 4 SSDs, the 3rd fails.
# If slot-level exclusive:false is ever made to propagate (a proposed
# follow-up), this pins the behavior change.
#

cmds047="${cmd_dir}/cmds-slot-excl-false-ignored.in"
test047_desc="exclusive:false on the slot element itself is ignored (not propagated)"
test_expect_success "${test047_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds047} > cmds047 &&
    ${query} -L ${jgf} -f jgf -P first -t 047.R.out < cmds047 &&
    test_cmp 047.R.out ${exp_dir}/slot-excl-false-ignored.R.out
'

#
# A shared unit-less waypoint does NOT multiply slots by its vertex size:
# on rabbit-cap2.json (2 chassis of size 2) each slot consumes one whole
# chassis egroup, so 2-slot jobs fit but a 4-slot job must fail rather
# than reuse a chassis egroup within one job (which would charge the
# chassis's nested pooled ssd draw once per egroup instead of once per
# slot and double-book the pool across jobs). The fit jobs pin the
# cumulative charge: jobs 1 and 3 drain one ssd per chassis to 193 GiB,
# job 4 must move to the fresh ssds, and job 6 fails exhausted.
#

cmds048="${cmd_dir}/cmds-pooled-cap2.in"
test048_desc="shared waypoint size does not multiply slots; pooled draws charge per slot"
test_expect_success "${test048_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds048} > cmds048 &&
    ${query} -L ${jgf_cap2} -f jgf -P first -t 048.R.out < cmds048 &&
    test_cmp 048.R.out ${exp_dir}/pooled-cap2.R.out
'

test_done

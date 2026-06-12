#!/bin/sh

test_description='Test dynamic exploration under first-match policies

The first* policies are the only ones that route through
explore_dynamically (), whose early-termination predicates must agree
with dom_slot ()s whole-edge-group packing and must observe counts
merged up from pass-through vertices by resolve (). These tests pin
both properties with golden outputs and run the same jobspecs under
-P high for differential comparison.
'

. $(dirname $0)/sharness.sh

cmd_dir="${SHARNESS_TEST_SRCDIR}/data/resource/commands/dyn_explore"
exp_dir="${SHARNESS_TEST_SRCDIR}/data/resource/expected/dyn_explore"
jgf="${SHARNESS_TEST_SRCDIR}/data/resource/jgfs/dyn_explore.json"
grugs="${SHARNESS_TEST_SRCDIR}/data/resource/grugs/medium.graphml"
query="../../resource/utilities/resource-query"

#
# Multi-slot jobspec whose shape includes high-capacity vertices: each
# 616 GiB ssd covers more than one slot's 101 GiB share, so satisfying
# the 2-slot request requires two ssd granules, not 202 GiB in aggregate.
# A decoy chassis with ssds but no compute nodes precedes the eligible
# one. Prior to the granule-aware stop condition, explore_dynamically ()
# terminated after a single ssd edge and the match failed (pol=first).
#

cmds001="${cmd_dir}/cmds01.in"
test001_desc="multi-slot match with high-capacity granules works (pol=first)"
test_expect_success "${test001_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds001} > cmds001 &&
    ${query} -L ${jgf} -f jgf -S CA -P first -t 001.R.out < cmds001 &&
    test_cmp 001.R.out ${exp_dir}/001.R.out
'

cmds002="${cmd_dir}/cmds01.in"
test002_desc="multi-slot match with high-capacity granules works (pol=high)"
test_expect_success "${test002_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds002} > cmds002 &&
    ${query} -L ${jgf} -f jgf -S CA -P high -t 002.R.out < cmds002 &&
    test_cmp 002.R.out ${exp_dir}/002.R.out
'

#
# Partially specified jobspec rooted at node on a multi-rack graph: the
# requested node counts reach the cluster level only through resolve ()/
# merge () from pass-through rack vertices, and early termination must
# fire on those merged counts. Sequential allocations perturb
# availability so that losing the early stop (exploring every rack)
# yields a different selection from the pinned golden (pol=first).
#

cmds003="${cmd_dir}/cmds02.in"
test003_desc="node-rooted prefix jobspec keeps early-stop selection (pol=first)"
test_expect_success "${test003_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds003} > cmds003 &&
    ${query} -L ${grugs} -S CA -P first -t 003.R.out < cmds003 &&
    test_cmp 003.R.out ${exp_dir}/003.R.out
'

cmds004="${cmd_dir}/cmds02.in"
test004_desc="node-rooted prefix jobspec selection differential (pol=high)"
test_expect_success "${test004_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds004} > cmds004 &&
    ${query} -L ${grugs} -S CA -P high -t 004.R.out < cmds004 &&
    test_cmp 004.R.out ${exp_dir}/004.R.out
'

#
# Per-slot requests exceeding any single vertex's capacity: each 700-unit
# ssd share must be packed from two whole 616-unit vertices. The share
# tally mirrors dom_slot ()'s greedy whole-egroup packing, so dynamic
# exploration stops on an exactly packable egroup set.
#

cmds005="${cmd_dir}/cmds03.in"
test005_desc="slot share spanning multiple vertices matches (pol=first)"
test_expect_success "${test005_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds005} > cmds005 &&
    ${query} -L ${jgf} -f jgf -S CA -P first -t 005.R.out < cmds005 &&
    test_cmp 005.R.out ${exp_dir}/005.R.out
'

# Known breakage: under static exploration the same satisfiable request
# fails because cnt_slot () estimates fit = min (qc/count, qg), which
# over-counts when shares span granules (fit = 2464/700 = 3 vs 2 truly
# packable), and dom_slot ()'s exhaustion path discards already-built
# slots. This test starts passing when cnt_slot () counts greedy
# whole-granule shares or dom_slot () keeps completed slots.
cmds006="${cmd_dir}/cmds03.in"
test006_desc="slot share spanning multiple vertices matches (pol=high)"
test_expect_failure "${test006_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds006} > cmds006 &&
    ${query} -L ${jgf} -f jgf -S CA -P high -t 006.high.R.out < cmds006 &&
    grep -q "RESOURCES=ALLOCATED" 006.high.R.out
'

#
# Per-slot requests exceeding the total ssd capacity must fail cleanly,
# and a failed exhaustive dynamic exploration must not corrupt traversal
# state: the satisfiable follow-up request in the same session allocates.
#

cmds007="${cmd_dir}/cmds04.in"
test007_desc="oversized request fails cleanly and next request matches (pol=first)"
test_expect_success "${test007_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds007} > cmds007 &&
    ${query} -L ${jgf} -f jgf -S CA -P first -t 006.R.out < cmds007 &&
    test_cmp 006.R.out ${exp_dir}/006.R.out
'

#
# firstnodex routes through the same dynamic exploration as first and
# exhibited the same granule bug; pin it with its static differential
# partner hinodex on a jobspec with no explicit exclusivity, where the
# nodex policies impose node exclusivity themselves.
#

cmds008="${cmd_dir}/cmds05.in"
test008_desc="multi-slot match with high-capacity granules works (pol=firstnodex)"
test_expect_success "${test008_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds008} > cmds008 &&
    ${query} -L ${jgf} -f jgf -S CA -P firstnodex -t 007.R.out < cmds008 &&
    test_cmp 007.R.out ${exp_dir}/007.R.out
'

cmds009="${cmd_dir}/cmds05.in"
test009_desc="multi-slot match with high-capacity granules works (pol=hinodex)"
test_expect_success "${test009_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds009} > cmds009 &&
    ${query} -L ${jgf} -f jgf -S CA -P hinodex -t 008.R.out < cmds009 &&
    test_cmp 008.R.out ${exp_dir}/008.R.out
'

test_done

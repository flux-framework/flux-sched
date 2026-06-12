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

test_done

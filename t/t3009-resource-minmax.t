#!/bin/sh

test_description='Test min and max moldable matching'

. $(dirname $0)/sharness.sh

cmd_dir="${SHARNESS_TEST_SRCDIR}/data/resource/commands/min_max"
exp_dir="${SHARNESS_TEST_SRCDIR}/data/resource/expected/min_max"
grugs="${SHARNESS_TEST_SRCDIR}/data/resource/grugs/resv_test.graphml"
query="../../resource/utilities/resource-query"

#
# Selection Policy -- High ID first (-P high)
#     The resource vertex with higher ID is preferred among its kind
#     (e.g., node1 is preferred over node0 if available)
#

cmds001="${cmd_dir}/cmds01.in"
test001_desc="min/max with OP=multiplication on slot type works (pol=hi)"
test_expect_success "${test001_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds001} > cmds001 &&
    ${query} -L ${grugs} -S CA -P high -t 001.R.out < cmds001 &&
    test_cmp 001.R.out ${exp_dir}/001.R.out
'

cmds002="${cmd_dir}/cmds02.in"
test002_desc="min/max with OP=addition on slot type works (pol=hi)"
test_expect_success "${test002_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds002} > cmds002 &&
    ${query} -L ${grugs} -S CA -P high -t 002.R.out < cmds002 &&
    test_cmp 002.R.out ${exp_dir}/002.R.out
'

cmds003="${cmd_dir}/cmds03.in"
test003_desc="min/max with OP=multiplication on node type works (pol=hi)"
test_expect_success "${test003_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds003} > cmds003 &&
    ${query} -L ${grugs} -S CA -P high -t 003.R.out < cmds003 &&
    test_cmp 003.R.out ${exp_dir}/003.R.out
'

cmds004="${cmd_dir}/cmds04.in"
test004_desc="min/max with OP=addition on node type works (pol=hi)"
test_expect_success "${test004_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds004} > cmds004 &&
    ${query} -L ${grugs} -S CA -P high -t 004.R.out < cmds004 &&
    test_cmp 004.R.out ${exp_dir}/004.R.out
'

cmds005="${cmd_dir}/cmds05.in"
test005_desc="min/max with OP=power on node type works (pol=hi)"
test_expect_success "${test005_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds005} > cmds005 &&
    ${query} -L ${grugs} -S CA -P high -t 005.R.out < cmds005 &&
    test_cmp 005.R.out ${exp_dir}/005.R.out
'

#
# Selection Policy -- Low ID first (-P low)
#     The resource vertex with lower ID is preferred among its kind
#     (e.g., node0 is preferred over node1 if available)
#

cmds006="${cmd_dir}/cmds01.in"
test006_desc="min/max with OP=multiplication on slot type works (pol=low)"
test_expect_success "${test006_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds006} > cmds006 &&
    ${query} -L ${grugs} -S CA -P low -t 006.R.out < cmds006 &&
    test_cmp 006.R.out ${exp_dir}/006.R.out
'

cmds007="${cmd_dir}/cmds02.in"
test007_desc="min/max with OP=addition on slot type works (pol=low)"
test_expect_success "${test007_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds007} > cmds007 &&
    ${query} -L ${grugs} -S CA -P low -t 007.R.out < cmds007 &&
    test_cmp 007.R.out ${exp_dir}/007.R.out
'

cmds008="${cmd_dir}/cmds03.in"
test008_desc="min/max with OP=multiplication on node type works (pol=low)"
test_expect_success "${test008_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds008} > cmds008 &&
    ${query} -L ${grugs} -S CA -P low -t 008.R.out < cmds008 &&
    test_cmp 008.R.out ${exp_dir}/008.R.out
'

cmds009="${cmd_dir}/cmds04.in"
test009_desc="min/max with OP=addition on node type works (pol=low)"
test_expect_success "${test009_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds009} > cmds009 &&
    ${query} -L ${grugs} -S CA -P low -t 009.R.out < cmds009 &&
    test_cmp 009.R.out ${exp_dir}/009.R.out
'

cmds010="${cmd_dir}/cmds05.in"
test010_desc="min/max with OP=power on node type works (pol=low)"
test_expect_success "${test010_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds010} > cmds010 &&
    ${query} -L ${grugs} -S CA -P high -t 010.R.out < cmds010 &&
    test_cmp 010.R.out ${exp_dir}/010.R.out
'

#
# Selection Policy -- First Match (-P first)
#

cmds011="${cmd_dir}/cmds01.in"
test011_desc="min/max with OP=multiplication on slot type works (pol=first)"
test_expect_success "${test011_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds011} > cmds011 &&
    ${query} -L ${grugs} -S CA -P first -t 011.R.out < cmds011 &&
    test_cmp 011.R.out ${exp_dir}/011.R.out
'

cmds012="${cmd_dir}/cmds02.in"
test012_desc="min/max with OP=addition on slot type works (pol=first)"
test_expect_success "${test012_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds012} > cmds012 &&
    ${query} -L ${grugs} -S CA -P first -t 012.R.out < cmds012 &&
    test_cmp 012.R.out ${exp_dir}/012.R.out
'

cmds013="${cmd_dir}/cmds03.in"
test013_desc="min/max with OP=multiplication on node type works (pol=first)"
test_expect_success "${test013_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds013} > cmds013 &&
    ${query} -L ${grugs} -S CA -P first -t 013.R.out < cmds013 &&
    test_cmp 013.R.out ${exp_dir}/013.R.out
'

cmds014="${cmd_dir}/cmds04.in"
test014_desc="min/max with OP=addition on node type works (pol=first)"
test_expect_success "${test014_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds014} > cmds014 &&
    ${query} -L ${grugs} -S CA -P first -t 014.R.out < cmds014 &&
    test_cmp 014.R.out ${exp_dir}/014.R.out
'

cmds015="${cmd_dir}/cmds05.in"
test015_desc="min/max with OP=power on node type works (pol=first)"
test_expect_success "${test015_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds015} > cmds015 &&
    ${query} -L ${grugs} -S CA -P first -t 015.R.out < cmds015 &&
    test_cmp 015.R.out ${exp_dir}/015.R.out
'

#
# Selection Policy -- High node first with node exclusivity (-P hinodex)
#     Selection behavior is identical to hinode except that
#     it marks each selected node as exclusive even if the
#     jobspec does not require node exclusivity and
#     that it selects and emits all of the node-local resources
#     for each node where at least one node-local resource is selected.
#
#     For a jobspec with node[1]->slot[1]->core[1], it selects
#     36 cores from the selected node if there is a total of
#     36 cores in that node.
#
#     For a jobspec with slot[18]->core[1], it selects
#     again all 36 cores from the current available highest node.
#

cmds021="${cmd_dir}/cmds01.in"
test021_desc="min/max with OP=multiplication on slot type works (pol=hinodex)"
test_expect_success "${test021_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds021} > cmds021 &&
    ${query} -L ${grugs} -S CA -P hinodex -t 021.R.out < cmds021 &&
    test_cmp 021.R.out ${exp_dir}/021.R.out
'

cmds022="${cmd_dir}/cmds02.in"
test022_desc="min/max with OP=addition on slot type works (pol=hinodex)"
test_expect_success "${test022_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds022} > cmds022 &&
    ${query} -L ${grugs} -S CA -P hinodex -t 022.R.out < cmds022 &&
    test_cmp 022.R.out ${exp_dir}/022.R.out
'

cmds023="${cmd_dir}/cmds03.in"
test023_desc="min/max with OP=multiplication on node type works (pol=hinodex)"
test_expect_success "${test023_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds023} > cmds023 &&
    ${query} -L ${grugs} -S CA -P hinodex -t 023.R.out < cmds023 &&
    test_cmp 023.R.out ${exp_dir}/023.R.out
'

cmds024="${cmd_dir}/cmds04.in"
test024_desc="min/max with OP=addition on node type works (pol=hinodex)"
test_expect_success "${test024_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds024} > cmds024 &&
    ${query} -L ${grugs} -S CA -P hinodex -t 024.R.out < cmds024 &&
    test_cmp 024.R.out ${exp_dir}/024.R.out
'

cmds025="${cmd_dir}/cmds05.in"
test025_desc="min/max with OP=power on node type works (pol=hinodex)"
test_expect_success "${test025_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds025} > cmds025 &&
    ${query} -L ${grugs} -S CA -P hinodex -t 025.R.out < cmds025 &&
    test_cmp 025.R.out ${exp_dir}/025.R.out
'

#
# Selection Policy -- Low node first with node exclusivity (-P lonodex)
#     Selection behavior is identical to lonode except that
#     it marks each selected node as exclusive even if the
#     jobspec does not require node exclusivity and
#     that it selects and emits all of the node-local resources
#     for each node where at least one node-local resource is selected.
#
#     For a jobspec with node[1]->slot[1]->core[1], it selects
#     36 cores from the selected node if there is a total of
#     36 cores in that node.
#
#     For a jobspec with slot[18]->core[1], it selects
#     again all 36 cores from the current available lowest node.
#

cmds031="${cmd_dir}/cmds01.in"
test031_desc="min/max with OP=multiplication on slot type works (pol=lonodex)"
test_expect_success "${test031_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds031} > cmds031 &&
    ${query} -L ${grugs} -S CA -P lonodex -t 031.R.out < cmds031 &&
    test_cmp 031.R.out ${exp_dir}/031.R.out
'

cmds032="${cmd_dir}/cmds03.in"
test032_desc="min/max with OP=addition on slot type works (pol=lonodex)"
test_expect_success "${test032_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds032} > cmds032 &&
    ${query} -L ${grugs} -S CA -P lonodex -t 032.R.out < cmds032 &&
    test_cmp 032.R.out ${exp_dir}/032.R.out
'

cmds033="${cmd_dir}/cmds03.in"
test033_desc="min/max with OP=multiplication on node type works (pol=lonodex)"
test_expect_success "${test033_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds033} > cmds033 &&
    ${query} -L ${grugs} -S CA -P lonodex -t 033.R.out < cmds033 &&
    test_cmp 033.R.out ${exp_dir}/033.R.out
'

cmds034="${cmd_dir}/cmds04.in"
test034_desc="min/max with OP=addition on node type works (pol=lonodex)"
test_expect_success "${test034_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds034} > cmds034 &&
    ${query} -L ${grugs} -S CA -P lonodex -t 034.R.out < cmds034 &&
    test_cmp 034.R.out ${exp_dir}/034.R.out
'

cmds035="${cmd_dir}/cmds05.in"
test035_desc="min/max with OP=power on node type works (pol=lonodex)"
test_expect_success "${test035_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds035} > cmds035 &&
    ${query} -L ${grugs} -S CA -P lonodex -t 035.R.out < cmds035 &&
    test_cmp 035.R.out ${exp_dir}/035.R.out
'

test_done

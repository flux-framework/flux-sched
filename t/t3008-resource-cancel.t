#!/bin/sh

test_description='Test cancellation of jobs of varying geometries and durations'

. $(dirname $0)/sharness.sh

cmd_dir="${SHARNESS_TEST_SRCDIR}/data/resource/commands/cancel"
exp_dir="${SHARNESS_TEST_SRCDIR}/data/resource/expected/cancel"
grugs="${SHARNESS_TEST_SRCDIR}/data/resource/grugs/resv_test.graphml"
rv1s="${SHARNESS_TEST_SRCDIR}/data/resource/rv1exec/tiny_rv1exec.json"
jgfs="${SHARNESS_TEST_SRCDIR}/data/resource/jgfs/elastic/tiny-partial-cancel.json"
query="../../resource/utilities/resource-query"

#
# Selection Policy -- High ID first (-P high)
#     The resource vertex with higher ID is preferred among its kind
#     (e.g., node1 is preferred over node0 if available)
#

cmds001="${cmd_dir}/cmds01.in"
test001_desc="allocate or reserve 17 jobs, cancel 3 (pol=hi)"
test_expect_success "${test001_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds001} > cmds001 &&
    ${query} -L ${grugs} -S CA -P high -t 001.R.out < cmds001 &&
    test_cmp 001.R.out ${exp_dir}/001.R.out
'

cmds002="${cmd_dir}/cmds02.in"
test002_desc="allocate or reserve 17 jobs, cancel all (pol=hi)"
test_expect_success "${test002_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds002} > cmds002 &&
    ${query} -L ${grugs} -S CA -P high -t 002.R.out < cmds002 &&
    test_cmp 002.R.out ${exp_dir}/002.R.out
'

#
# Selection Policy -- High ID first (-P low)
#     The resource vertex with higher ID is preferred among its kind
#     (e.g., node0 is preferred over node1 if available)
#

cmds003="${cmd_dir}/cmds01.in"
test003_desc="allocate or reserve 17 jobs, cancel 3 (pol=low)"
test_expect_success "${test003_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds003} > cmds003 &&
    ${query} -L ${grugs} -S CA -P low -t 003.R.out < cmds003 &&
    test_cmp 003.R.out ${exp_dir}/003.R.out
'

cmds004="${cmd_dir}/cmds02.in"
test004_desc="allocate or reserve 17 jobs, cancel all (pol=low)"
test_expect_success "${test004_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds004} > cmds004 &&
    ${query} -L ${grugs} -S CA -P low -t 004.R.out < cmds004 &&
    test_cmp 004.R.out ${exp_dir}/004.R.out
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

cmds011="${cmd_dir}/cmds01.in"
test011_desc="allocate or reserve 17 jobs, cancel 3 (pol=hinodex)"
test_expect_success "${test011_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds011} > cmds011 &&
    ${query} -L ${grugs} -S CA -P hinodex -t 011.R.out < cmds011 &&
    test_cmp 011.R.out ${exp_dir}/011.R.out
'

cmds012="${cmd_dir}/cmds02.in"
test012_desc="allocate or reserve 17 jobs, cancel all (pol=hinodex)"
test_expect_success "${test012_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds012} > cmds012 &&
    ${query} -L ${grugs} -S CA -P hinodex -t 012.R.out < cmds012 &&
    test_cmp 012.R.out ${exp_dir}/012.R.out
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

cmds013="${cmd_dir}/cmds01.in"
test013_desc="allocate or reserve 17 jobs, cancel 3 (pol=lonodex)"
test_expect_success "${test013_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds013} > cmds013 &&
    ${query} -L ${grugs} -S CA -P lonodex -t 013.R.out < cmds013 &&
    test_cmp 013.R.out ${exp_dir}/013.R.out
'

cmds014="${cmd_dir}/cmds02.in"
test014_desc="allocate or reserve 17 jobs, cancel all (pol=lonodex)"
test_expect_success "${test014_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds014} > cmds014 &&
    ${query} -L ${grugs} -S CA -P lonodex -t 014.R.out < cmds014 &&
    test_cmp 014.R.out ${exp_dir}/014.R.out
'

# Partial cancel/release -- Use low node policy
#     Tests to ensure correctness of partial cancel/release behavior

cmds015="${cmd_dir}/cmds03.in"
test015_desc="test reader file option"
test_expect_success "${test015_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds015} > cmds015 &&
    ${query} -f rv1exec -L ${rv1s} -S CA -P low -t 015.R.out < cmds015 2>> 015.R.out &&
    test_cmp 015.R.out ${exp_dir}/015.R.out
'

cmds016="${cmd_dir}/cmds04.in"
test016_desc="test partial cancel and reallocation of one rank"
test_expect_success "${test016_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds016} > cmds016 &&
    ${query} -f rv1exec -L ${rv1s} -S CA -P low -t 016.R.out < cmds016 &&
    test_cmp 016.R.out ${exp_dir}/016.R.out
'

cmds017="${cmd_dir}/cmds05.in"
test017_desc="check for unsupported partial cancel of reservation"
test_expect_success "${test017_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds017} > cmds017 &&
    ${query} -f rv1exec -L ${rv1s} -S CA -P low -t 017.R.out < cmds017 2>> 017.R.out &&
    test_cmp 017.R.out ${exp_dir}/017.R.out
'

cmds018="${cmd_dir}/cmds06.in"
test018_desc="partial cancel of full allocation is the same as full cancel"
test_expect_success "${test018_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds018} > cmds018 &&
    ${query} -f rv1exec -L ${rv1s} -S CA -P low -t 018.R.out < cmds018 &&
    test_cmp 018.R.out ${exp_dir}/018.R.out
'

cmds019="${cmd_dir}/cmds06.in"
test019_desc="partial cancel of full allocation is the same as full cancel with all pruning filters set"
test_expect_success "${test019_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds019} > cmds019 &&
    ${query} -f rv1exec -L ${rv1s} -S CA -P low --prune-filters=ALL:core,ALL:node,ALL:gpu -t 019.R.out < cmds019 &&
    test_cmp 019.R.out ${exp_dir}/018.R.out
'

cmds020="${cmd_dir}/cmds07.in"
test020_desc="test partial cancel and reallocation of non-exclusive jobs"
test_expect_success "${test020_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds020} > cmds020 &&
    ${query} -f rv1exec -L ${rv1s} -S CA -P low -t 019.R.out < cmds020 &&
    test_cmp 019.R.out ${exp_dir}/019.R.out
'

cmds021="${cmd_dir}/cmds08.in"
test021_desc="test partial cancel and reallocation of one rank; jgf"
test_expect_success "${test021_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds021} > cmds021 &&
    ${query} -f jgf -L ${jgfs} -S CA -P low -t 020.R.out < cmds021 &&
    test_cmp 020.R.out ${exp_dir}/020.R.out
'

cmds022="${cmd_dir}/cmds09.in"
test022_desc="partial cancel of full allocation is the same as full cancel; jgf"
test_expect_success "${test022_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds022} > cmds022 &&
    ${query} -f jgf -L ${jgfs} -S CA -P low -t 021.R.out < cmds022 &&
    test_cmp 021.R.out ${exp_dir}/021.R.out
'

cmds023="${cmd_dir}/cmds09.in"
test023_desc="partial cancel of full allocation is the same as full cancel with all pruning filters set; jgf"
test_expect_success "${test023_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds023} > cmds023 &&
    ${query} -f jgf -L ${jgfs} -S CA -P low --prune-filters=ALL:core,ALL:node,ALL:gpu -t 022.R.out < cmds023 &&
    test_cmp 022.R.out ${exp_dir}/021.R.out
'

cmds024="${cmd_dir}/cmds10.in"
test024_desc="test partial cancel and reallocation of two ranks; jgf"
test_expect_success "${test024_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds024} > cmds024 &&
    ${query} -f jgf -L ${jgfs} -S CA -P low -t 024.R.out < cmds024 &&
    test_cmp 024.R.out ${exp_dir}/024.R.out
'

# Uses the same expected output as above
cmds025="${cmd_dir}/cmds11.in"
test025_desc="test partial cancel and reallocation of two ranks; jgf graph rv1exec partial cancel"
test_expect_success "${test025_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds025} > cmds025 &&
    ${query} -f jgf -L ${jgfs} -S CA -P low -t 025.R.out < cmds025 &&
    test_cmp 025.R.out ${exp_dir}/024.R.out
'

test_done

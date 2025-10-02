#!/bin/sh

test_description='Test cancellation of jobs of varying geometries and durations'

. $(dirname $0)/sharness.sh

cmd_dir="${SHARNESS_TEST_SRCDIR}/data/resource/commands/cancel"
exp_dir="${SHARNESS_TEST_SRCDIR}/data/resource/expected/cancel"
grugs="${SHARNESS_TEST_SRCDIR}/data/resource/grugs/resv_test.graphml"
rv1s="${SHARNESS_TEST_SRCDIR}/data/resource/rv1exec/tiny_rv1exec.json"
jgfs="${SHARNESS_TEST_SRCDIR}/data/resource/jgfs/elastic/tiny-partial-cancel.json"
jgfs_power="${SHARNESS_TEST_SRCDIR}/data/resource/jgfs/power.json"
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

cmds026="${cmd_dir}/cmds12.in"
test026_desc="Test partial cancel of cross-rack allocation"
test_expect_success "${test026_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds026} > cmds026 &&
    ${query} -f jgf -L ${jgfs_power} -S CA -P low -t 026.R.out < cmds026 2>026.R.err &&
    test_cmp 026.R.out ${exp_dir}/026.R.out &&
    test_cmp 026.R.err ${exp_dir}/026.R.err
'

cmds027="${cmd_dir}/cmds12.in"
test027_desc="Test partial cancel of cross-rack allocation with more filters set"
test_expect_success "${test027_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds027} > cmds027 &&
    ${query} -f jgf -L ${jgfs_power} -S CA -P low --prune-filters=ALL:core,ALL:gpu,ALL:socket,ALL:node,ALL:memory -t 027.R.out < cmds027 2>027.R.err &&
    test_cmp 027.R.out ${exp_dir}/027.R.out &&
    test_cmp 027.R.err ${exp_dir}/027.R.err
'

cmds028="${cmd_dir}/cmds12.in"
test028_desc="Test partial cancel of cross-rack allocation with intermediate filters set"
test_expect_success "${test028_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds028} > cmds028 &&
    ${query} -f jgf -L ${jgfs_power} -S CA -P low --prune-filters=ALL:socket,ALL:node -t 028.R.out < cmds028 2>028.R.err &&
    test_cmp 028.R.out ${exp_dir}/028.R.out &&
    test_cmp 028.R.err ${exp_dir}/028.R.err
'

cmds029="${cmd_dir}/cmds12.in"
test029_desc="Test partial cancel of cross-rack allocation with leaf filters set"
test_expect_success "${test029_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds029} > cmds029 &&
    ${query} -f jgf -L ${jgfs_power} -S CA -P low --prune-filters=ALL:core,ALL:gpu,ALL:memory -t 029.R.out < cmds029 2>029.R.err &&
    test_cmp 029.R.out ${exp_dir}/029.R.out &&
    test_cmp 029.R.err ${exp_dir}/029.R.err
'

cmds030="${cmd_dir}/cmds13.in"
test030_desc="Test partial cancel of cross-rack allocation specifying lower-level resources"
test_expect_success "${test030_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds030} > cmds030 &&
    ${query} -f jgf -L ${jgfs_power} -S CA -P low -t 030.R.out < cmds030 2>030.R.err &&
    test_cmp 030.R.out ${exp_dir}/030.R.out &&
    test_cmp 030.R.err ${exp_dir}/030.R.err
'

cmds031="${cmd_dir}/cmds13.in"
test031_desc="Test partial cancel of cross-rack allocation with more filters set, specifying lower-level resources"
test_expect_success "${test031_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds031} > cmds031 &&
    ${query} -f jgf -L ${jgfs_power} -S CA -P low --prune-filters=ALL:core,ALL:gpu,ALL:socket,ALL:node,ALL:memory -t 031.R.out < cmds031 2>031.R.err &&
    test_cmp 031.R.out ${exp_dir}/031.R.out &&
    test_cmp 031.R.err ${exp_dir}/031.R.err
'

cmds032="${cmd_dir}/cmds13.in"
test032_desc="Test partial cancel of cross-rack allocation with intermediate filters set, specifying lower-level resources"
test_expect_success "${test032_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds032} > cmds032 &&
    ${query} -f jgf -L ${jgfs_power} -S CA -P low --prune-filters=ALL:socket,ALL:node -t 032.R.out < cmds032 2>032.R.err &&
    test_cmp 032.R.out ${exp_dir}/032.R.out &&
    test_cmp 032.R.err ${exp_dir}/032.R.err
'

cmds033="${cmd_dir}/cmds13.in"
test033_desc="Test partial cancel of cross-rack allocation with leaf filters set, specifying lower-level resources"
test_expect_success "${test033_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds033} > cmds033 &&
    ${query} -f jgf -L ${jgfs_power} -S CA -P low --prune-filters=ALL:core,ALL:gpu,ALL:memory -t 033.R.out < cmds033 2>033.R.err &&
    test_cmp 033.R.out ${exp_dir}/033.R.out &&
    test_cmp 033.R.err ${exp_dir}/033.R.err
'
# Uses the same expected output as 030
cmds034="${cmd_dir}/cmds14.in"
test034_desc="Test partial cancel of cross-rack allocation specifying rack-level resources"
test_expect_success "${test034_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds034} > cmds034 &&
    ${query} -f jgf -L ${jgfs_power} -S CA -P low -t 034.R.out < cmds034 2>034.R.err &&
    test_cmp 034.R.out ${exp_dir}/030.R.out &&
    test_cmp 034.R.err ${exp_dir}/030.R.err
'
# Uses the same expected output as 031
cmds035="${cmd_dir}/cmds14.in"
test035_desc="Test partial cancel of cross-rack allocation with more filters set, specifying rack-level resources"
test_expect_success "${test035_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds035} > cmds035 &&
    ${query} -f jgf -L ${jgfs_power} -S CA -P low --prune-filters=ALL:core,ALL:gpu,ALL:socket,ALL:node,ALL:memory -t 035.R.out < cmds035 2>035.R.err &&
    test_cmp 035.R.out ${exp_dir}/031.R.out &&
    test_cmp 035.R.err ${exp_dir}/031.R.err
'
# Uses the same expected output as 032
cmds036="${cmd_dir}/cmds14.in"
test036_desc="Test partial cancel of cross-rack allocation with intermediate filters set, specifying rack-level resources"
test_expect_success "${test036_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds036} > cmds036 &&
    ${query} -f jgf -L ${jgfs_power} -S CA -P low --prune-filters=ALL:socket,ALL:node -t 036.R.out < cmds036 2>036.R.err &&
    test_cmp 036.R.out ${exp_dir}/032.R.out &&
    test_cmp 036.R.err ${exp_dir}/032.R.err
'
# Uses the same expected output as 033
cmds037="${cmd_dir}/cmds14.in"
test037_desc="Test partial cancel of cross-rack allocation with leaf filters set, specifying rack-level resources"
test_expect_success "${test037_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds037} > cmds037 &&
    ${query} -f jgf -L ${jgfs_power} -S CA -P low --prune-filters=ALL:core,ALL:gpu,ALL:memory -t 037.R.out < cmds037 2>037.R.err &&
    test_cmp 037.R.out ${exp_dir}/033.R.out &&
    test_cmp 037.R.err ${exp_dir}/033.R.err
'

cmds038="${cmd_dir}/cmds12.in"
test038_desc="Test partial cancel of cross-rack allocation, lonodex"
test_expect_success "${test038_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds038} > cmds038 &&
    ${query} -f jgf -L ${jgfs_power} -S CA -P lonodex -t 038.R.out < cmds038 2>038.R.err &&
    test_cmp 038.R.out ${exp_dir}/038.R.out &&
    test_cmp 038.R.err ${exp_dir}/038.R.err
'

cmds039="${cmd_dir}/cmds12.in"
test039_desc="Test partial cancel of cross-rack allocation with more filters set, lonodex"
test_expect_success "${test039_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds039} > cmds039 &&
    ${query} -f jgf -L ${jgfs_power} -S CA -P lonodex --prune-filters=ALL:core,ALL:gpu,ALL:socket,ALL:node,ALL:memory -t 039.R.out < cmds039 2>039.R.err &&
    test_cmp 039.R.out ${exp_dir}/039.R.out &&
    test_cmp 039.R.err ${exp_dir}/039.R.err
'

cmds040="${cmd_dir}/cmds12.in"
test040_desc="Test partial cancel of cross-rack allocation with intermediate filters set, lonodex"
test_expect_success "${test040_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds040} > cmds040 &&
    ${query} -f jgf -L ${jgfs_power} -S CA -P lonodex --prune-filters=ALL:socket,ALL:node -t 040.R.out < cmds040 2>040.R.err &&
    test_cmp 040.R.out ${exp_dir}/040.R.out &&
    test_cmp 040.R.err ${exp_dir}/040.R.err
'

cmds041="${cmd_dir}/cmds12.in"
test041_desc="Test partial cancel of cross-rack allocation with leaf filters set, lonodex"
test_expect_success "${test041_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds041} > cmds041 &&
    ${query} -f jgf -L ${jgfs_power} -S CA -P lonodex --prune-filters=ALL:core,ALL:gpu,ALL:memory -t 041.R.out < cmds041 2>041.R.err &&
    test_cmp 041.R.out ${exp_dir}/041.R.out &&
    test_cmp 041.R.err ${exp_dir}/041.R.err
'

cmds042="${cmd_dir}/cmds13.in"
test042_desc="Test partial cancel of cross-rack allocation specifying lower-level resources, lonodex"
test_expect_success "${test042_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds042} > cmds042 &&
    ${query} -f jgf -L ${jgfs_power} -S CA -P lonodex -t 042.R.out < cmds042 2>042.R.err &&
    test_cmp 042.R.out ${exp_dir}/042.R.out &&
    test_cmp 042.R.err ${exp_dir}/042.R.err
'

cmds043="${cmd_dir}/cmds13.in"
test043_desc="Test partial cancel of cross-rack allocation with more filters set, specifying lower-level resources, lonodex"
test_expect_success "${test043_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds043} > cmds043 &&
    ${query} -f jgf -L ${jgfs_power} -S CA -P lonodex --prune-filters=ALL:core,ALL:gpu,ALL:socket,ALL:node,ALL:memory -t 043.R.out < cmds043 2>043.R.err &&
    test_cmp 043.R.out ${exp_dir}/043.R.out &&
    test_cmp 043.R.err ${exp_dir}/043.R.err
'

cmds044="${cmd_dir}/cmds13.in"
test044_desc="Test partial cancel of cross-rack allocation with intermediate filters set, specifying lower-level resources, lonodex"
test_expect_success "${test044_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds044} > cmds044 &&
    ${query} -f jgf -L ${jgfs_power} -S CA -P lonodex --prune-filters=ALL:socket,ALL:node -t 044.R.out < cmds044 2>044.R.err &&
    test_cmp 044.R.out ${exp_dir}/044.R.out &&
    test_cmp 044.R.err ${exp_dir}/044.R.err
'

cmds045="${cmd_dir}/cmds13.in"
test045_desc="Test partial cancel of cross-rack allocation with leaf filters set, specifying lower-level resources, lonodex"
test_expect_success "${test045_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds045} > cmds045 &&
    ${query} -f jgf -L ${jgfs_power} -S CA -P lonodex --prune-filters=ALL:core,ALL:gpu,ALL:memory -t 045.R.out < cmds045 2>045.R.err &&
    test_cmp 045.R.out ${exp_dir}/045.R.out &&
    test_cmp 045.R.err ${exp_dir}/045.R.err
'
# Uses the same expected output as 042
cmds046="${cmd_dir}/cmds14.in"
test046_desc="Test partial cancel of cross-rack allocation specifying rack-level resources, lonodex"
test_expect_success "${test046_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds046} > cmds046 &&
    ${query} -f jgf -L ${jgfs_power} -S CA -P lonodex -t 046.R.out < cmds046 2>046.R.err &&
    test_cmp 046.R.out ${exp_dir}/042.R.out &&
    test_cmp 046.R.err ${exp_dir}/042.R.err
'
# Uses the same expected output as 043
cmds047="${cmd_dir}/cmds14.in"
test047_desc="Test partial cancel of cross-rack allocation with more filters set, specifying rack-level resources, lonodex"
test_expect_success "${test047_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds047} > cmds047 &&
    ${query} -f jgf -L ${jgfs_power} -S CA -P lonodex --prune-filters=ALL:core,ALL:gpu,ALL:socket,ALL:node,ALL:memory -t 047.R.out < cmds047 2>047.R.err &&
    test_cmp 047.R.out ${exp_dir}/043.R.out &&
    test_cmp 047.R.err ${exp_dir}/043.R.err
'
# Uses the same expected output as 044
cmds048="${cmd_dir}/cmds14.in"
test048_desc="Test partial cancel of cross-rack allocation with intermediate filters set, specifying rack-level resources, lonodex"
test_expect_success "${test048_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds048} > cmds048 &&
    ${query} -f jgf -L ${jgfs_power} -S CA -P lonodex --prune-filters=ALL:socket,ALL:node -t 048.R.out < cmds048 2>048.R.err &&
    test_cmp 048.R.out ${exp_dir}/044.R.out &&
    test_cmp 048.R.err ${exp_dir}/044.R.err
'
# Uses the same expected output as 045
cmds049="${cmd_dir}/cmds14.in"
test049_desc="Test partial cancel of cross-rack allocation with leaf filters set, specifying rack-level resources, lonodex"
test_expect_success "${test049_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds049} > cmds049 &&
    ${query} -f jgf -L ${jgfs_power} -S CA -P lonodex --prune-filters=ALL:core,ALL:gpu,ALL:memory -t 049.R.out < cmds049 2>049.R.err &&
    test_cmp 049.R.out ${exp_dir}/045.R.out &&
    test_cmp 049.R.err ${exp_dir}/045.R.err
'

# Tests for invalid partial cancel requests
cmds050="${cmd_dir}/cmds15.in"
test050_desc="partial cancel with invalid jgf rank leaves allocation unchanged"
test_expect_success "${test050_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds050} > cmds050 &&
    ${query} -f jgf -L ${jgfs} -S CA -P low -t 050.R.out < cmds050 2>050.R.err &&
    test_cmp 050.R.out ${exp_dir}/050.R.out &&
    test_cmp 050.R.err ${exp_dir}/050.R.err
'

cmds051="${cmd_dir}/cmds16.in"
test051_desc="partial cancel with invalid rv1exec rank leaves allocation unchanged"
test_expect_success "${test051_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds051} > cmds051 &&
    ${query} -f rv1exec -L ${rv1s} -S CA -P low -t 051.R.out < cmds051 2>051.R.err &&
    test_cmp 051.R.out ${exp_dir}/051.R.out &&
    test_cmp 051.R.err ${exp_dir}/051.R.err
'

# Add simple test for cross-rack allocation partial cancel that fails to remove resources
# without rank-based, parent update cancellation
cmds052="${cmd_dir}/cmds17.in"
test052_desc="partial cancel with invalid resource specification fails to remove resources"
test_expect_success "${test052_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds052} > cmds052 &&
    ${query} -f jgf -L ${jgfs_power} -S CA -P low -t 052.R.out < cmds052 &&
    test_cmp 052.R.out ${exp_dir}/052.R.out
'

test_done

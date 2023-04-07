#!/bin/sh

test_description='Test for exclusivity at various levels'

. $(dirname $0)/sharness.sh

cmd_dir="${SHARNESS_TEST_SRCDIR}/data/resource/commands/exclusive"
exp_dir="${SHARNESS_TEST_SRCDIR}/data/resource/expected/exclusive"
grugs="${SHARNESS_TEST_SRCDIR}/data/resource/grugs/medium.graphml"
query="../../resource/utilities/resource-query"

#
# Selection Policy -- High ID first (-P high)
#     The resource vertex with higher ID is preferred among its kind
#     (e.g., node1 is preferred over node0 if available)
#

cmds001="${cmd_dir}/cmds01.in"
test001_desc="allocate entire cluster then nothing scheduled (pol=hi)"
test_expect_success "${test001_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds001} > cmds001 &&
    ${query} -L ${grugs} -S CA -P high -t 001.R.out < cmds001 &&
    test_cmp 001.R.out ${exp_dir}/001.R.out
'

cmds002="${cmd_dir}/cmds02.in"
test002_desc="allocate 1 node then rack4x shouldn't match (pol=hi)"
test_expect_success "${test002_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds002} > cmds002 &&
    ${query} -L ${grugs} -S CA -P high -t 002.R.out < cmds002 &&
    test_cmp 002.R.out ${exp_dir}/002.R.out
'

cmds003="${cmd_dir}/cmds03.in"
test003_desc="match allocate with several rack exclusives 1 (pol=hi)"
test_expect_success "${test003_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds003} > cmds003 &&
    ${query} -L ${grugs} -S CA -P high -t 003.R.out < cmds003 &&
    test_cmp 003.R.out ${exp_dir}/003.R.out
'

cmds004="${cmd_dir}/cmds04.in"
test004_desc="match allocate 4 full rack exclusives (pol=hi)"
test_expect_success "${test004_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds004} > cmds004 &&
    ${query} -L ${grugs} -S CA -P high -t 004.R.out < cmds004 &&
    test_cmp 004.R.out ${exp_dir}/004.R.out
'

cmds005="${cmd_dir}/cmds05.in"
test005_desc="match allocate with several rack exclusives 2 (pol=hi)"
test_expect_success "${test005_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds005} > cmds005 &&
    ${query} -L ${grugs} -S CA -P high -t 005.R.out < cmds005 &&
    test_cmp 005.R.out ${exp_dir}/005.R.out
'

cmds006="${cmd_dir}/cmds06.in"
test006_desc="match allocate 4 rack exclusively then nothing matched 1 (pol=hi)"
test_expect_success "${test006_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds006} > cmds006 &&
    ${query} -L ${grugs} -S CA -P high -t 006.R.out < cmds006 &&
    test_cmp 006.R.out ${exp_dir}/006.R.out
'

cmds007="${cmd_dir}/cmds07.in"
test007_desc="match allocate 4 rack exclusively then nothing matched 2 (pol=hi)"
test_expect_success "${test007_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds007} > cmds007 &&
    ${query} -L ${grugs} -S CA -P high -t 007.R.out < cmds007 &&
    test_cmp 007.R.out ${exp_dir}/007.R.out
'


#
# Selection Policy -- High ID first (-P low)
#     The resource vertex with higher ID is preferred among its kind
#     (e.g., node0 is preferred over node1 if available)
#

cmds008="${cmd_dir}/cmds01.in"
test008_desc="allocate entire cluster then nothing scheduled (pol=low)"
test_expect_success "${test008_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds008} > cmds008 &&
    ${query} -L ${grugs} -S CA -P high -t 008.R.out < cmds008 &&
    test_cmp 008.R.out ${exp_dir}/008.R.out
'

cmds009="${cmd_dir}/cmds02.in"
test009_desc="allocate 1 node then rack4x shouldn't match (pol=low)"
test_expect_success "${test009_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds009} > cmds009 &&
    ${query} -L ${grugs} -S CA -P low -t 009.R.out < cmds009 &&
    test_cmp 009.R.out ${exp_dir}/009.R.out
'

cmds010="${cmd_dir}/cmds03.in"
test010_desc="match allocate with several rack exclusives 1 (pol=low)"
test_expect_success "${test010_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds010} > cmds010 &&
    ${query} -L ${grugs} -S CA -P low -t 010.R.out < cmds010 &&
    test_cmp 010.R.out ${exp_dir}/010.R.out
'

cmds011="${cmd_dir}/cmds04.in"
test011_desc="match allocate 4 full rack exclusives (pol=low)"
test_expect_success "${test011_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds011} > cmds011 &&
    ${query} -L ${grugs} -S CA -P low -t 011.R.out < cmds011 &&
    test_cmp 011.R.out ${exp_dir}/011.R.out
'

cmds012="${cmd_dir}/cmds05.in"
test012_desc="match allocate with several rack exclusives 2 (pol=low)"
test_expect_success "${test012_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds012} > cmds012 &&
    ${query} -L ${grugs} -S CA -P low -t 012.R.out < cmds012 &&
    test_cmp 012.R.out ${exp_dir}/012.R.out
'

cmds013="${cmd_dir}/cmds06.in"
test013_desc="allocate 4 rack exclusively then nothing matched 1 (pol=low)"
test_expect_success "${test013_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds013} > cmds013 &&
    ${query} -L ${grugs} -S CA -P low -t 013.R.out < cmds013 &&
    test_cmp 013.R.out ${exp_dir}/013.R.out
'

cmds014="${cmd_dir}/cmds07.in"
test014_desc="allocate 4 rack exclusively then nothing matched 2 (pol=low)"
test_expect_success "${test014_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds014} > cmds014 &&
    ${query} -L ${grugs} -S CA -P low -t 014.R.out < cmds014 &&
    test_cmp 014.R.out ${exp_dir}/014.R.out
'

#
# Selection Policy -- First Match (-P first)
#

cmds021="${cmd_dir}/cmds01.in"
test021_desc="allocate entire cluster then nothing scheduled (pol=first)"
test_expect_success "${test021_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds021} > cmds021 &&
    ${query} -L ${grugs} -S CA -P first -t 021.R.out < cmds021 &&
    test_cmp 021.R.out ${exp_dir}/021.R.out
'

cmds022="${cmd_dir}/cmds02.in"
test022_desc="allocate 1 node then rack4x shouldn't match (pol=first)"
test_expect_success "${test022_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds022} > cmds022 &&
    ${query} -L ${grugs} -S CA -P first -t 022.R.out < cmds022 &&
    test_cmp 022.R.out ${exp_dir}/022.R.out
'

cmds023="${cmd_dir}/cmds03.in"
test023_desc="match allocate with several rack exclusives 1 (pol=first)"
test_expect_success "${test023_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds023} > cmds023 &&
    ${query} -L ${grugs} -S CA -P first -t 023.R.out < cmds023 &&
    test_cmp 023.R.out ${exp_dir}/023.R.out
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

# cluster and rack exclusivity must not lead to the emission
# of the entire cluster resources
cmds031="${cmd_dir}/cmds01.in"
test031_desc="allocate entire cluster then nothing scheduled (pol=hinodex)"
test_expect_success "${test031_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds031} > cmds031 &&
    ${query} -L ${grugs} -S CA -P hinodex -t 031.R.out < cmds031 &&
    test_cmp 031.R.out ${exp_dir}/031.R.out
'

cmds032="${cmd_dir}/cmds02.in"
test032_desc="allocate 1 node then rack4x shouldn't match (pol=hinodex)"
test_expect_success "${test032_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds032} > cmds032 &&
    ${query} -L ${grugs} -S CA -P hinodex -t 032.R.out < cmds032 &&
    test_cmp 032.R.out ${exp_dir}/032.R.out
'

cmds033="${cmd_dir}/cmds03.in"
test033_desc="match allocate with several rack exclusives 1 (pol=hinodex)"
test_expect_success "${test033_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds033} > cmds033 &&
    ${query} -L ${grugs} -S CA -P hinodex -t 033.R.out < cmds033 &&
    test_cmp 033.R.out ${exp_dir}/033.R.out
'

cmds034="${cmd_dir}/cmds04.in"
test034_desc="match allocate 4 full rack exclusives (pol=hinodex)"
test_expect_success "${test034_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds034} > cmds034 &&
    ${query} -L ${grugs} -S CA -P hinodex -t 034.R.out < cmds034 &&
    test_cmp 034.R.out ${exp_dir}/034.R.out
'

cmds035="${cmd_dir}/cmds05.in"
test035_desc="match allocate with several rack exclusives 2 (pol=hinodex)"
test_expect_success "${test035_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds035} > cmds035 &&
    ${query} -L ${grugs} -S CA -P hinodex -t 035.R.out < cmds035 &&
    test_cmp 035.R.out ${exp_dir}/035.R.out
'

# full resource emission on only one node per each rack
cmds036="${cmd_dir}/cmds06.in"
test036_desc="allocate 4 rack exclusively then nothing matched 1 (pol=hinodex)"
test_expect_success "${test036_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds036} > cmds036 &&
    ${query} -L ${grugs} -S CA -P hinodex -t 036.R.out < cmds036 &&
    test_cmp 036.R.out ${exp_dir}/036.R.out
'

cmds037="${cmd_dir}/cmds07.in"
test037_desc="allocate 4 rack exclusively then nothing matched 2 (pol=hinodex)"
test_expect_success "${test037_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds037} > cmds037 &&
    ${query} -L ${grugs} -S CA -P hinodex -t 037.R.out < cmds037 &&
    test_cmp 037.R.out ${exp_dir}/037.R.out
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

# cluster and rack exclusivity must not lead to the emission
# of the entire cluster resources
cmds041="${cmd_dir}/cmds01.in"
test041_desc="allocate entire cluster then nothing scheduled (pol=lonodex)"
test_expect_success "${test041_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds041} > cmds041 &&
    ${query} -L ${grugs} -S CA -P lonodex -t 041.R.out < cmds041 &&
    test_cmp 041.R.out ${exp_dir}/041.R.out
'

cmds042="${cmd_dir}/cmds02.in"
test042_desc="allocate 1 node then rack4x shouldn't match (pol=lonodex)"
test_expect_success "${test042_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds042} > cmds042 &&
    ${query} -L ${grugs} -S CA -P lonodex -t 042.R.out < cmds042 &&
    test_cmp 042.R.out ${exp_dir}/042.R.out
'

cmds043="${cmd_dir}/cmds04.in"
test043_desc="match allocate with several rack exclusives 1 (pol=lonodex)"
test_expect_success "${test043_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds043} > cmds043 &&
    ${query} -L ${grugs} -S CA -P lonodex -t 043.R.out < cmds043 &&
    test_cmp 043.R.out ${exp_dir}/043.R.out
'

cmds044="${cmd_dir}/cmds04.in"
test044_desc="match allocate 4 full rack exclusives (pol=lonodex)"
test_expect_success "${test044_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds044} > cmds044 &&
    ${query} -L ${grugs} -S CA -P lonodex -t 044.R.out < cmds044 &&
    test_cmp 044.R.out ${exp_dir}/044.R.out
'

cmds045="${cmd_dir}/cmds05.in"
test045_desc="match allocate with several rack exclusives 2 (pol=lonodex)"
test_expect_success "${test045_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds045} > cmds045 &&
    ${query} -L ${grugs} -S CA -P lonodex -t 045.R.out < cmds045 &&
    test_cmp 045.R.out ${exp_dir}/045.R.out
'

# full resource emission on only one node per each rack
cmds046="${cmd_dir}/cmds06.in"
test046_desc="allocate 4 rack exclusively then nothing matched 1 (pol=lonodex)"
test_expect_success "${test046_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds046} > cmds046 &&
    ${query} -L ${grugs} -S CA -P lonodex -t 046.R.out < cmds046 &&
    test_cmp 046.R.out ${exp_dir}/046.R.out
'

cmds047="${cmd_dir}/cmds07.in"
test047_desc="allocate 4 rack exclusively then nothing matched 2 (pol=lonodex)"
test_expect_success "${test047_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds047} > cmds047 &&
    ${query} -L ${grugs} -S CA -P lonodex -t 047.R.out < cmds047 &&
    test_cmp 047.R.out ${exp_dir}/047.R.out
'

test_done

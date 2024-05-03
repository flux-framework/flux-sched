#!/bin/sh

test_description='Test min/max matching 2'

. $(dirname $0)/sharness.sh

cmd_dir="${SHARNESS_TEST_SRCDIR}/data/resource/commands/min_max2"
exp_dir="${SHARNESS_TEST_SRCDIR}/data/resource/expected/min_max2"
grugs="${SHARNESS_TEST_SRCDIR}/data/resource/grugs/tiny.graphml"
query="../../src/resource/utilities/resource-query"

#
# Match selection Policy -- High ID first (-P high)
#

cmds001="${cmd_dir}/cmds01.in"
test001_desc="min=1/max=18 on core works (pol=hi)"
test_expect_success "${test001_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds001} > cmds001 &&
    ${query} -L ${grugs} -S CA -P high -t 001.R.out < cmds001 &&
    test_cmp 001.R.out ${exp_dir}/001.R.out
'

cmds002="${cmd_dir}/cmds02.in"
test002_desc="min=1/max=99999 on core works (pol=hi)"
test_expect_success "${test002_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds002} > cmds002 &&
    ${query} -L ${grugs} -S CA -P high -t 002.R.out < cmds002 &&
    test_cmp 002.R.out ${exp_dir}/002.R.out
'

cmds003="${cmd_dir}/cmds03.in"
test003_desc="min=1/max=18 on core min=1/max=2 on gpu works (pol=hi)"
test_expect_success "${test003_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds003} > cmds003 &&
    ${query} -L ${grugs} -S CA -P high -t 003.R.out < cmds003 &&
    test_cmp 003.R.out ${exp_dir}/003.R.out
'

cmds004="${cmd_dir}/cmds04.in"
test004_desc="min=1/max=99999 on core min=1/max=99999 on gpu works (pol=hi)"
test_expect_success "${test004_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds004} > cmds004 &&
    ${query} -L ${grugs} -S CA -P high -t 004.R.out < cmds004 &&
    test_cmp 004.R.out ${exp_dir}/004.R.out
'

cmds005="${cmd_dir}/cmds05.in"
test005_desc="min=1/max=18 on core results in count=4 (pol=hi)"
test_expect_success "${test005_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds005} > cmds005 &&
    ${query} -L ${grugs} -S CA -P high -t 005.R.out < cmds005 &&
    test_cmp 005.R.out ${exp_dir}/005.R.out
'

cmds006="${cmd_dir}/cmds06.in"
test006_desc="A node exclusive request (node=1) works (pol=hi)"
test_expect_success "${test006_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds006} > cmds006 &&
    ${query} -L ${grugs} -S CA -P high -t 006.R.out < cmds006 &&
    test_cmp 006.R.out ${exp_dir}/006.R.out
'

cmds008="${cmd_dir}/cmds08.in"
test008_desc="A multi-level min/max works (pol=hi)"
test_expect_success "${test008_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds008} > cmds008 &&
    ${query} -L ${grugs} -S CA -P high -t 008.R.out < cmds008 &&
    test_cmp 008.R.out ${exp_dir}/008.R.out
'

cmds009="${cmd_dir}/cmds09.in"
test009_desc="A multi-level min/max works with an existing allocation (pol=hi)"
test_expect_success "${test009_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds009} > cmds009 &&
    ${query} -L ${grugs} -S CA -P high -t 009.R.out < cmds009 &&
    test_cmp 009.R.out ${exp_dir}/009.R.out
'

#
# Match selection Policy -- Low ID first (-P low)
#

cmds011="${cmd_dir}/cmds01.in"
test011_desc="min=1/max=18 on core works (pol=low)"
test_expect_success "${test011_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds011} > cmds011 &&
    ${query} -L ${grugs} -S CA -P low -t 011.R.out < cmds011 &&
    test_cmp 011.R.out ${exp_dir}/011.R.out
'

cmds012="${cmd_dir}/cmds02.in"
test012_desc="min=1/max=99999 on core works (pol=low)"
test_expect_success "${test012_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds012} > cmds012 &&
    ${query} -L ${grugs} -S CA -P low -t 012.R.out < cmds012 &&
    test_cmp 012.R.out ${exp_dir}/012.R.out
'

cmds013="${cmd_dir}/cmds03.in"
test013_desc="min=1/max=18 on core min=1/max=2 on gpu works (pol=low)"
test_expect_success "${test013_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds013} > cmds013 &&
    ${query} -L ${grugs} -S CA -P low -t 013.R.out < cmds013 &&
    test_cmp 013.R.out ${exp_dir}/013.R.out
'

cmds014="${cmd_dir}/cmds04.in"
test014_desc="min=1/max=99999 on core min=1/max=99999 on gpu works (pol=low)"
test_expect_success "${test014_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds014} > cmds014 &&
    ${query} -L ${grugs} -S CA -P low -t 014.R.out < cmds014 &&
    test_cmp 014.R.out ${exp_dir}/014.R.out
'

cmds015="${cmd_dir}/cmds05.in"
test015_desc="min=1/max=18 on core results in count=4 (pol=low)"
test_expect_success "${test015_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds015} > cmds015 &&
    ${query} -L ${grugs} -S CA -P low -t 015.R.out < cmds015 &&
    test_cmp 015.R.out ${exp_dir}/015.R.out
'

cmds016="${cmd_dir}/cmds06.in"
test016_desc="A node exclusive request (node=1) works (pol=low)"
test_expect_success "${test016_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds016} > cmds016 &&
    ${query} -L ${grugs} -S CA -P low -t 016.R.out < cmds016 &&
    test_cmp 016.R.out ${exp_dir}/016.R.out
'

cmds018="${cmd_dir}/cmds08.in"
test018_desc="A multi-level min/max works (pol=low)"
test_expect_success "${test018_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds018} > cmds018 &&
    ${query} -L ${grugs} -S CA -P low -t 018.R.out < cmds018 &&
    test_cmp 018.R.out ${exp_dir}/018.R.out
'

cmds019="${cmd_dir}/cmds09.in"
test019_desc="A multi-level min/max works with an existing allocation (pol=low)"
test_expect_success "${test019_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds019} > cmds019 &&
    ${query} -L ${grugs} -S CA -P low -t 019.R.out < cmds019 &&
    test_cmp 019.R.out ${exp_dir}/019.R.out
'


#
# Match selection Policy -- Low ID first (-P first)
#

cmds021="${cmd_dir}/cmds01.in"
test021_desc="min=1/max=18 on core works (pol=first)"
test_expect_success "${test021_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds021} > cmds021 &&
    ${query} -L ${grugs} -S CA -P first -t 021.R.out < cmds021 &&
    test_cmp 021.R.out ${exp_dir}/021.R.out
'

cmds022="${cmd_dir}/cmds02.in"
test022_desc="min=1/max=99999 on core works (pol=first)"
test_expect_success "${test022_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds022} > cmds022 &&
    ${query} -L ${grugs} -S CA -P first -t 022.R.out < cmds022 &&
    test_cmp 022.R.out ${exp_dir}/022.R.out
'

cmds023="${cmd_dir}/cmds03.in"
test023_desc="min=1/max=18 on core min=1/max=2 on gpu works (pol=first)"
test_expect_success "${test023_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds023} > cmds023 &&
    ${query} -L ${grugs} -S CA -P first -t 023.R.out < cmds023 &&
    test_cmp 023.R.out ${exp_dir}/023.R.out
'

cmds024="${cmd_dir}/cmds04.in"
test024_desc="min=1/max=99999 on core min=1/max=99999 on gpu works (pol=first)"
test_expect_success "${test024_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds024} > cmds024 &&
    ${query} -L ${grugs} -S CA -P first -t 024.R.out < cmds024 &&
    test_cmp 024.R.out ${exp_dir}/024.R.out
'

cmds025="${cmd_dir}/cmds05.in"
test025_desc="min=1/max=18 on core results in count=4 (pol=first)"
test_expect_success "${test025_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds025} > cmds025 &&
    ${query} -L ${grugs} -S CA -P first -t 025.R.out < cmds025 &&
    test_cmp 025.R.out ${exp_dir}/025.R.out
'

cmds026="${cmd_dir}/cmds06.in"
test026_desc="A node exclusive request (node=1) works (pol=first)"
test_expect_success "${test026_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds026} > cmds026 &&
    ${query} -L ${grugs} -S CA -P first -t 026.R.out < cmds026 &&
    test_cmp 026.R.out ${exp_dir}/026.R.out
'

cmds028="${cmd_dir}/cmds08.in"
test028_desc="A multi-level min/max works (pol=first)"
test_expect_success "${test028_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds028} > cmds028 &&
    ${query} -L ${grugs} -S CA -P first -t 028.R.out < cmds028 &&
    test_cmp 028.R.out ${exp_dir}/028.R.out
'

cmds029="${cmd_dir}/cmds09.in"
test029_desc="A multi-level min/max works with an existing allocation (pol=first)"
test_expect_success "${test029_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds029} > cmds029 &&
    ${query} -L ${grugs} -S CA -P first -t 029.R.out < cmds029 &&
    test_cmp 029.R.out ${exp_dir}/029.R.out
'

test_done

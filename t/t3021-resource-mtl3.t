#!/bin/sh

test_description='Test multi-tiered storage level3: L1+L2 simultaneously'

. $(dirname $0)/sharness.sh

cmd_dir="${SHARNESS_TEST_SRCDIR}/data/resource/commands/mt-storage/L3"
exp_dir="${SHARNESS_TEST_SRCDIR}/data/resource/expected/mt-storage/L3"
grugs="${SHARNESS_TEST_SRCDIR}/data/resource/grugs/mt-storage/mtl3.graphml"
query="../../resource/utilities/resource-query"

cmds001="${cmd_dir}/cmds01.in"
test001_desc="match allocate 9 L1 jobs - last one fails (pol=hi)"
test_expect_success "${test001_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds001} > cmds001 &&
    ${query} -L ${grugs} -S CA -P high -t 001.R.out < cmds001 &&
    test_cmp 001.R.out ${exp_dir}/001.R.out
'

cmds002="${cmd_dir}/cmds02.in"
test002_desc="match allocate 9 L2 jobs - last one fails (pol=hi)"
test_expect_success "${test002_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds002} > cmds002 &&
    ${query} -L ${grugs} -S CA -P high -t 002.R.out < cmds002 &&
    test_cmp 002.R.out ${exp_dir}/002.R.out
'

cmds003="${cmd_dir}/cmds03.in"
test003_desc="match allocate 9 L1+L2 jobs - last one fails (pol=hi)"
test_expect_success "${test003_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds003} > cmds003 &&
    ${query} -L ${grugs} -S CA -P high -t 003.R.out < cmds003 &&
    test_cmp 003.R.out ${exp_dir}/003.R.out
'

cmds004="${cmd_dir}/cmds01.in"
test004_desc="match allocate 9 L1 jobs - last one fails (pol=low)"
test_expect_success "${test004_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds004} > cmds004 &&
    ${query} -L ${grugs} -S CA -P low -t 004.R.out < cmds004 &&
    test_cmp 004.R.out ${exp_dir}/004.R.out
'

cmds005="${cmd_dir}/cmds02.in"
test005_desc="match allocate 9 L2 jobs - last one fails (pol=low)"
test_expect_success "${test005_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds005} > cmds005 &&
    ${query} -L ${grugs} -S CA -P low -t 005.R.out < cmds005 &&
    test_cmp 005.R.out ${exp_dir}/005.R.out
'

cmds006="${cmd_dir}/cmds03.in"
test006_desc="match allocate 9 L1+L2 jobs - last one fails (pol=low)"
test_expect_success "${test006_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds006} > cmds006 &&
    ${query} -L ${grugs} -S CA -P low -t 006.R.out < cmds006 &&
    test_cmp 006.R.out ${exp_dir}/006.R.out
'

cmds007="${cmd_dir}/storage-constrained-same-rack.in"
test007_desc="match allocate 4 L1 storage-constrained jobs \
- last one fails (pol=hi)"
test_expect_success "${test007_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds007} > cmds007 &&
    ${query} -L ${grugs} -S CA -P high -t 007.R.out < cmds007 &&
    test_cmp 007.R.out ${exp_dir}/007.R.out
'

cmds008="${cmd_dir}/storage-constrained-global.in"
test008_desc="match allocate 4 L2 storage-constrained jobs \
- last one fails (pol=hi)"
test_expect_success "${test008_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds008} > cmds008 &&
    ${query} -L ${grugs} -S CA -P high -t 008.R.out < cmds008 &&
    test_cmp 008.R.out ${exp_dir}/008.R.out
'

cmds009="${cmd_dir}/storage-constrained-mix.in"
test009_desc="match allocate 4 L1+L2 storage-constrained jobs \
- last one fails (pol=hi)"
test_expect_success "${test009_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds009} > cmds009 &&
    ${query} -L ${grugs} -S CA -P high -t 009.R.out < cmds009 &&
    test_cmp 009.R.out ${exp_dir}/009.R.out
'

cmds011="${cmd_dir}/cmds01.in"
test011_desc="match allocate 9 L1 jobs - last 5 fail (pol=hinodex)"
test_expect_success "${test011_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds011} > cmds011 &&
    ${query} -L ${grugs} -S CA -P hinodex -t 011.R.out < cmds011 &&
    test_cmp 011.R.out ${exp_dir}/011.R.out
'

cmds012="${cmd_dir}/cmds02.in"
test012_desc="match allocate 9 L2 jobs - last 5 fails (pol=hinodex)"
test_expect_success "${test012_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds012} > cmds012 &&
    ${query} -L ${grugs} -S CA -P hinodex -t 012.R.out < cmds012 &&
    test_cmp 012.R.out ${exp_dir}/012.R.out
'

cmds013="${cmd_dir}/cmds03.in"
test013_desc="match allocate 9 L1+L2 jobs - last 5 fail (pol=hinodex)"
test_expect_success "${test013_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds013} > cmds013 &&
    ${query} -L ${grugs} -S CA -P hinodex -t 013.R.out < cmds013 &&
    test_cmp 013.R.out ${exp_dir}/013.R.out
'

cmds014="${cmd_dir}/cmds01.in"
test014_desc="match allocate 9 L1 jobs - last 5 fail (pol=lonodex)"
test_expect_success "${test014_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds014} > cmds014 &&
    ${query} -L ${grugs} -S CA -P lonodex -t 014.R.out < cmds014 &&
    test_cmp 014.R.out ${exp_dir}/014.R.out
'

cmds015="${cmd_dir}/cmds02.in"
test015_desc="match allocate 9 L2 jobs - last 5 fails (pol=lonodex)"
test_expect_success "${test015_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds015} > cmds015 &&
    ${query} -L ${grugs} -S CA -P lonodex -t 015.R.out < cmds015 &&
    test_cmp 015.R.out ${exp_dir}/015.R.out
'

cmds016="${cmd_dir}/cmds03.in"
test016_desc="match allocate 9 L1+L2 jobs - last 5 fail (pol=lonodex)"
test_expect_success "${test016_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds016} > cmds016 &&
    ${query} -L ${grugs} -S CA -P lonodex -t 016.R.out < cmds016 &&
    test_cmp 016.R.out ${exp_dir}/016.R.out
'

cmds017="${cmd_dir}/storage-constrained-same-rack.in"
test017_desc="match allocate 4 L1 storage-constrained jobs \
- last one fails (pol=hinodex)"
test_expect_success "${test017_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds017} > cmds017 &&
    ${query} -L ${grugs} -S CA -P hinodex -t 017.R.out < cmds017 &&
    test_cmp 017.R.out ${exp_dir}/017.R.out
'

cmds018="${cmd_dir}/storage-constrained-global.in"
test018_desc="match allocate 4 L2 storage-constrained jobs \
- last one fails (pol=hinodex)"
test_expect_success "${test018_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds018} > cmds018 &&
    ${query} -L ${grugs} -S CA -P hinodex -t 018.R.out < cmds018 &&
    test_cmp 018.R.out ${exp_dir}/018.R.out
'

cmds019="${cmd_dir}/storage-constrained-mix.in"
test019_desc="match allocate 4 L1+L2 storage-constrained jobs \
- last one fails (pol=hinodex)"
test_expect_success "${test019_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds019} > cmds019 &&
    ${query} -L ${grugs} -S CA -P hinodex -t 019.R.out < cmds019 &&
    test_cmp 019.R.out ${exp_dir}/019.R.out
'

test_done

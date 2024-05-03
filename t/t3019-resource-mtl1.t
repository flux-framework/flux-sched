#!/bin/sh

test_description='Test multi-tiered storage level1: mtl1unit built rack'

. $(dirname $0)/sharness.sh

cmd_dir="${SHARNESS_TEST_SRCDIR}/data/resource/commands/mt-storage/L1"
exp_dir="${SHARNESS_TEST_SRCDIR}/data/resource/expected/mt-storage/L1"
grugs="${SHARNESS_TEST_SRCDIR}/data/resource/grugs/mt-storage/mtl1.graphml"
query="../../src/resource/utilities/resource-query"

cmds001="${cmd_dir}/cmds01.in"
test001_desc="match allocate 9 jobs with 1TB L1 ssds - last one fails (pol=hi)"
test_expect_success "${test001_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds001} > cmds001 &&
    ${query} -L ${grugs} -S CA -P high -t 001.R.out < cmds001 &&
    test_cmp 001.R.out ${exp_dir}/001.R.out
'

cmds002="${cmd_dir}/cmds02.in"
test002_desc="match allocate_orelse_reserve works with MTS L1 (pol=hi)"
test_expect_success "${test002_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds002} > cmds002 &&
    ${query} -L ${grugs} -S CA -P high -t 002.R.out < cmds002 &&
    test_cmp 002.R.out ${exp_dir}/002.R.out
'

cmds003="${cmd_dir}/cmds01.in"
test003_desc="match allocate 9 jobs with 1TB L2 ssds - last one fails (pol=low)"
test_expect_success "${test003_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds003} > cmds003 &&
    ${query} -L ${grugs} -S CA -P low -t 003.R.out < cmds003 &&
    test_cmp 003.R.out ${exp_dir}/003.R.out
'

cmds004="${cmd_dir}/cmds02.in"
test004_desc="match allocate_orelse_reserve works with MTS L1 (pol=low)"
test_expect_success "${test004_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds004} > cmds004 &&
    ${query} -L ${grugs} -S CA -P low -t 004.R.out < cmds004 &&
    test_cmp 004.R.out ${exp_dir}/004.R.out
'

cmds005="${cmd_dir}/cmds-ssd-constrained-alloc.in"
test005_desc="match allocate 5 jobs with 2TB L1 ssds - last one fails (pol=hi)"
test_expect_success "${test005_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds005} > cmds005 &&
    ${query} -L ${grugs} -S CA -P high -t 005.R.out < cmds005 &&
    test_cmp 005.R.out ${exp_dir}/005.R.out
'

cmds006="${cmd_dir}/cmds-ssd-constrained-alloc-orelse.in"
test006_desc="match allocate_orelse_reserve works with MTS L1 (pol=hi)"
test_expect_success "${test006_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds006} > cmds006 &&
    ${query} -L ${grugs} -S CA -P high -t 006.R.out < cmds006 &&
    test_cmp 006.R.out ${exp_dir}/006.R.out
'

cmds007="${cmd_dir}/cmds-ssd-constrained-alloc.in"
test007_desc="match allocate 5 jobs with 2TB L1 ssds - last one fails (pol=low)"
test_expect_success "${test007_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds007} > cmds007 &&
    ${query} -L ${grugs} -S CA -P low -t 007.R.out < cmds007 &&
    test_cmp 007.R.out ${exp_dir}/007.R.out
'

cmds008="${cmd_dir}/cmds-ssd-constrained-alloc-orelse.in"
test008_desc="match allocate_orelse_reserve works with MTS L1 (pol=low)"
test_expect_success "${test008_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds008} > cmds008 &&
    ${query} -L ${grugs} -S CA -P low -t 008.R.out < cmds008 &&
    test_cmp 008.R.out ${exp_dir}/008.R.out
'

cmds009="${cmd_dir}/cmds-ssd-constrained-same-rack-alloc.in"
test009_desc="match allocate 5 jobs with 2TB L1 ssds in same rack \
- last one fails (pol=hi)"
test_expect_success "${test009_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds009} > cmds009 &&
    ${query} -L ${grugs} -S CA -P high -t 009.R.out < cmds009 &&
    test_cmp 009.R.out ${exp_dir}/009.R.out
'

cmds010="${cmd_dir}/cmds-ssd-constrained-same-rack-alloc-orelse.in"
test010_desc="match allocate_orelse_reserve works with MTS L1 (pol=hi)"
test_expect_success "${test010_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds010} > cmds010 &&
    ${query} -L ${grugs} -S CA -P high -t 010.R.out < cmds010 &&
    test_cmp 010.R.out ${exp_dir}/010.R.out
'

cmds011="${cmd_dir}/cmds-ssd-constrained-same-rack-alloc.in"
test011_desc="match allocate 5 jobs with 2TB L1 ssds in same rack \
- last one fails (pol=low)"
test_expect_success "${test011_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds011} > cmds011 &&
    ${query} -L ${grugs} -S CA -P low -t 011.R.out < cmds011 &&
    test_cmp 011.R.out ${exp_dir}/011.R.out
'

cmds012="${cmd_dir}/cmds-ssd-constrained-same-rack-alloc-orelse.in"
test012_desc="match allocate_orelse_reserve works with MTS L1 (pol=low)"
test_expect_success "${test012_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds012} > cmds012 &&
    ${query} -L ${grugs} -S CA -P low -t 012.R.out < cmds012 &&
    test_cmp 012.R.out ${exp_dir}/012.R.out
'

#
# Selection Policy -- node exclusive policies
#

cmds021="${cmd_dir}/cmds01.in"
test021_desc="match allocate 4 jobs with 1TB L1 ssds - rest fails (pol=hinodex)"
test_expect_success "${test021_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds021} > cmds021 &&
    ${query} -L ${grugs} -S CA -P hinodex -t 021.R.out < cmds021 &&
    test_cmp 021.R.out ${exp_dir}/021.R.out
'

cmds022="${cmd_dir}/cmds02.in"
test022_desc="match allocate_orelse_reserve works with MTS L1 (pol=hinodex)"
test_expect_success "${test022_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds022} > cmds022 &&
    ${query} -L ${grugs} -S CA -P hinodex -t 022.R.out < cmds022 &&
    test_cmp 022.R.out ${exp_dir}/022.R.out
'

cmds023="${cmd_dir}/cmds01.in"
test023_desc="match allocate 4 jobs with 1TB L1 ssds - rest fails (pol=lonodex)"
test_expect_success "${test023_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds023} > cmds023 &&
    ${query} -L ${grugs} -S CA -P lonodex -t 023.R.out < cmds023 &&
    test_cmp 023.R.out ${exp_dir}/023.R.out
'

cmds024="${cmd_dir}/cmds02.in"
test024_desc="match allocate_orelse_reserve works with MTS L1 (pol=lonodex)"
test_expect_success "${test024_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds024} > cmds024 &&
    ${query} -L ${grugs} -S CA -P lonodex -t 024.R.out < cmds024 &&
    test_cmp 024.R.out ${exp_dir}/024.R.out
'

cmds025="${cmd_dir}/cmds-ssd-constrained-alloc.in"
test025_desc="match allocate 5 jobs with 2TB L1 ssds - last fails (pol=hinodex)"
test_expect_success "${test025_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds025} > cmds025 &&
    ${query} -L ${grugs} -S CA -P hinodex -t 025.R.out < cmds025 &&
    test_cmp 025.R.out ${exp_dir}/025.R.out
'

cmds026="${cmd_dir}/cmds-ssd-constrained-alloc-orelse.in"
test026_desc="match allocate_orelse_reserve works with MTS L1 (pol=hinodex)"
test_expect_success "${test026_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds026} > cmds026 &&
    ${query} -L ${grugs} -S CA -P hinodex -t 026.R.out < cmds026 &&
    test_cmp 026.R.out ${exp_dir}/026.R.out
'

cmds027="${cmd_dir}/cmds-ssd-constrained-alloc.in"
test027_desc="match allocate 5 jobs with 2TB L1 ssds - last fails (pol=lonodex)"
test_expect_success "${test027_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds027} > cmds027 &&
    ${query} -L ${grugs} -S CA -P lonodex -t 027.R.out < cmds027 &&
    test_cmp 027.R.out ${exp_dir}/027.R.out
'

cmds028="${cmd_dir}/cmds-ssd-constrained-alloc-orelse.in"
test028_desc="match allocate_orelse_reserve works with MTS L1 (pol=lonodex)"
test_expect_success "${test028_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds008} > cmds028 &&
    ${query} -L ${grugs} -S CA -P lonodex -t 028.R.out < cmds028 &&
    test_cmp 028.R.out ${exp_dir}/028.R.out
'

cmds029="${cmd_dir}/cmds-ssd-constrained-same-rack-alloc.in"
test029_desc="match allocate 5 jobs with 2TB L1 ssds in same rack \
- last one fails (pol=hinodex)"
test_expect_success "${test029_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds029} > cmds029 &&
    ${query} -L ${grugs} -S CA -P hinodex -t 029.R.out < cmds029 &&
    test_cmp 029.R.out ${exp_dir}/029.R.out
'

cmds030="${cmd_dir}/cmds-ssd-constrained-same-rack-alloc-orelse.in"
test030_desc="match allocate_orelse_reserve works with MTS L1 (pol=hinodex)"
test_expect_success "${test030_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds030} > cmds030 &&
    ${query} -L ${grugs} -S CA -P hinodex -t 030.R.out < cmds030 &&
    test_cmp 030.R.out ${exp_dir}/030.R.out
'

cmds031="${cmd_dir}/cmds-ssd-constrained-same-rack-alloc.in"
test031_desc="match allocate 5 jobs with 2TB L1 ssds in same rack \
- last one fails (pol=lonodex)"
test_expect_success "${test031_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds031} > cmds031 &&
    ${query} -L ${grugs} -S CA -P lonodex -t 031.R.out < cmds031 &&
    test_cmp 031.R.out ${exp_dir}/031.R.out
'

cmds032="${cmd_dir}/cmds-ssd-constrained-same-rack-alloc-orelse.in"
test032_desc="match allocate_orelse_reserve works with MTS L1 (pol=lonodex)"
test_expect_success "${test032_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds032} > cmds032 &&
    ${query} -L ${grugs} -S CA -P lonodex -t 032.R.out < cmds032 &&
    test_cmp 032.R.out ${exp_dir}/032.R.out
'

test_done

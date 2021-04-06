#!/bin/sh

test_description='Test Scheduling On Tiny Machine Configuration in JGF'

. $(dirname $0)/sharness.sh

cmd_dir="${SHARNESS_TEST_SRCDIR}/data/resource/commands/basics"
exp_dir="${SHARNESS_TEST_SRCDIR}/data/resource/expected/basics"
jgf="${SHARNESS_TEST_SRCDIR}/data/resource/jgfs/tiny.json"
query="../../resource/utilities/resource-query"

#
# Selection Policy -- High ID first (-P high)
#     The resource vertex with higher ID is preferred among its kind
#     (e.g., node1 is preferred over node0 if available)
#

cmds001="${cmd_dir}/cmds01.in"
test001_desc="JGF: allocate 4 jobspecs with 1 slot: 1 socket: 1 core (pol=hi)"
test_expect_success "${test001_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds001} > cmds001 &&
    ${query} -L ${jgf} -f jgf -S CA -P high -t 001.R.out < cmds001 &&
    test_cmp 001.R.out ${exp_dir}/001.R.out
'

cmds002="${cmd_dir}/cmds02.in"
test002_desc="JGF: allocate 5 jobspecs instead - last one must fail (pol=hi)"
test_expect_success "${test002_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds002} > cmds002 &&
    ${query} -L ${jgf} -f jgf -S CA -P high -t 002.R.out < cmds002 &&
    test_cmp 002.R.out ${exp_dir}/002.R.out
'

cmds003="${cmd_dir}/cmds03.in"
test003_desc="JGF: allocate_orelse_reserve 10 jobspecs (pol=hi)"
test_expect_success "${test003_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds003} > cmds003 &&
    ${query} -L ${jgf} -f jgf -S CA -P high -t 003.R.out < cmds003 &&
    test_cmp 003.R.out ${exp_dir}/003.R.out
'

### Note that the memory pool granularity is 2GB
cmds004="${cmd_dir}/cmds04.in"
test004_desc="JGF: allocate 3 jobspecs with 1 slot: 2 sockets (pol=hi)"
test_expect_success "${test004_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds004} > cmds004 &&
    ${query} -L ${jgf} -f jgf -S CA -P high -t 004.R.out < cmds004 &&
    test_cmp 004.R.out ${exp_dir}/004.R.out
'

cmds005="${cmd_dir}/cmds05.in"
test005_desc="JGF: allocate_orelse_reserve 100 jobspecs instead (pol=hi)"
test_expect_success "${test005_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds005} > cmds005 &&
    ${query} -L ${jgf} -f jgf -S CA -P high -t 005.R.out < cmds005 &&
    test_cmp 005.R.out ${exp_dir}/005.R.out
'

cmds006="${cmd_dir}/cmds06.in"
test006_desc="JGF: allocate 2 jobspecs with 2 nodes - last must fail (pol=hi)"
test_expect_success "${test006_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds006} > cmds006 &&
    ${query} -L ${jgf} -f jgf -S CA -P high -t 006.R.out < cmds006 &&
    test_cmp 006.R.out ${exp_dir}/006.R.out
'

# FIXME: investigate the failure
#cmds007="${cmd_dir}/cmds07.in"
#test007_desc="JGF: allocate 9 jobspecs with 1 slot (8c,2m) (pol=hi)"
#test_expect_success "${test007_desc}" '
#    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds007} > cmds007 &&
#    ${query} -L ${jgf} -f jgf -S CA -P high -t 007.R.out < cmds007 &&
#    test_cmp 007.R.out ${exp_dir}/007.R.out
#'

cmds008="${cmd_dir}/cmds08.in"
test008_desc="JGF: 36 core (satisfiable) cores and 37 cores (unsatisfiable)"
test_expect_success "${test008_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds008} > cmds008 &&
    ${query} -L ${jgf} -f jgf -S CA -P high -t 008.R.out < cmds008 &&
    test_cmp 008.R.out ${exp_dir}/008.R.out
'


## Selection Policy -- Low ID first (-P low)
##     The resource vertex with lower ID is preferred among its kind
##     (e.g., node0 is preferred over node1 if available)
##

cmds009="${cmd_dir}/cmds01.in"
test009_desc="JGF: allocate 4 jobspecs with 1 slot: 1 socket: 1 core (pol=low)"
test_expect_success "${test009_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds009} > cmds009 &&
    ${query} -L ${jgf} -f jgf -S CA -P low -t 009.R.out < cmds009 &&
    test_cmp 009.R.out ${exp_dir}/009.R.out
'

cmds010="${cmd_dir}/cmds02.in"
test010_desc="JGF: allocate 5 jobspecs instead - last one must fail (pol=low)"
test_expect_success "${test010_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds010} > cmds010 &&
    ${query} -L ${jgf} -f jgf -S CA -P low -t 010.R.out < cmds010 &&
    test_cmp 010.R.out ${exp_dir}/010.R.out
'

cmds011="${cmd_dir}/cmds03.in"
test011_desc="JGF: attempt to match allocate_orelse_reserve 10 jobspecs"
test_expect_success "${test011_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds011} > cmds011 &&
    ${query} -L ${jgf} -f jgf -S CA -P low -t 011.R.out < cmds011 &&
    test_cmp 011.R.out ${exp_dir}/011.R.out
'

# Note that the memory pool granularity is 2GB
cmds012="${cmd_dir}/cmds04.in"
test012_desc="JGF: allocate 3 jobspecs with 1 slot: 2 sockets (pol=low)"
test_expect_success "${test012_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds012} > cmds012 &&
    ${query} -L ${jgf} -f jgf -S CA -P low -t 012.R.out < cmds012 &&
    test_cmp 012.R.out ${exp_dir}/012.R.out
'

cmds013="${cmd_dir}/cmds05.in"
test013_desc="JGF allocate_orelse_reserve 100 jobspecs instead (pol=low)"
test_expect_success "${test013_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds013} > cmds013 &&
    ${query} -L ${jgf} -f jgf -S CA -P low -t 013.R.out < cmds013 &&
    test_cmp 013.R.out ${exp_dir}/013.R.out
'

cmds014="${cmd_dir}/cmds06.in"
test014_desc="JGF: allocate 2 jobspecs with 2 nodes - last must fail (pol=low)"
test_expect_success "${test014_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds014} > cmds014 &&
    ${query} -L ${jgf} -f jgf -S CA -P low -t 014.R.out < cmds014 &&
    test_cmp 014.R.out ${exp_dir}/014.R.out
'

cmds015="${cmd_dir}/cmds07.in"
test015_desc="JGF: allocate 9 jobspecs with 1 slot (8c,2m) (pol=low)"
test_expect_success "${test015_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds015} > cmds015 &&
    ${query} -L ${jgf} -f jgf -S CA -P low -t 015.R.out < cmds015 &&
    test_cmp 015.R.out ${exp_dir}/015.R.out
'

cmds016="${cmd_dir}/cmds08.in"
test016_desc="JGF: 36 core (satisfiable) cores and 37 cores (unsatisfiable)"
test_expect_success "${test016_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds016} > cmds016 &&
    ${query} -L ${jgf} -f jgf -S CA -P low -t 016.R.out < cmds016 &&
    test_cmp 016.R.out ${exp_dir}/016.R.out
'

cmds040="${cmd_dir}/cmds40.in"
test040_desc="JGF: Once all sockets are exclusively allocated, no jobs match"
test_expect_success "${test040_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds040} > cmds040 &&
    ${query} -L ${jgf} -f jgf -S CA -P low -t 040.R.out < cmds040 &&
    test_cmp 040.R.out ${exp_dir}/040.R.out
'

#
# Selection Policy -- First Match (-P first)
#

cmds041="${cmd_dir}/cmds01.in"
test041_desc="match alloc 4 jobs with 1 slot: 1 socket: 1 core (pol=first)"
test_expect_success "${test041_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds041} > cmds041 &&
    ${query} -L ${jgf} -f jgf -S CA -P first -t 041.R.out < cmds041 &&
    test_cmp 041.R.out ${exp_dir}/041.R.out
'

cmds042="${cmd_dir}/cmds02.in"
test042_desc="match allocate 5 jobs instead - last one must fail (pol=first)"
test_expect_success "${test042_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds042} > cmds042 &&
    ${query} -L ${jgf} -f jgf -S CA -P first -t 042.R.out < cmds042 &&
    test_cmp 042.R.out ${exp_dir}/042.R.out
'

cmds043="${cmd_dir}/cmds03.in"
test043_desc="match allocate_orelse_reserve 10 jobspecs (pol=first)"
test_expect_success "${test043_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds043} > cmds043 &&
    ${query} -L ${jgf} -f jgf -S CA -P first -t 043.R.out < cmds043 &&
    test_cmp 043.R.out ${exp_dir}/043.R.out
'

cmds044="${cmd_dir}/cmds04.in"
test044_desc="match allocate 3 jobspecs with 1 slot: 2 sockets (pol=first)"
test_expect_success "${test044_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds044} > cmds044 &&
    ${query} -L ${jgf} -f jgf -S CA -P first -t 044.R.out < cmds044 &&
    test_cmp 044.R.out ${exp_dir}/044.R.out
'

cmds045="${cmd_dir}/cmds05.in"
test045_desc="match allocate_orelse_reserve 100 jobspecs instead (pol=first)"
test_expect_success "${test045_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds045} > cmds045 &&
    ${query} -L ${jgf} -f jgf -S CA -P first -t 045.R.out < cmds045 &&
    test_cmp 045.R.out ${exp_dir}/045.R.out
'

cmds046="${cmd_dir}/cmds06.in"
test046_desc="match allocate 2 jobspecs with 2 nodes - last must fail (pol=first)"
test_expect_success "${test046_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds046} > cmds046 &&
    ${query} -L ${jgf} -f jgf -S CA -P first -t 046.R.out < cmds046 &&
    test_cmp 046.R.out ${exp_dir}/046.R.out
'

cmds048="${cmd_dir}/cmds08.in"
test048_desc="36 core (satisfiable) cores and 37 cores (unsatisfiable) (pol=first)"
test_expect_success "${test048_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds048} > cmds048 &&
    ${query} -L ${jgf} -f jgf -S CA -P first -t 048.R.out < cmds048 &&
    test_cmp 048.R.out ${exp_dir}/048.R.out
'

test_done

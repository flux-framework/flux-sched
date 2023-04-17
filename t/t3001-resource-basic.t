#!/bin/sh

test_description='Test Scheduling On Tiny Machine Configuration'

. $(dirname $0)/sharness.sh

cmd_dir="${SHARNESS_TEST_SRCDIR}/data/resource/commands/basics"
exp_dir="${SHARNESS_TEST_SRCDIR}/data/resource/expected/basics"
grugs="${SHARNESS_TEST_SRCDIR}/data/resource/grugs/tiny.graphml"
query="../../resource/utilities/resource-query"

#
# Selection Policy -- High ID first (-P high)
#     The resource vertex with higher ID is preferred among its kind
#     (e.g., node1 is preferred over node0 if available)
#

cmds001="${cmd_dir}/cmds01.in"
test001_desc="match allocate 4 jobspecs with 1 slot: 1 socket: 1 core (pol=hi)"
test_expect_success "${test001_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds001} > cmds001 &&
    ${query} -L ${grugs} -S CA -P high -t 001.R.out < cmds001 &&
    test_cmp 001.R.out ${exp_dir}/001.R.out
'

cmds002="${cmd_dir}/cmds02.in"
test002_desc="match allocate 5 jobspecs instead - last one must fail (pol=hi)"
test_expect_success "${test002_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds002} > cmds002 &&
    ${query} -L ${grugs} -S CA -P high -t 002.R.out < cmds002 &&
    test_cmp 002.R.out ${exp_dir}/002.R.out
'

cmds003="${cmd_dir}/cmds03.in"
test003_desc="match allocate_orelse_reserve 10 jobspecs (pol=hi)"
test_expect_success "${test003_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds003} > cmds003 &&
    ${query} -L ${grugs} -S CA -P high -t 003.R.out < cmds003 &&
    test_cmp 003.R.out ${exp_dir}/003.R.out
'

### Note that the memory pool granularity is 2GB
cmds004="${cmd_dir}/cmds04.in"
test004_desc="match allocate 3 jobspecs with 1 slot: 2 sockets (pol=hi)"
test_expect_success "${test004_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds004} > cmds004 &&
    ${query} -L ${grugs} -S CA -P high -t 004.R.out < cmds004 &&
    test_cmp 004.R.out ${exp_dir}/004.R.out
'

cmds005="${cmd_dir}/cmds05.in"
test005_desc="match allocate_orelse_reserve 100 jobspecs instead (pol=hi)"
test_expect_success "${test005_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds005} > cmds005 &&
    ${query} -L ${grugs} -S CA -P high -t 005.R.out < cmds005 &&
    test_cmp 005.R.out ${exp_dir}/005.R.out
'

cmds006="${cmd_dir}/cmds06.in"
test006_desc="match allocate 2 jobspecs with 2 nodes - last must fail (pol=hi)"
test_expect_success "${test006_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds006} > cmds006 &&
    ${query} -L ${grugs} -S CA -P high -t 006.R.out < cmds006 &&
    test_cmp 006.R.out ${exp_dir}/006.R.out
'

cmds008="${cmd_dir}/cmds08.in"
test008_desc="1s,18c slot (sat) and 1s,19c slot (unsat) (pol=hi)"
test_expect_success "${test008_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds008} > cmds008 &&
    ${query} -L ${grugs} -S CA -P high -t 008.R.out < cmds008 &&
    test_cmp 008.R.out ${exp_dir}/008.R.out
'


#
# Selection Policy -- Low ID first (-P low)
#     The resource vertex with lower ID is preferred among its kind
#     (e.g., node0 is preferred over node1 if available)
#

cmds009="${cmd_dir}/cmds01.in"
test009_desc="match allocate 4 jobspecs with 1 slot: 1 socket: 1 core (pol=low)"
test_expect_success "${test009_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds009} > cmds009 &&
    ${query} -L ${grugs} -S CA -P low -t 009.R.out < cmds009 &&
    test_cmp 009.R.out ${exp_dir}/009.R.out
'

cmds010="${cmd_dir}/cmds02.in"
test010_desc="match allocate 5 jobspecs instead - last one must fail (pol=low)"
test_expect_success "${test010_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds010} > cmds010 &&
    ${query} -L ${grugs} -S CA -P low -t 010.R.out < cmds010 &&
    test_cmp 010.R.out ${exp_dir}/010.R.out
'

cmds011="${cmd_dir}/cmds03.in"
test011_desc="attempt to match allocate_orelse_reserve 10 jobspecs (pol=low)"
test_expect_success "${test011_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds011} > cmds011 &&
    ${query} -L ${grugs} -S CA -P low -t 011.R.out < cmds011 &&
    test_cmp 011.R.out ${exp_dir}/011.R.out
'

# Note that the memory pool granularity is 2GB
cmds012="${cmd_dir}/cmds04.in"
test012_desc="match allocate 3 jobspecs with 1 slot: 2 sockets (pol=low)"
test_expect_success "${test012_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds012} > cmds012 &&
    ${query} -L ${grugs} -S CA -P low -t 012.R.out < cmds012 &&
    test_cmp 012.R.out ${exp_dir}/012.R.out
'

cmds013="${cmd_dir}/cmds05.in"
test013_desc="match allocate_orelse_reserve 100 jobspecs instead (pol=low)"
test_expect_success "${test013_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds013} > cmds013 &&
    ${query} -L ${grugs} -S CA -P low -t 013.R.out < cmds013 &&
    test_cmp 013.R.out ${exp_dir}/013.R.out
'

cmds014="${cmd_dir}/cmds06.in"
test014_desc="match allocate 2 jobspecs with 2 nodes - last must fail (pol=low)"
test_expect_success "${test014_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds014} > cmds014 &&
    ${query} -L ${grugs} -S CA -P low -t 014.R.out < cmds014 &&
    test_cmp 014.R.out ${exp_dir}/014.R.out
'

cmds015="${cmd_dir}/cmds07.in"
test015_desc="match allocate 9 jobspecs with 1 slot (8c,2m) (pol=low)"
test_expect_success "${test015_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds015} > cmds015 &&
    ${query} -L ${grugs} -S CA -P low -t 015.R.out < cmds015 &&
    test_cmp 015.R.out ${exp_dir}/015.R.out
'

cmds016="${cmd_dir}/cmds08.in"
test016_desc="1s,18c slot (sat) and 1s,19c slot (unsat) (pol=low)"
test_expect_success "${test016_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds016} > cmds016 &&
    ${query} -L ${grugs} -S CA -P low -t 016.R.out < cmds016 &&
    test_cmp 016.R.out ${exp_dir}/016.R.out
'

cmds040="${cmd_dir}/cmds40.in"
test040_desc="Once all sockets are exclusively allocated, no jobs can match"
test_expect_success "${test040_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds040} > cmds040 &&
    ${query} -L ${grugs} -S CA -P low -t 040.R.out < cmds040 &&
    test_cmp 040.R.out ${exp_dir}/040.R.out
'

#
# Selection Policy -- First Match (-P first)
#

cmds041="${cmd_dir}/cmds01.in"
test041_desc="match alloc 4 jobs with 1 slot: 1 socket: 1 core (pol=first)"
test_expect_success "${test041_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds041} > cmds041 &&
    ${query} -L ${grugs} -S CA -P first -t 041.R.out < cmds041 &&
    test_cmp 041.R.out ${exp_dir}/041.R.out
'

cmds042="${cmd_dir}/cmds02.in"
test042_desc="match allocate 5 jobs instead - last one must fail (pol=first)"
test_expect_success "${test042_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds042} > cmds042 &&
    ${query} -L ${grugs} -S CA -P first -t 042.R.out < cmds042 &&
    test_cmp 042.R.out ${exp_dir}/042.R.out
'

cmds043="${cmd_dir}/cmds03.in"
test043_desc="match allocate_orelse_reserve 10 jobspecs (pol=first)"
test_expect_success "${test043_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds043} > cmds043 &&
    ${query} -L ${grugs} -S CA -P first -t 043.R.out < cmds043 &&
    test_cmp 043.R.out ${exp_dir}/043.R.out
'

cmds044="${cmd_dir}/cmds04.in"
test044_desc="match allocate 3 jobspecs with 1 slot: 2 sockets (pol=first)"
test_expect_success "${test044_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds044} > cmds044 &&
    ${query} -L ${grugs} -S CA -P first -t 044.R.out < cmds044 &&
    test_cmp 044.R.out ${exp_dir}/044.R.out
'

cmds045="${cmd_dir}/cmds05.in"
test045_desc="match allocate_orelse_reserve 100 jobspecs instead (pol=first)"
test_expect_success "${test045_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds045} > cmds045 &&
    ${query} -L ${grugs} -S CA -P first -t 045.R.out < cmds045 &&
    test_cmp 045.R.out ${exp_dir}/045.R.out
'

cmds046="${cmd_dir}/cmds06.in"
test046_desc="match allocate 2 jobspecs with 2 nodes - last must fail (pol=first)"
test_expect_success "${test046_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds046} > cmds046 &&
    ${query} -L ${grugs} -S CA -P first -t 046.R.out < cmds046 &&
    test_cmp 046.R.out ${exp_dir}/046.R.out
'

cmds048="${cmd_dir}/cmds08.in"
test048_desc="1s,18c slot (sat) and 1s,19c slot (unsat) (pol=first)"
test_expect_success "${test048_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds048} > cmds048 &&
    ${query} -L ${grugs} -S CA -P first -t 048.R.out < cmds048 &&
    test_cmp 048.R.out ${exp_dir}/048.R.out
'

#
# Selection Policy -- High node first (-P hinode)
#     Within node-level locality constraint, the resource vertex
#     with higher ID is preferred among its kind
#     (e.g., core1 is preferred over core0 but those in node1
#            is selected first before those in node0 is selected)
#     If a node-level locality constraint is already specified in
#     the jobspec, this policy is identical to high: e.g.,
#         node[2]->slot[2]->core[2].
#     Only when node-level locality constraint is absent, the
#     match behavior is deviated from high: e.g.,
#         slot[2]->core[2].

cmds051="${cmd_dir}/cmds01.in"
test051_desc="match allocate 4 jobs with 1 slot: 1 socket: 1 core (pol=hinode)"
test_expect_success "${test051_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds051} > cmds051 &&
    ${query} -L ${grugs} -S CA -P hinode -t 051.R.out < cmds051 &&
    test_cmp 051.R.out ${exp_dir}/001.R.out
'

cmds052="${cmd_dir}/cmds02.in"
test052_desc="match allocate 5 jobs instead - last one must fail (pol=hinode)"
test_expect_success "${test052_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds052} > cmds052 &&
    ${query} -L ${grugs} -S CA -P hinode -t 052.R.out < cmds052 &&
    test_cmp 052.R.out ${exp_dir}/002.R.out
'

cmds053="${cmd_dir}/cmds03.in"
test053_desc="match allocate_orelse_reserve 10 jobspecs (pol=hinode)"
test_expect_success "${test053_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds053} > cmds053 &&
    ${query} -L ${grugs} -S CA -P hinode -t 053.R.out < cmds053 &&
    test_cmp 053.R.out ${exp_dir}/003.R.out
'

### Note that the memory pool granularity is 2GB
cmds054="${cmd_dir}/cmds04.in"
test054_desc="match allocate 3 jobs with 1 slot: 2 sockets (pol=hinode)"
test_expect_success "${test054_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds054} > cmds054 &&
    ${query} -L ${grugs} -S CA -P hinode -t 054.R.out < cmds054 &&
    test_cmp 054.R.out ${exp_dir}/004.R.out
'

cmds055="${cmd_dir}/cmds05.in"
test055_desc="match allocate_orelse_reserve 100 jobs instead (pol=hinode)"
test_expect_success "${test055_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds055} > cmds055 &&
    ${query} -L ${grugs} -S CA -P hinode -t 055.R.out < cmds055 &&
    test_cmp 055.R.out ${exp_dir}/005.R.out
'

cmds056="${cmd_dir}/cmds06.in"
test056_desc="match allocate 2 jobs with 2 nodes - last must fail (pol=hinode)"
test_expect_success "${test056_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds056} > cmds056 &&
    ${query} -L ${grugs} -S CA -P hinode -t 056.R.out < cmds056 &&
    test_cmp 056.R.out ${exp_dir}/006.R.out
'

cmds057="${cmd_dir}/cmds07.in"
test057_desc="match allocate 9 jobs with 1 slot (8c,2m) (pol=hinode)"
test_expect_success "${test057_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds057} > cmds057 &&
    ${query} -L ${grugs} -S CA -P hinode -t 057.R.out < cmds057 &&
    test_cmp 057.R.out ${exp_dir}/057.R.out
'

cmds058="${cmd_dir}/cmds08.in"
test058_desc="1s,18c slot (sat) and 1s,19c slot (unsat) (pol=hinode)"
test_expect_success "${test058_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds058} > cmds058 &&
    ${query} -L ${grugs} -S CA -P hinode -t 058.R.out < cmds058 &&
    test_cmp 058.R.out ${exp_dir}/008.R.out
'

#
# Selection Policy -- Low node first (-P lonode)
#     Within node-level locality constraint, the resource vertex
#     with lower ID is preferred among its kind
#     (e.g., core0 is preferred over core1 but those in node0
#            is selected first before those in node1 is selected)
#     If a node-level locality constraint is already specified in
#     the jobspec, this policy is identical to low: e.g.,
#         node[2]->slot[2]->core[2].
#     Only when node-level locality constraint is absent, the
#     match behavior is deviated from low: e.g.,
#         slot[2]->core[2].

cmds061="${cmd_dir}/cmds01.in"
test061_desc="match allocate 4 jobs with 1 slot: 1 socket: 1 core (pol=lonode)"
test_expect_success "${test061_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds061} > cmds061 &&
    ${query} -L ${grugs} -S CA -P lonode -t 061.R.out < cmds061 &&
    test_cmp 061.R.out ${exp_dir}/009.R.out
'

cmds062="${cmd_dir}/cmds02.in"
test062_desc="match allocate 5 jobs instead - last one must fail (pol=lonode)"
test_expect_success "${test062_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds062} > cmds062 &&
    ${query} -L ${grugs} -S CA -P lonode -t 062.R.out < cmds062 &&
    test_cmp 062.R.out ${exp_dir}/010.R.out
'

cmds063="${cmd_dir}/cmds03.in"
test063_desc="match allocate_orelse_reserve 10 jobspecs (pol=lonode)"
test_expect_success "${test063_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds063} > cmds063 &&
    ${query} -L ${grugs} -S CA -P lonode -t 063.R.out < cmds063 &&
    test_cmp 063.R.out ${exp_dir}/011.R.out
'

### Note that the memory pool granularity is 2GB
cmds064="${cmd_dir}/cmds04.in"
test064_desc="match allocate 3 jobs with 1 slot: 2 sockets (pol=lonode)"
test_expect_success "${test064_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds064} > cmds064 &&
    ${query} -L ${grugs} -S CA -P lonode -t 064.R.out < cmds064 &&
    test_cmp 064.R.out ${exp_dir}/012.R.out
'

cmds065="${cmd_dir}/cmds05.in"
test065_desc="match allocate_orelse_reserve 100 jobs instead (pol=lonode)"
test_expect_success "${test065_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds065} > cmds065 &&
    ${query} -L ${grugs} -S CA -P lonode -t 065.R.out < cmds065 &&
    test_cmp 065.R.out ${exp_dir}/013.R.out
'

cmds066="${cmd_dir}/cmds06.in"
test066_desc="match allocate 2 jobs with 2 nodes - last must fail (pol=lonode)"
test_expect_success "${test066_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds066} > cmds066 &&
    ${query} -L ${grugs} -S CA -P lonode -t 066.R.out < cmds066 &&
    test_cmp 066.R.out ${exp_dir}/014.R.out
'

cmds067="${cmd_dir}/cmds07.in"
test067_desc="match allocate 9 jobs with 1 slot (8c,2m) (pol=lonode)"
test_expect_success "${test067_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds067} > cmds067 &&
    ${query} -L ${grugs} -S CA -P lonode -t 067.R.out < cmds067 &&
    test_cmp 067.R.out ${exp_dir}/067.R.out
'

cmds068="${cmd_dir}/cmds08.in"
test068_desc="1s,18c slot (sat) and 1s,19c slot (unsat) (pol=lonode)"
test_expect_success "${test068_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds068} > cmds068 &&
    ${query} -L ${grugs} -S CA -P lonode -t 068.R.out < cmds068 &&
    test_cmp 068.R.out ${exp_dir}/016.R.out
'

cmds069="${cmd_dir}/cmds40.in"
test069_desc="Once all sockets are exclusively allocated, no jobs can match"
test_expect_success "${test069_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds069} > cmds069 &&
    ${query} -L ${grugs} -S CA -P lonode -t 069.R.out < cmds069 &&
    test_cmp 069.R.out ${exp_dir}/040.R.out
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

cmds071="${cmd_dir}/cmds01.in"
test071_desc="match allocate 4 jobs with 1 slot: 1 socket: 1 core (pol=hinodex)"
test_expect_success "${test071_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds071} > cmds071 &&
    ${query} -L ${grugs} -S CA -P hinodex -t 071.R.out < cmds071 &&
    test_cmp 071.R.out ${exp_dir}/071.R.out
'

cmds072="${cmd_dir}/cmds02.in"
test072_desc="match allocate 5 jobs instead - last three must fail (pol=hinodex)"
test_expect_success "${test072_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds072} > cmds072 &&
    ${query} -L ${grugs} -S CA -P hinodex -t 072.R.out < cmds072 &&
    test_cmp 072.R.out ${exp_dir}/072.R.out
'

cmds073="${cmd_dir}/cmds03.in"
test073_desc="match allocate_orelse_reserve 10 jobspecs (pol=hinodex)"
test_expect_success "${test073_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds073} > cmds073 &&
    ${query} -L ${grugs} -S CA -P hinodex -t 073.R.out < cmds073 &&
    test_cmp 073.R.out ${exp_dir}/073.R.out
'

### Note that the memory pool granularity is 2GB
cmds074="${cmd_dir}/cmds04.in"
test074_desc="match allocate 3 jobs with 1 slot: 2 sockets (pol=hinodex)"
test_expect_success "${test074_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds074} > cmds074 &&
    ${query} -L ${grugs} -S CA -P hinodex -t 074.R.out < cmds074 &&
    test_cmp 074.R.out ${exp_dir}/074.R.out
'

cmds075="${cmd_dir}/cmds05.in"
test075_desc="match allocate_orelse_reserve 100 jobs instead (pol=hinodex)"
test_expect_success "${test075_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds075} > cmds075 &&
    ${query} -L ${grugs} -S CA -P hinodex -t 075.R.out < cmds075 &&
    test_cmp 075.R.out ${exp_dir}/075.R.out
'

cmds076="${cmd_dir}/cmds06.in"
test076_desc="match allocate 2 jobs with 2 nodes - last must fail (pol=hinodex)"
test_expect_success "${test076_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds076} > cmds076 &&
    ${query} -L ${grugs} -S CA -P hinodex -t 076.R.out < cmds076 &&
    test_cmp 076.R.out ${exp_dir}/076.R.out
'

cmds077="${cmd_dir}/cmds07.in"
test077_desc="match allocate 9 jobs with 1 slot (8c,2m) (pol=hinodex)"
test_expect_success "${test077_desc}" '
sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds077} > cmds077 &&
    ${query} -L ${grugs} -S CA -P hinodex -t 077.R.out < cmds077 &&
test_cmp 077.R.out ${exp_dir}/077.R.out
'

cmds078="${cmd_dir}/cmds08.in"
test078_desc="1s,18c slot (sat) and 1s,19c slot (unsat) (pol=hinodex)"
test_expect_success "${test078_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds078} > cmds078 &&
    ${query} -L ${grugs} -S CA -P hinodex -t 078.R.out < cmds078 &&
    test_cmp 078.R.out ${exp_dir}/078.R.out
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

cmds081="${cmd_dir}/cmds01.in"
test081_desc="match allocate 4 jobs with 1 slot: 1 socket: 1 core (pol=lonodex)"
test_expect_success "${test081_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds081} > cmds081 &&
    ${query} -L ${grugs} -S CA -P lonodex -t 081.R.out < cmds081 &&
    test_cmp 081.R.out ${exp_dir}/081.R.out
'

cmds082="${cmd_dir}/cmds02.in"
test082_desc="match allocate 5 jobs instead - last three must fail (pol=lonodex)"
test_expect_success "${test082_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds082} > cmds082 &&
    ${query} -L ${grugs} -S CA -P lonodex -t 082.R.out < cmds082 &&
    test_cmp 082.R.out ${exp_dir}/082.R.out
'

cmds083="${cmd_dir}/cmds03.in"
test083_desc="match allocate_orelse_reserve 10 jobspecs (pol=lonodex)"
test_expect_success "${test083_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds083} > cmds083 &&
    ${query} -L ${grugs} -S CA -P lonodex -t 083.R.out < cmds083 &&
    test_cmp 083.R.out ${exp_dir}/083.R.out
'

### Note that the memory pool granularity is 2GB
cmds084="${cmd_dir}/cmds04.in"
test084_desc="match allocate 3 jobs with 1 slot: 2 sockets (pol=lonodex)"
test_expect_success "${test084_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds084} > cmds084 &&
    ${query} -L ${grugs} -S CA -P lonodex -t 084.R.out < cmds084 &&
    test_cmp 084.R.out ${exp_dir}/084.R.out
'

cmds085="${cmd_dir}/cmds05.in"
test085_desc="match allocate_orelse_reserve 100 jobs instead (pol=lonodex)"
test_expect_success "${test085_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds085} > cmds085 &&
    ${query} -L ${grugs} -S CA -P lonodex -t 085.R.out < cmds085 &&
    test_cmp 085.R.out ${exp_dir}/085.R.out
'

cmds086="${cmd_dir}/cmds06.in"
test086_desc="match allocate 2 jobs with 2 nodes - last must fail (pol=lonodex)"
test_expect_success "${test086_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds086} > cmds086 &&
    ${query} -L ${grugs} -S CA -P lonodex -t 086.R.out < cmds086 &&
    test_cmp 086.R.out ${exp_dir}/086.R.out
'

cmds087="${cmd_dir}/cmds07.in"
test087_desc="match allocate 9 jobs with 1 slot (8c,2m) (pol=lonodex)"
test_expect_success "${test087_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds087} > cmds087 &&
    ${query} -L ${grugs} -S CA -P lonodex -t 087.R.out < cmds087 &&
    test_cmp 087.R.out ${exp_dir}/087.R.out
'

cmds088="${cmd_dir}/cmds08.in"
test088_desc="1s,18c slot (sat) and 1s,19c slot (unsat) (pol=lonodex)"
test_expect_success "${test088_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds088} > cmds088 &&
    ${query} -L ${grugs} -S CA -P lonodex -t 088.R.out < cmds088 &&
    test_cmp 088.R.out ${exp_dir}/088.R.out
'


test_done

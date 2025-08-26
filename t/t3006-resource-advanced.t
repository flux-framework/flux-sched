#!/bin/sh

test_description='Test advanced cases (Burst Buffers and Heterogenous)'

. $(dirname $0)/sharness.sh

cmd_dir="${SHARNESS_TEST_SRCDIR}/data/resource/commands/advanced"
exp_dir="${SHARNESS_TEST_SRCDIR}/data/resource/expected/advanced"
grugs="${SHARNESS_TEST_SRCDIR}/data/resource/grugs/advanced_test.graphml"
disag="${SHARNESS_TEST_SRCDIR}/data/resource/grugs/disaggr.graphml"
ssd="${SHARNESS_TEST_SRCDIR}/data/resource/jgfs/issue1260.json"
query="../../resource/utilities/resource-query"

#
# Selection Policy -- High ID first (-P high)
#     The resource vertex with higher ID is preferred among its kind
#     (e.g., node1 is preferred over node0 if available)
#

cmds001="${cmd_dir}/cmds01.in"
test001_desc="match allocate with advanced criteria including BB (pol=hi)"
test_expect_success "${test001_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds001} > cmds001 &&
    ${query} -L ${grugs} -S CA -P high -t 001.R.out < cmds001 &&
    test_cmp 001.R.out ${exp_dir}/001.R.out
'

#
# Selection Policy -- Low ID first (-P low)
#     The resource vertex with higher ID is preferred among its kind
#     (e.g., node0 is preferred over node1 if available)
#

cmds002="${cmd_dir}/cmds01.in"
test002_desc="match allocate with advanced criteria including BB (pol=low)"
test_expect_success "${test002_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds002} > cmds002 &&
    ${query} -L ${grugs} -S CA -P low -t 002.R.out < cmds002 &&
    test_cmp 002.R.out ${exp_dir}/002.R.out
'

#
# Selection Policy -- High ID first (-P high)
#     The resource vertex with higher ID is preferred among its kind
#     (e.g., node1 is preferred over node0 if available)
#
cmds003="${cmd_dir}/cmds02.in"
test003_desc="match for a large system with disaggregated resources (pol=high)"
test_expect_success LONGTEST "${test003_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds003} > cmds003 &&
    ${query} -L ${disag} -S CA -P high -t 003.R.out -r 400000< cmds003 &&
    test_cmp 003.R.out ${exp_dir}/003.R.out
'

#
# Selection Policy -- Low ID first (-P low)
#     The resource vertex with higher ID is preferred among its kind
#     (e.g., node0 is preferred over node1 if available)
#
cmds004="${cmd_dir}/cmds02.in"
test004_desc="match for a large system with disaggregated resources (pol=low)"
test_expect_success LONGTEST "${test004_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds004} > cmds004 &&
    ${query} -L ${disag} -S CA -P low -t 004.R.out -r 400000 < cmds004 &&
    test_cmp 004.R.out ${exp_dir}/004.R.out
'

cmds005="${cmd_dir}/cmds05.in"
test005_desc="Resource labels in jobspec carry through to JGF R"
test_expect_success "${test005_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds005} > cmds005 &&
    ${query} -d -L ${disag} -F jgf -S CA -P low -t 005.R.out \
-r 400000 < cmds005 &&
    grep -v "INFO" 005.R.out > 005.jgf.json &&
    jq -r ".graph.nodes[] | .metadata | .ephemeral.label" 005.jgf.json \
       | sort | uniq -c > 005.counts &&
    test_cmp -w 005.counts ${exp_dir}/005.counts
'

# Test to ensure scheduling leaf vertices in clusters with ssds and nodes
# connected to rack and cluster vertices, respectively, doesn't result in
# match errors (see issue 1260).
#
cmds006="${cmd_dir}/cmds06.in"
test006_desc="match vertices in asymmetrical system (pol=lonodex)"
test_expect_success "${test006_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds006} > cmds006 &&
    ${query} -L ${ssd} -S CA -P lonodex -f jgf -t 006.R.out < cmds006 2> err.out &&
    cat err.out >> 006.R.out &&
    test_cmp 006.R.out ${exp_dir}/006.R.out
'

test_done

#!/bin/sh

test_description='Test Resource Status On Tiny Machine Configuration'

. $(dirname $0)/sharness.sh

cmd_dir="${SHARNESS_TEST_SRCDIR}/data/resource/commands/status"
exp_dir="${SHARNESS_TEST_SRCDIR}/data/resource/expected/status"
grugs="${SHARNESS_TEST_SRCDIR}/data/resource/grugs/tiny.graphml"
grug_aux="${SHARNESS_TEST_SRCDIR}/data/resource/grugs/coarse_iobw.graphml"
query="../../resource/utilities/resource-query"

#
# Selection Policy -- High ID first (-P high)
#     The resource vertex with higher ID is preferred among its kind
#     (e.g., node1 is preferred over node0 if available)
#

test001_desc="error on malformed get-status "
test_expect_success "${test001_desc}" '
    ${query} -L ${grugs} -F jgf -S CA -P high < ${cmd_dir}/cmds01 \
2> 001.R.out &&
    test_cmp 001.R.out ${exp_dir}/001.R.out
'

test002_desc="exit on get-status of nonexistent resource "
test_expect_success "${test002_desc}" '
    ${query} -L ${grugs} -F jgf -S CA -P high < ${cmd_dir}/cmds02 \
> 002.R.out &&
    test_cmp 002.R.out ${exp_dir}/002.R.out
'

test003_desc="get resource vertex status "
test_expect_success "${test003_desc}" '
    ${query} -L ${grugs} -F jgf -S CA -P high < ${cmd_dir}/cmds03 \
> 003.R.out &&
    test_cmp 003.R.out ${exp_dir}/003.R.out
'

test004_desc="error on malformed get-status "
test_expect_success "${test004_desc}" '
    ${query} -L ${grugs} -F jgf -S CA -P high < ${cmd_dir}/cmds04 \
2> 004.R.out &&
    test_cmp 004.R.out ${exp_dir}/004.R.out
'

test005_desc="exit on set-status of nonexistent resource "
test_expect_success "${test005_desc}" '
    ${query} -L ${grugs} -F jgf -S CA -P high < ${cmd_dir}/cmds05 \
> 005.R.out &&
    test_cmp 005.R.out ${exp_dir}/005.R.out
'

test006_desc="error on set-status of nonexistent status "
test_expect_success "${test006_desc}" '
    ${query} -L ${grugs} -F jgf -S CA -P high < ${cmd_dir}/cmds06 \
2> 006.R.out &&
    test_cmp 006.R.out ${exp_dir}/006.R.out
'

test007_desc="change resource vertex status and get output "
test_expect_success "${test007_desc}" '
    ${query} -L ${grugs} -F jgf -S CA -P high < ${cmd_dir}/cmds07 \
> 007.R.out &&
    test_cmp 007.R.out ${exp_dir}/007.R.out
'

test008_desc="change aux hierarchy resource vertex status and get output  "
test_expect_success "${test008_desc}" '
    ${query} -L ${grug_aux} -F jgf -S CA -P high < ${cmd_dir}/cmds08 \
> 008.R.out &&
    test_cmp 008.R.out ${exp_dir}/008.R.out
'

cmds009="${cmd_dir}/cmds09.in"
test009_desc="attempt to allocate basic job when the cluster is marked up"
test_expect_success "${test009_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds009} > cmds009 &&
    ${query} -L ${grugs} -F simple -S CA -P high -t 009.R.out < cmds009 &&
    test_cmp 009.R.out ${exp_dir}/009.R.out
'

cmds010="${cmd_dir}/cmds10.in"
test010_desc="attempt to allocate basic job when the cluster root is set down"
test_expect_success "${test010_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds010} > cmds010 &&
    ${query} -L ${grugs} -F simple -S CA -P high -t 010.R.out < cmds010 &&
    test_cmp 010.R.out ${exp_dir}/010.R.out
'

cmds011="${cmd_dir}/cmds11.in"
test011_desc="attempt to allocate basic job when node1 is set down"
test_expect_success "${test011_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds011} > cmds011 &&
    ${query} -L ${grugs} -F simple -S CA -P high -t 011.R.out < cmds011 &&
    test_cmp 011.R.out ${exp_dir}/011.R.out
'

cmds012="${cmd_dir}/cmds12.in"
test012_desc="attempt to allocate basic job when node1 is set down then up"
test_expect_success "${test012_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds012} > cmds012 &&
    ${query} -L ${grugs} -F simple -S CA -P high -t 012.R.out < cmds012 &&
    test_cmp 012.R.out ${exp_dir}/012.R.out
'

cmds013="${cmd_dir}/cmds13.in"
test013_desc="attempt to allocate basic job when cluster is set down then up"
test_expect_success "${test013_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds013} > cmds013 &&
    ${query} -L ${grugs} -F simple -S CA -P high -t 013.R.out < cmds013 &&
    test_cmp 013.R.out ${exp_dir}/013.R.out
'

cmds014="${cmd_dir}/cmds14.in"
test014_desc="attempt to allocate w/sat cluster when node1 is set down then up"
test_expect_success "${test014_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds014} > cmds014 &&
    ${query} -L ${grugs} -F simple -S CA -P high -t 014.R.out < cmds014 &&
    test_cmp 014.R.out ${exp_dir}/014.R.out
'

cmds015="${cmd_dir}/cmds15.in"
test015_desc="attempt to allocate/res cluster when node1 is set down then up"
test_expect_success "${test015_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds015} > cmds015 &&
    ${query} -L ${grugs} -F simple -S CA -P high -t 015.R.out < cmds015 &&
    test_cmp 015.R.out ${exp_dir}/015.R.out
'

cmds016="${cmd_dir}/cmds16.in"
test016_desc="submit unsatisfiable request to cluster with one node down"
test_expect_success "${test016_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds016} > cmds016 &&
    ${query} -L ${grugs} -F simple -S CA -P high -t 016.R.out < cmds016 &&
    test_cmp 016.R.out ${exp_dir}/016.R.out
'


test_done

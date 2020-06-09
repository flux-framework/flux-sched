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

test_done

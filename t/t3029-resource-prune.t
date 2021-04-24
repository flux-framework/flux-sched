#!/bin/sh

test_description='Test Scheduling On Tiny Machine Configuration'

. $(dirname $0)/sharness.sh

cmd_dir="${SHARNESS_TEST_SRCDIR}/data/resource/commands/pruning"
exp_dir="${SHARNESS_TEST_SRCDIR}/data/resource/expected/basics"
grugs="${SHARNESS_TEST_SRCDIR}/data/resource/grugs/tiny.graphml"
query="../../resource/utilities/resource-query"

# tiny machine has a total of 100 resource vertices:
#     cluster[1]->rack[1]->node[2]->socket[2]->core[18],memory[4],gpu[1]
#
# jobspec is node[1]->socket[1]->slot[1]->core[18]
#
# Thus, the baseline preorder visit count (e.g., machine with no job allocated)
# should be 100.
# At the socket level, non-core resource vertices underneath it will not incur
# postorder visits so the baseline postorder visit count should be 80.
#
# As each job gets allocated, a socket's subtree resources will
# be allocated and won't lead to postorder visits. Plus, because of
# the default "ALL:core" pruning filter, preorder visit won't occur
# for those subtree resources under each socket either.
#
# There are further pruning at even higher level resources
# like compute node vertices.
# But in general, the preorder visit count should be reduced
# by O(23) starting from 100 each time. The postorder visit count
# starts from 80 and then should be reduced by O(18) each time.
#
cmds01="${cmd_dir}/cmds01.in"
test_expect_success 'prune: default core-level pruning works' '
    cat <<-EOF >cmp01 &&
	INFO: PREORDER VISIT COUNT=100
	INFO: POSTORDER VISIT COUNT=80
	INFO: PREORDER VISIT COUNT=77
	INFO: POSTORDER VISIT COUNT=61
	INFO: PREORDER VISIT COUNT=52
	INFO: POSTORDER VISIT COUNT=41
	INFO: PREORDER VISIT COUNT=29
	INFO: POSTORDER VISIT COUNT=22
	EOF
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds01} > cmds01 &&
    ${query} -L ${grugs} -S CA -P high -t 001.R.out -e < cmds01 > out01 &&
    cat out01 | grep ORDER > visit_count.out01 &&
    test_cmp cmp01 visit_count.out01
'

# jobspec is node[1]->socket[1]->slot[1]->core[1],gpu[1]
# See the comment with the first test for reasoning.
# The default core-level pruning won't be that helpful as you don't
# saturate core resources at all
#
cmds02="${cmd_dir}/cmds02.in"
test_expect_success 'prune: default core-level pruning works with gpu jobspec' '
    cat <<-EOF >cmp02 &&
	INFO: PREORDER VISIT COUNT=100
	INFO: POSTORDER VISIT COUNT=84
	INFO: PREORDER VISIT COUNT=100
	INFO: POSTORDER VISIT COUNT=81
	INFO: PREORDER VISIT COUNT=100
	INFO: POSTORDER VISIT COUNT=77
	INFO: PREORDER VISIT COUNT=100
	INFO: POSTORDER VISIT COUNT=74
	EOF
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds02} > cmds02 &&
    ${query} -L ${grugs} -S CA -P high -t 002.R.out -e < cmds02 > out02 &&
    cat out02 | grep ORDER > visit_count.out02 &&
    test_cmp cmp02 visit_count.out02
'

# jobspec is node[1]->socket[1]->slot[1]->core[1],gpu[1]
# See the comment with the first test for reasoning.
# Adding an additional pruning filter with GPU significantly
# helps in this case.
cmds03="${cmd_dir}/cmds02.in"
test_expect_success 'prune: using additional for faster gpu-level pruning' '
    cat <<-EOF >cmp03 &&
	INFO: PREORDER VISIT COUNT=100
	INFO: POSTORDER VISIT COUNT=84
	INFO: PREORDER VISIT COUNT=77
	INFO: POSTORDER VISIT COUNT=64
	INFO: PREORDER VISIT COUNT=52
	INFO: POSTORDER VISIT COUNT=43
	INFO: PREORDER VISIT COUNT=29
	INFO: POSTORDER VISIT COUNT=23
	EOF
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds03} > cmds03 &&
    ${query} -L ${grugs} -S CA -P high -p ALL:gpu -t 003.R.out -e < cmds03 > out03 &&
    cat out03 | grep ORDER > visit_count.out03 &&
    test_cmp cmp03 visit_count.out03
'

# jobspec is slot[1]->core[18]
# See the comment with the first test for reasoning.
# Notice the minor difference in the 3rd preorder
# visit count with the one above. Pruning could not be done at
# the node level because this jobspec doesn't have the aggregate
# core count requirement info at the node level.
cmds04="${cmd_dir}/cmds03.in"
test_expect_success 'prune: default core-level pruning works on w/ headless spec' '
    cat <<-EOF >cmp04 &&
	INFO: PREORDER VISIT COUNT=100
	INFO: POSTORDER VISIT COUNT=80
	INFO: PREORDER VISIT COUNT=77
	INFO: POSTORDER VISIT COUNT=61
	INFO: PREORDER VISIT COUNT=54
	INFO: POSTORDER VISIT COUNT=42
	INFO: PREORDER VISIT COUNT=29
	INFO: POSTORDER VISIT COUNT=22
	EOF
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds04} > cmds04 &&
    ${query} -L ${grugs} -S CA -P high -t 004.R.out -e < cmds04 > out04 &&
    cat out04 | grep ORDER > visit_count.out04 &&
    test_cmp cmp04 visit_count.out04
'

#
# First match only visits min number of vertices
cmds05="${cmd_dir}/cmds01.in"
test_expect_success 'prune: default core-level pruning works with first match' '
    cat <<-EOF >cmp05 &&
	INFO: PREORDER VISIT COUNT=27
	INFO: POSTORDER VISIT COUNT=22
	INFO: PREORDER VISIT COUNT=27
	INFO: POSTORDER VISIT COUNT=22
	INFO: PREORDER VISIT COUNT=27
	INFO: POSTORDER VISIT COUNT=22
	INFO: PREORDER VISIT COUNT=27
	INFO: POSTORDER VISIT COUNT=22
	EOF
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds05} > cmds05 &&
    ${query} -L ${grugs} -S CA -P first -t 005.R.out -e < cmds05 > out05 &&
    cat out05 | grep ORDER > visit_count.out05 &&
    test_cmp cmp05 visit_count.out05
'

#
# First match only visits min number of vertices
# jobspec is node[1]->socket[1]->slot[1]->core[1],gpu[1]
# gpu pruning filter helps reducing visit count further.
cmds06="${cmd_dir}/cmds02.in"
test_expect_success 'prune: use gpu-level pruning' '
    cat <<-EOF >cmp06 &&
	INFO: PREORDER VISIT COUNT=10
	INFO: POSTORDER VISIT COUNT=6
	INFO: PREORDER VISIT COUNT=10
	INFO: POSTORDER VISIT COUNT=6
	INFO: PREORDER VISIT COUNT=10
	INFO: POSTORDER VISIT COUNT=6
	INFO: PREORDER VISIT COUNT=10
	INFO: POSTORDER VISIT COUNT=6
	EOF
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds06} > cmds06 &&
    ${query} -L ${grugs} -S CA -P first -p ALL:gpu -t 006.R.out -e < cmds06 > out06 &&
    cat out06 | grep ORDER > visit_count.out06 &&
    test_cmp cmp06 visit_count.out06
'

test_done


#!/bin/sh
#set -x

test_description='Test functionality of variation aware scheduler in
flux ion-resource match service.
'

ORIG_HOME=${HOME}

. `dirname $0`/sharness.sh

#
# sharness modifies $HOME environment variable, but this interferes
# with python's package search path, in particular its user site package.
#
HOME=${ORIG_HOME}

grug="${SHARNESS_TEST_SRCDIR}/data/resource/grugs/uniform-storage.graphml"
cmd_dir="${SHARNESS_TEST_SRCDIR}/data/resource/commands/uniform_storage/"
exp_dir="${SHARNESS_TEST_SRCDIR}/data/resource/expected/uniform_storage/"
query="../../resource/utilities/resource-query"
#
# test_under_flux is under sharness.d/
#
test_under_flux 1

#
# print only with --debug
#
test_debug '
	echo ${grug}
'

cmds001="${cmd_dir}/mtl1unit-spread.in"
test_expect_success 'Uniform storage policy works with resource-match.' '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds001} > cmds001 &&
    ${query} -F pretty_simple -d -L ${grug} -S CA -P uniform-storage -t 001.R.out < cmds001 &&
    test_cmp ${exp_dir}/001.R.out ./001.R.out
'

test_done

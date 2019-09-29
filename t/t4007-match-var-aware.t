#!/bin/sh
#set -x

test_description='Test functionality of variation aware scheduler in
flux resource match service. 
'

ORIG_HOME=${HOME}

. `dirname $0`/sharness.sh

#
# sharness modifies $HOME environment variable, but this interferes
# with python's package search path, in particular its user site package.
#
HOME=${ORIG_HOME}

grug="${SHARNESS_TEST_SRCDIR}/data/resource/grugs/small.graphml"
j1N="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/var_aware/job_1N.yaml"
j2N="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/var_aware/job_2N.yaml"
j4N="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/var_aware/job_4N.yaml"
#
# test_under_flux is under sharness.d/
#
test_under_flux 1

#
# print only with --debug
#
test_debug '
	echo ${grug} &&
	echo ${j1N} && 
	echo ${j2N} &&
	echo ${j3N}
'

test_expect_success 'Loading variation policy on a small config works' '
	flux module load resource load-file=${grug} load-format=grug \
prune-filters=ALL:core subsystems=containment policy=variation
'

test_expect_success 'Variation policy works with resource-match.' '
	flux resource set-property /small0/rack0/node0 perf_class=3 &&
	flux resource set-property /small0/rack0/node1 perf_class=1 &&
	flux resource set-property /small0/rack0/node2 perf_class=2 &&
	flux resource set-property /small0/rack0/node3 perf_class=1 &&
	flux resource set-property /small0/rack0/node4 perf_class=2 &&
	flux resource set-property /small0/rack0/node5 perf_class=2 &&
	flux resource set-property /small0/rack0/node6 perf_class=3 &&
	flux resource set-property /small0/rack0/node7 perf_class=2 &&
	flux resource match allocate ${j2N} &&
	flux resource match allocate ${j1N} &&
	flux resource match allocate ${j4N} &&
	flux resource match allocate ${j1N} &&
	test_expect_code 16 flux resource match allocate ${j1N} &&
	flux resource match allocate_orelse_reserve ${j2N} &&
	flux resource match allocate_orelse_reserve ${j1N} &&
	flux resource match allocate_orelse_reserve ${j4N} &&
	flux resource match allocate_orelse_reserve ${j1N}
'

test_expect_success 'removing resource works' '
	flux module remove resource
'

test_done

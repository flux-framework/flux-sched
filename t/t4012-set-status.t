#!/bin/sh
#set -x

test_description='Test the basic functionality of properties (get/set) within resource
'

. `dirname $0`/sharness.sh

grug="${SHARNESS_TEST_SRCDIR}/data/resource/grugs/tiny.graphml"
jobspec="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/basics/test008.yaml"

#
# test_under_flux is under sharness.d/
#
export FLUX_SCHED_MODULE=none
test_under_flux 1

#
# print only with --debug
#

test_debug '
	echo ${grug}
'

test_expect_success 'loading resource module with a tiny machine config works' '
	load_resource \
load-file=${grug} load-format=grug \
prune-filters=ALL:core subsystems=containment policy=high &&
	load_feasibility load-file=${grug} load-format=grug subsystems=containment
'

test_expect_success 'set-status basic test works' '
	flux ion-resource find status=down | grep null &&
	flux ion-resource set-status /tiny0/rack0/node0 down &&
	flux ion-resource find status=down | grep node0 &&
	flux ion-resource set-status /tiny0/rack0/node0 up &&
	flux ion-resource find status=down | grep null
'

test_expect_success 'bad resource path produces an error' '
	test_must_fail flux ion-resource set-status /foobar/not/a/vertex down
'

test_expect_success 'bad status produces an error' '
	test_must_fail flux ion-resource set-status /tiny0/rack0/node0 foobar
'

test_expect_success 'set-status not-so-basic test works' '
	flux ion-resource find status=down | grep null &&
	flux ion-resource set-status /tiny0/rack0/node0 down &&
	flux ion-resource find status=down | grep node0 &&
	flux ion-resource set-status /tiny0/rack0/node1 down &&
	flux ion-resource find status=down | grep "node\[0-1\]" &&
	flux ion-resource set-status /tiny0/rack0/node0 up &&
	flux ion-resource find status=down | grep node1 &&
	flux ion-resource set-status /tiny0/rack0/node1 up &&
	flux ion-resource find status=down | grep null
'

test_expect_success 'jobs fail when all nodes are marked down' '
	flux ion-resource set-status /tiny0/rack0/node0 down &&
	flux ion-resource set-status /tiny0/rack0/node1 down &&
	flux ion-resource find status=up | grep null &&	
	flux ion-resource match satisfiability $jobspec &&
	test_must_fail flux ion-resource match allocate $jobspec &&
	flux ion-resource set-status /tiny0/rack0/node0 up &&
	flux ion-resource set-status /tiny0/rack0/node1 up &&
	flux ion-resource find status=down | grep null
'

test_expect_success 'jobs fail when all racks are marked down' '
	flux ion-resource find status=down | grep null &&
	flux ion-resource set-status /tiny0/rack0 down &&
	flux ion-resource find status=up | grep null &&	
	flux ion-resource match satisfiability $jobspec &&
	test_must_fail flux ion-resource match allocate $jobspec &&
	flux ion-resource set-status /tiny0/rack0 up &&
	flux ion-resource find status=down | grep null
'

test_expect_success 'reloading resource and loading qmanager works' '
	remove_resource &&
	flux module reload resource &&
	reload_resource &&
	reload_qmanager_sync &&
	reload_feasibility
'

test_expect_success 'set-status RPC causes jobs to be reconsidered' '
	up=$(flux ion-resource find -q --format jgf status=up \
		| jq -r ".graph.nodes[].metadata | select(.type == \"node\") \
		| .paths.containment") &&
	for path in $up; do flux ion-resource set-status $path down ; done &&
	flux ion-resource find status=up | grep null &&
	jobid=$(flux submit -n1 true) &&
	test_must_fail flux job wait-event -vt2 $jobid alloc &&
	for path in $up; do flux ion-resource set-status $path up ; done &&
	flux job wait-event -vt1 $jobid alloc &&
	flux job wait-event -vt2 -H $jobid clean
'

test_expect_success 'removing qmanager and resource works' '
	remove_qmanager &&
    remove_feasibility &&
	remove_resource
'

test_done

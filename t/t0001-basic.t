#!/bin/sh
#set -x

test_description='Test basic schedsvr usage in flux session

Ensure the very basics of flux schedsvr work.
'
#
# source sharness from the directore where this test
# file resides
#
. $(dirname $0)/sharness.sh

#
# test_under_flux is under sharness.d/
#
SIZE=2
test_under_flux ${SIZE}


test_expect_success 'sched: module load works' '
	flux module load sched
'

test_expect_success 'sched: module remove works' '
	flux module remove sched
'

test_expect_success 'sched: flux-module load works after a successful unload' '
	flux module load sched &&
	flux module remove sched
'

# comment this one out for now
test_expect_success 'sched: module load should fail' '
	test_expect_code 1 flux module load sched rdl-conf=foo
'

test_expect_success 'sched: module load works after a load failure' '
	flux module load sched
'

test_expect_success 'sched: module list works' '
	flux module list
'

test_expect_success 'get_instance_size works' '
	test "$(get_instance_size)" = "$SIZE" &&
	test "$(get_instance_size)" = "$(get_instance_size)"
'
test_done

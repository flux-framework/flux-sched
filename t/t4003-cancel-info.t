#!/bin/sh
#set -x

test_description='Test the basic functionality of cancel and info within resource

Ensure that the cancel and info handlers within the resource module works
'

. `dirname $0`/sharness.sh

grug="${SHARNESS_TEST_SRCDIR}/data/resource/grugs/tiny.graphml"
jobspec="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/basics/test001.yaml"

#
# test_under_flux is under sharness.d/
#
test_under_flux 1

#
# print only with --debug
#
test_debug '
    echo ${grug} &&
    echo ${jobspec}
'

test_expect_success 'loading resource module with a tiny machine config works' '
    load_resource \
load-file=${grug} prune-filters=ALL:core \
load-format=grug subsystems=containment policy=high
'

test_expect_success 'resource-cancel works' '
    flux ion-resource match allocate ${jobspec} &&
    flux ion-resource cancel 0 &&
    flux ion-resource match allocate ${jobspec} &&
    flux ion-resource cancel 0 &&
    flux ion-resource match allocate ${jobspec} &&
    flux ion-resource cancel 0 &&
    flux ion-resource match allocate ${jobspec} &&
    flux ion-resource cancel 0
'

test_expect_success 'resource-info will not report for canceled jobs' '
    test_must_fail flux ion-resource info 0
'

test_expect_success 'allocate works with 1-node/1-socket after cancels' '
    flux ion-resource match allocate ${jobspec} &&
    flux ion-resource match allocate ${jobspec} &&
    flux ion-resource match allocate ${jobspec} &&
    flux ion-resource match allocate ${jobspec}
'

test_expect_success 'resource-info on allocated jobs works' '
    flux ion-resource info 0 > info.0 &&
    flux ion-resource info 1 > info.1 &&
    flux ion-resource info 2 > info.2 &&
    flux ion-resource info 3 > info.3 &&
    grep ALLOCATED info.0 &&
    grep ALLOCATED info.1 &&
    grep ALLOCATED info.2 &&
    grep ALLOCATED info.3
'

test_expect_success 'cancel on nonexistent jobid is handled gracefully' '
    test_expect_code 3 flux ion-resource cancel 100000
'

test_expect_success 'removing resource works' '
    remove_resource
'

test_done

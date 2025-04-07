#!/bin/sh
#set -x

test_description='Test the basic functionality of resource-match-allocate

Ensure that the match (allocate) handler within the resource module works
'

. `dirname $0`/sharness.sh

grug="${SHARNESS_TEST_SRCDIR}/data/resource/grugs/tiny.graphml"
jobspec="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/basics/test001.yaml"
malform="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/basics/bad.yaml"
duration_too_large="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/duration/test001.yaml"
duration_negative="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/duration/test002.yaml"

#
# test_under_flux is under sharness.d/
#
test_under_flux 1

#
# print only with --debug
#
test_debug '
    echo ${grug} &&
    echo ${jobspec} &&
    echo ${malform}
'

test_expect_success 'loading resource module with a tiny machine config works' '
    load_resource \
load-file=${grug} prune-filters=ALL:core \
load-format=grug subsystems=containment policy=high
'

test_expect_success 'match-allocate works with a 1-node, 1-socket jobspec' '
    flux ion-resource match allocate ${jobspec} &&
    flux ion-resource match allocate ${jobspec} &&
    flux ion-resource match allocate ${jobspec} &&
    flux ion-resource match allocate ${jobspec}
'

test_expect_success 'match-allocate fails when all resources are allocated' '
    test_expect_code 16 flux ion-resource match allocate ${jobspec} &&
    test_expect_code 16 flux ion-resource match allocate ${jobspec} &&
    test_expect_code 16 flux ion-resource match allocate ${jobspec} &&
    test_expect_code 16 flux ion-resource match allocate ${jobspec}
'

test_expect_success 'detecting of a non-existent jobspec file works' '
    test_expect_code 3 flux ion-resource match allocate foo
'

test_expect_success 'handling of a malformed jobspec works' '
    test_expect_code 2 flux ion-resource match allocate ${malform}
'

test_expect_success 'handling of an invalid resource type works' '
    test_expect_code 1 flux ion-resource match allocate \
        "${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/basics/bad_res_type.yaml"
'

test_expect_success 'invalid duration is caught' '
    test_must_fail flux ion-resource match allocate ${duration_too_large} &&
    test_must_fail flux ion-resource match allocate ${duration_negative}
'


test_expect_success 'removing resource works' '
    remove_resource
'

test_done

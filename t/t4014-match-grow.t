#!/bin/sh
#set -x

test_description='Test the basic functionality of resource-match-grow-allocation

Ensure that the match (allocate) handler within the resource module works
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

test_expect_success 'match-grow works with a 1-node, 1-socket jobspec' '
    output=$( flux ion-resource match allocate ${jobspec} ) &&
    jobid=$(echo ${output} | awk "{print \$6}") &&
    flux ion-resource match grow_allocation ${jobspec} ${jobid}
'

test_expect_failure 'match-grow fails with nonexistent jobid' '
    flux ion-resource match grow_allocation ${jobspec} 10000000
'

test_expect_success 'removing resource works' '
    remove_resource
'

test_done

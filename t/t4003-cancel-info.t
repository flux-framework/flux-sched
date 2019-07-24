#!/bin/sh
#set -x

test_description='Test the basic functionality of cancel and info within resource

Ensure that the cancel and info handlers within the resource module works
'

ORIG_HOME=${HOME}

. `dirname $0`/sharness.sh

#
# sharness modifies $HOME environment variable, but this interferes
# with python's package search path, in particular its user site package.
#
HOME=${ORIG_HOME}

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
    flux module load resource grug-conf=${grug} prune-filters=ALL:core \
subsystems=containment policy=high
'

test_expect_success 'resource-cancel works' '
    flux resource match allocate ${jobspec} &&
    flux resource cancel 0 &&
    flux resource match allocate ${jobspec} &&
    flux resource cancel 0 &&
    flux resource match allocate ${jobspec} &&
    flux resource cancel 0 &&
    flux resource match allocate ${jobspec} &&
    flux resource cancel 0
'

test_expect_success 'resource-info will not report for cancelled jobs' '
    test_must_fail flux resource info 0
'

test_expect_success 'allocate works with 1-node/1-socket after cancels' '
    flux resource match allocate ${jobspec} &&
    flux resource match allocate ${jobspec} &&
    flux resource match allocate ${jobspec} &&
    flux resource match allocate ${jobspec}
'

test_expect_success 'resource-info on allocated jobs works' '
    flux resource info 0 > info.0 &&
    flux resource info 1 > info.1 &&
    flux resource info 2 > info.2 &&
    flux resource info 3 > info.3 &&
    grep ALLOCATED info.0 &&
    grep ALLOCATED info.1 &&
    grep ALLOCATED info.2 &&
    grep ALLOCATED info.3
'

test_expect_success 'cancel on nonexistent jobid is handled gracefully' '
    test_expect_code 1 flux resource cancel 100000
'

test_expect_success 'removing resource works' '
    flux module remove resource
'

test_done

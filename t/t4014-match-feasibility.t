#!/bin/sh
#set -x

# Adapted from t4005

test_description='Test the basic functionality of match satisfiability
'

. `dirname $0`/sharness.sh

grug="${SHARNESS_TEST_SRCDIR}/data/resource/grugs/tiny.graphml"
jobspec1="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/basics/test001.yaml"
jobspec2="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/satisfiability/test001.yaml"

test_under_flux 1

test_debug '
    echo ${grug} &&
    echo ${jobspec1} &&
    echo ${jobspec2}
'

test_expect_success 'loading feasibility module over sched-simple fails' '
    load_feasibility
    flux dmesg -c | grep -q "File exists"
'

test_expect_success 'removing sched-simple works' '
    flux module remove sched-simple
'

test_expect_success 'loading feasibility module before resource fails' '
    load_feasibility
    flux dmesg -c | grep -q "Function not implemented"
'

test_expect_success 'loading resource module with a tiny machine config works' '
    load_resource load-file=${grug} load-format=grug \
prune-filters=ALL:core subsystems=containment policy=high
    test -z "$(flux dmesg -c | grep -q err)"
'

test_expect_success 'loading feasibility module with a tiny machine config works' '
    load_feasibility load-file=${grug} load-format=grug \
prune-filters=ALL:core subsystems=containment policy=high
    test -z "$(flux dmesg -c | grep -q err)"
'

test_expect_success 'satisfiability works with a 1-node, 1-socket jobspec' '
    flux ion-resource match allocate_with_satisfiability ${jobspec1} &&
    flux ion-resource match allocate_with_satisfiability ${jobspec1} &&
    flux ion-resource match allocate_with_satisfiability ${jobspec1} &&
    flux ion-resource match allocate_with_satisfiability ${jobspec1}
'

test_expect_success 'satisfiability returns EBUSY when no available resources' '
    test_expect_code 16 flux ion-resource \
match allocate_with_satisfiability ${jobspec1} &&
    test_expect_code 16 flux ion-resource \
match allocate_with_satisfiability ${jobspec1} &&
    test_expect_code 16 flux ion-resource \
match allocate_with_satisfiability ${jobspec1} &&
    test_expect_code 16 flux ion-resource \
match allocate_with_satisfiability ${jobspec1}
'

test_expect_success 'jobspec is still satisfiable even when no available resources' '
    flux ion-resource match satisfiability ${jobspec1} &&
    flux ion-resource match satisfiability ${jobspec1} &&
    flux ion-resource match satisfiability ${jobspec1} &&
    flux ion-resource match satisfiability ${jobspec1}
'

test_expect_success 'removing load-file feasibility module works' '
    remove_feasibility
    test -z "$(flux dmesg -c | grep -q err)"
'

# A resource module that has a load-file will not relay those resources to
# the feasibility module. The feasibility module needs the same load-file.
test_expect_success 'loading feasibility module from load-file resource module fails' '
    load_feasibility &&
    flux dmesg -c | grep -q err &&
    ! flux module list | grep -q sched-fluxion-feasib
'

test_expect_success 'removing resource module works' '
    remove_resource
'

test_expect_success 'loading non-load-file resource module works' '
    load_resource
    test -z "$(flux dmesg -c | grep -q err)"
'

test_expect_success 'loading feasibility from non-load-file resource module works' '
    load_feasibility
    test -z "$(flux dmesg -c | grep -q err)"
'

test_expect_success 'removing resource works and removes feasibility' '
    remove_resource &&
    flux dmesg -c | grep -q "exiting due to sched-fluxion-resource.notify failure"
'

test_done

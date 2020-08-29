#!/bin/sh

test_description='Test Synchronization Requirements for Fluxion modules'

. `dirname $0`/sharness.sh

test_under_flux 1

# sched-simple acquires "exclusive" access to resource.
test_expect_success 'sync: fluxion-resource cannot load w/ sched-simple' '
    flux module info sched-simple &&
    load_resource policy=low load-allowlist=node,socket,core,gpu &&
    test_must_fail flux module stats sched-fluxion-resource
'

test_expect_success 'sync: qmanager does not load w/o fluxion-resource' '
    flux module remove sched-simple &&
    load_qmanager &&
    test_must_fail flux module stats sched-fluxion-qmanager
'

# sched-simple releases the exclusive access to resource.
test_expect_success 'sync: fluxion-resource loads w/ sched-simple unloaded' '
    load_resource load-allowlist=node,socket,core,gpu
'

test_expect_success 'sync: qmanager loads w/ sched-fluxion-resource loaded' '
    load_qmanager &&
    flux module stats sched-fluxion-qmanager &&
    flux module stats sched-fluxion-resource
'

test_expect_success 'sync: unloading fluxion-resource removes qmanager' '
    remove_resource &&
    test_must_fail remove_qmanager
'

test_expect_success 'sync: resource is still intact and sched-simple loads' '
    flux module info resource &&
    flux module load sched-simple
'

test_expect_success 'sync: qmanager will not prematurely proceed' '
    flux module remove sched-simple &&
    load_resource load-allowlist=node,socket,core,gpu &&
    flux dmesg -C &&
    load_qmanager &&
    flux module stats sched-fluxion-qmanager &&
    flux module stats sched-fluxion-resource &&
    flux dmesg > out &&
    sync1=$(grep -n "handshaking with sched-fluxion-resource" out) &&
    sync2=$(grep -n "handshaking with job-manager" out) &&
    sync1=$(echo ${sync1} | awk -F: "{print \$1}") &&
    sync2=$(echo ${sync2} | awk -F: "{print \$1}") &&
    test ${sync1} -lt ${sync2}
'

test_expect_success 'removing the core resource will unload fluxion modules' '
    flux module remove resource &&
    test_must_fail remove_resource &&
    test_must_fail remove_qmanager
'

# Reload the core resource and scheduler so that rc3 won't hang waiting for
# queue to become idle after jobs are canceled.
test_expect_success 'load resource and sched-simple module' '
    flux module load resource &&
    flux module load sched-simple
'

test_done


#!/bin/sh

test_description='Test Synchronization Requirements for Fluxion modules'

. `dirname $0`/sharness.sh

export FLUX_SCHED_MODULE=none
test_under_flux 1 full -Sbroker.module-nopanic=1

# sched-simple acquires "exclusive" access to resource.

test_expect_success 'load sched-simple and wait for resources to be acquired' '
    flux module load sched-simple &&
    flux resource list
'

test_expect_success 'sync: fluxion-resource cannot load w/ sched-simple' '
    load_resource policy=low &&
    test_must_fail flux module stats sched-fluxion-resource
'

test_expect_success 'sync: qmanager does not load w/o fluxion-resource' '
    test_must_fail load_qmanager
'

test_expect_success 'remove sched-simple' '
    flux module remove sched-simple
'

# sched-simple releases the exclusive access to resource.
test_expect_success 'sync: fluxion-resource loads w/ sched-simple unloaded' '
    load_resource policy=high
'

test_expect_success 'sync: qmanager loads w/ sched-fluxion-resource loaded' '
    load_qmanager_sync
'

test_expect_success 'sync: unloading fluxion-resource removes qmanager' '
    remove_resource &&
    test_must_fail remove_qmanager
'

test_expect_success 'sync: load sched-simple' '
    flux module load sched-simple
'

test_expect_success 'sync: remove sched-simple' '
    flux module remove sched-simple
'

test_expect_success 'sync: qmanager will not prematurely proceed' '
    load_resource policy=high &&
    flux dmesg -C &&
    load_qmanager_sync &&
    flux dmesg > out &&
    sync1=$(grep -n "handshaking with sched-fluxion-resource" out) &&
    sync2=$(grep -n "handshaking with job-manager" out) &&
    sync1=$(echo ${sync1} | awk -F: "{print \$1}") &&
    sync2=$(echo ${sync2} | awk -F: "{print \$1}") &&
    test ${sync1} -lt ${sync2}
'

test_expect_success 'unload core resource module' '
    flux module remove resource
'

test_expect_success 'that killed both qmanager and fluxion-resource' '
    test_must_fail remove_qmanager &&
    test_must_fail remove_resource
'

test_expect_success 'load core resource module' '
    flux module load resource
'

test_done


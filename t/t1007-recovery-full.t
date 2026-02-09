#!/bin/sh

test_description='Test the state recovery of qmanager and resource modules A:

Full recovery case --
1. The format of running jobs is rv1 and restart both resource and qmanager
2. Restart only qmanager, not the resource module.
'

. `dirname $0`/sharness.sh

hwloc_basepath=`readlink -e ${SHARNESS_TEST_SRCDIR}/data/hwloc-data`
# 1 brokers, each (exclusively) have: 1 node, 2 sockets, 16 cores (8 per socket)
excl_1N1B="${hwloc_basepath}/001N/exclusive/01-brokers"

export FLUX_SCHED_MODULE=none
test_under_flux 1 full -Slog-stderr-level=6


test_expect_success 'recovery: generate a test jobspec' '
    flux run --dry-run -N 1 -n 4 -t 1h sleep 3600 > basic.json
'

test_expect_success 'load test resources' '
    load_test_resources ${excl_1N1B}
'

test_expect_success 'recovery: loading flux-sched modules works (rv1)' '
    load_resource match-format=rv1 policy=high &&
    load_qmanager
'

# jobid1 - 4 will be scheduled; jobid 5 - 7 pending
test_expect_success 'recovery: submit to occupy resources fully (rv1)' '
    jobid1=$(flux job submit basic.json) &&
    jobid2=$(flux job submit basic.json) &&
    jobid3=$(flux job submit basic.json) &&
    jobid4=$(flux job submit basic.json) &&
    jobid5=$(flux job submit basic.json) &&
    jobid6=$(flux job submit basic.json) &&
    jobid7=$(flux job submit basic.json) &&
    flux job wait-event -t 10 ${jobid4} start &&
    flux job wait-event -t 10 ${jobid7} submit
'

test_expect_success 'recovery: cancel one running job without fluxion' '
    remove_qmanager &&
    remove_resource &&
    flux cancel ${jobid1} &&
    flux job wait-event -t 10 ${jobid1} release
'

test_expect_success 'recovery: works when both modules restart (rv1)' '
    load_resource match-format=rv1 policy=high &&
    load_qmanager_sync &&
    test_must_fail flux ion-resource info ${jobid1} &&
    flux ion-resource info ${jobid2} | grep "ALLOCATED" &&
    flux ion-resource info ${jobid3} | grep "ALLOCATED" &&
    flux ion-resource info ${jobid4} | grep "ALLOCATED" &&
    flux job wait-event -t 10 ${jobid5} start &&
    test_expect_code 3 flux ion-resource info ${jobid6}
'

test_expect_success 'recovery: a cancel leads to a job schedule (rv1)' '
    flux cancel ${jobid2} &&
    flux job wait-event -t 10 ${jobid6} start
'

test_expect_success 'recovery: works when only qmanager restarts (rv1)' '
    reload_qmanager &&
    test_must_fail flux ion-resource info ${jobid2} &&
    flux ion-resource info ${jobid3} | grep "ALLOCATED" &&
    flux ion-resource info ${jobid4} | grep "ALLOCATED" &&
    flux ion-resource info ${jobid5} | grep "ALLOCATED" &&
    flux ion-resource info ${jobid6} | grep "ALLOCATED" &&
    test_must_fail flux job wait-event -t 0.5 ${jobid7} start &&
    test_expect_code 3 flux ion-resource info ${jobid7}
'

test_expect_success 'recovery: a cancel leads to a job schedule (rv1)' '
    flux cancel ${jobid3} &&
    flux job wait-event -t 10 ${jobid3} clean &&
    flux job wait-event -t 10 ${jobid7} start
'

test_expect_success 'recovery: both modules restart (rv1->rv1_nosched)' '
    remove_qmanager &&
    reload_resource match-format=rv1_nosched policy=high &&
    load_qmanager_sync &&
    flux ion-resource info ${jobid4} | grep "ALLOCATED" &&
    flux ion-resource info ${jobid5} | grep "ALLOCATED" &&
    flux ion-resource info ${jobid6} | grep "ALLOCATED" &&
    flux ion-resource info ${jobid7} | grep "ALLOCATED"
'

test_expect_success 'recovery: only qmanager restarts (rv1->rv1_nosched)' '
    reload_qmanager_sync &&
    flux ion-resource info ${jobid4} | grep "ALLOCATED" &&
    flux ion-resource info ${jobid5} | grep "ALLOCATED" &&
    flux ion-resource info ${jobid6} | grep "ALLOCATED" &&
    flux ion-resource info ${jobid7} | grep "ALLOCATED"
'

test_expect_success 'recovery: cancel all jobs (rv1_nosched)' '
    flux cancel ${jobid4} &&
    flux cancel ${jobid5} &&
    flux cancel ${jobid6} &&
    flux cancel ${jobid7} &&
    flux job wait-event -t 10 ${jobid4} clean &&
    flux job wait-event -t 10 ${jobid5} clean &&
    flux job wait-event -t 10 ${jobid6} clean &&
    flux job wait-event -t 10 ${jobid7} clean
'

test_expect_success 'recovery: restart w/ no running jobs (rv1_nosched)' '
    remove_qmanager &&
    reload_resource match-format=rv1_nosched policy=high &&
    load_qmanager_sync
'

test_expect_success 'recovery: submit to occupy resources fully (rv1_nosched)' '
    jobid1=$(flux job submit basic.json) &&
    jobid2=$(flux job submit basic.json) &&
    jobid3=$(flux job submit basic.json) &&
    jobid4=$(flux job submit basic.json) &&
    jobid5=$(flux job submit basic.json) &&
    flux job wait-event -t 10 ${jobid4} start &&
    flux job wait-event -t 10 ${jobid5} submit
'

test_expect_success 'recovery: qmanager restarts (rv1_nosched->rv1_nosched)' '
    reload_qmanager_sync &&
    flux ion-resource info ${jobid1} | grep "ALLOCATED" &&
    flux ion-resource info ${jobid2} | grep "ALLOCATED" &&
    flux ion-resource info ${jobid3} | grep "ALLOCATED" &&
    flux ion-resource info ${jobid4} | grep "ALLOCATED" &&
    test_must_fail flux job wait-event -t 0.5 ${jobid5} start &&
    test_expect_code 3 flux ion-resource info ${jobid5}
'

test_expect_success 'recovery: a cancel leads to a job schedule (rv1_nosched)' '
    flux cancel ${jobid1} &&
    flux job wait-event -t 60 ${jobid5} start
'

# flux-framework/flux-sched#991
test_expect_success 'recovery: both modules restart (rv1_nosched->rv1_nosched)' '
    reload_resource match-format=rv1_nosched policy=high &&
    reload_qmanager &&
    flux module stats sched-fluxion-qmanager &&
    flux module stats sched-fluxion-resource &&
    flux ion-resource info ${jobid2} | grep "ALLOCATED" &&
    flux ion-resource info ${jobid3} | grep "ALLOCATED" &&
    flux ion-resource info ${jobid4} | grep "ALLOCATED" &&
    flux ion-resource info ${jobid5} | grep "ALLOCATED"
'

test_expect_success 'removing resource and qmanager modules, if loaded' '
    flux module remove -f sched-fluxion-qmanager &&
    flux module remove -f sched-fluxion-resource
'

# sched-simple is needed while flux-framework/flux-sched#991 is not fixed
test_expect_success 'cleanup active jobs using sched-simple' '
    flux module load sched-simple &&
    cleanup_active_jobs &&
    flux module remove sched-simple
'

test_done

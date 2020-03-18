#!/bin/sh

test_description='Test the state recovery of qmanager and resource modules A:

Full recovery case --
1. The format of running jobs is rv1 and restart both resource and qmanager
2. Restart only qmanager, not the resource module.
'

. `dirname $0`/sharness.sh

hwloc_basepath=`readlink -e ${SHARNESS_TEST_SRCDIR}/data/hwloc-data`
# 4 brokers, each (exclusively) have: 1 node, 2 sockets, 16 cores (8 per socket)
excl_4N4B="${hwloc_basepath}/004N/exclusive/04-brokers"

test_under_flux 1

test_expect_success 'recovery: generate a test jobspec' '
    flux mini run --dry-run -N 1 -n 4 -t 1h sleep 3600 > basic.json
'

test_expect_success 'recovery: hwloc reload works' '
    flux hwloc reload ${excl_4N4B}
'

test_expect_success 'recovery: loading flux-sched modules works (rv1)' '
    flux module remove sched-simple &&
    flux module load resource load-whitelist=node,core,gpu \
match-format=rv1 &&
    flux module load qmanager
'

# jobid1 - 4 will be scheduled; jobid 5 - 6 pending
test_expect_success 'recovery: submit to occupy resources fully (rv1)' '
    jobid1=$(flux job submit basic.json) &&
    jobid2=$(flux job submit basic.json) &&
    jobid3=$(flux job submit basic.json) &&
    jobid4=$(flux job submit basic.json) &&
    jobid5=$(flux job submit basic.json) &&
    jobid6=$(flux job submit basic.json) &&
    flux job wait-event -t 2 ${jobid4} start &&
    flux job wait-event -t 2 ${jobid6} submit
'

test_expect_success 'recovery: works when both modules restart (rv1)' '
    flux module reload -f resource load-whitelist=node,core,gpu \
match-format=rv1 &&
    flux module reload -f qmanager &&
    flux resource info ${jobid1} | grep "ALLOCATED" &&
    flux resource info ${jobid2} | grep "ALLOCATED" &&
    flux resource info ${jobid3} | grep "ALLOCATED" &&
    flux resource info ${jobid4} | grep "ALLOCATED" &&
    test_must_fail flux job wait-event -t 0.5 ${jobid5} start &&
    test_expect_code 3 flux resource info ${jobid5}
'

test_expect_success 'recovery: a cancel leads to a job schedule (rv1)' '
    flux job cancel ${jobid1} &&
    flux job wait-event -t 2 ${jobid5} start
'

test_expect_success 'recovery: works when only qmanager restarts (rv1)' '
    flux module reload -f qmanager &&
    flux resource info ${jobid2} | grep "ALLOCATED" &&
    flux resource info ${jobid3} | grep "ALLOCATED" &&
    flux resource info ${jobid4} | grep "ALLOCATED" &&
    flux resource info ${jobid5} | grep "ALLOCATED" &&
    test_must_fail flux job wait-event -t 0.5 ${jobid6} start &&
    test_expect_code 3 flux resource info ${jobid6}
'

test_expect_success 'recovery: a cancel leads to a job schedule (rv1)' '
    flux job cancel ${jobid2} &&
    flux job wait-event -t 2 ${jobid6} start
'

test_expect_success 'recovery: both modules restart (rv1->rv1_nosched)' '
    flux module reload -f resource load-whitelist=node,core,gpu \
match-format=rv1_nosched &&
    flux module reload -f qmanager &&
    flux resource info ${jobid3} | grep "ALLOCATED" &&
    flux resource info ${jobid4} | grep "ALLOCATED" &&
    flux resource info ${jobid5} | grep "ALLOCATED" &&
    flux resource info ${jobid6} | grep "ALLOCATED"
'

test_expect_success 'recovery: only qmanager restarts (rv1->rv1_nosched)' '
    flux module reload -f qmanager &&
    flux resource info ${jobid3} | grep "ALLOCATED" &&
    flux resource info ${jobid4} | grep "ALLOCATED" &&
    flux resource info ${jobid5} | grep "ALLOCATED" &&
    flux resource info ${jobid6} | grep "ALLOCATED"
'

test_expect_success 'recovery: cancel all jobs (rv1_nosched)' '
    flux job cancel ${jobid3} &&
    flux job cancel ${jobid4} &&
    flux job cancel ${jobid5} &&
    flux job cancel ${jobid6} &&
    flux job wait-event -t 2 ${jobid6} release 
'

test_expect_success 'recovery: restart w/ no running jobs (rv1_nosched)' '
    flux module reload -f resource load-whitelist=node,core,gpu \
match-format=rv1_nosched &&
    flux module reload qmanager
'

test_expect_success 'recovery: submit to occupy resources fully (rv1_nosched)' '
    jobid1=$(flux job submit basic.json) &&
    jobid2=$(flux job submit basic.json) &&
    jobid3=$(flux job submit basic.json) &&
    jobid4=$(flux job submit basic.json) &&
    jobid5=$(flux job submit basic.json) &&
    flux job wait-event -t 2 ${jobid4} start &&
    flux job wait-event -t 2 ${jobid5} submit
'

test_expect_success 'recovery: qmanager restarts (rv1_nosched->rv1_nosched)' '
    flux module reload -f qmanager &&
    flux resource info ${jobid1} | grep "ALLOCATED" &&
    flux resource info ${jobid2} | grep "ALLOCATED" &&
    flux resource info ${jobid3} | grep "ALLOCATED" &&
    flux resource info ${jobid4} | grep "ALLOCATED" &&
    test_must_fail flux job wait-event -t 0.5 ${jobid5} start &&
    test_expect_code 3 flux resource info ${jobid5}
'

test_expect_success 'recovery: a cancel leads to a job schedule (rv1_nosched)' '
    flux job cancel ${jobid1} &&
    flux job wait-event -t 2 ${jobid5} start
'

test_expect_success 'recovery: cancel all jobs (rv1_nosched)' '
    flux job cancel ${jobid2} &&
    flux job cancel ${jobid3} &&
    flux job cancel ${jobid4} &&
    flux job cancel ${jobid5} &&
    flux job wait-event -t 2 ${jobid5} release 
'

test_expect_success 'removing resource and qmanager modules' '
    flux module remove qmanager &&
    flux module remove resource
'

# Reload the core scheduler so that rc3 won't hang waiting for
# queue to become idle after jobs are canceled.
test_expect_success 'load sched-simple module' '
    flux module load sched-simple
'

test_done

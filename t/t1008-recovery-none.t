#!/bin/sh

test_description='Test the state recovery of qmanager and resource modules:

No recovery case: 1. the format of running jobs is rv1_nosched, and both
resource and qmanager restart.  (i.e., qmanager without
resource-recovery-on-load=false)
'

. `dirname $0`/sharness.sh

hwloc_basepath=`readlink -e ${SHARNESS_TEST_SRCDIR}/data/hwloc-data`
# 1 brokers, each (exclusively) have: 1 node, 2 sockets, 16 cores (8 per socket)
excl_1N1B="${hwloc_basepath}/001N/exclusive/01-brokers"

export FLUX_SCHED_MODULE=none
test_under_flux 1

test_expect_success 'recovery: generate a test jobspec' '
    flux mini run --dry-run -N 1 -n 4 -t 1h sleep 3600 > basic.json
'

test_expect_success 'load test resources' '
    load_test_resources ${excl_1N1B}
'

test_expect_success 'recovery: loading flux-sched modules works (rv1_nosched)' '
    load_resource match-format=rv1_nosched &&
    load_qmanager
'

test_expect_success 'recovery: submit a job (rv1_nosched)' '
    jobid1=$(flux job submit basic.json) &&
    flux job wait-event -t 10 ${jobid1} start
'

test_expect_success 'recovery: qmanager w/o an option must fail (rv1_nosched)' '
    reload_resource match-format=rv1_nosched &&
    reload_qmanager &&
    test_must_fail flux module stats sched-fluxion-qmanager
'

test_expect_success 'removing resource and qmanager modules' '
    test_must_fail remove_qmanager &&
    remove_resource
'

# At this point qmanager cannot be reloaded because it will fail recovery,
# so load the simple scheduler so we can purge the queue
test_expect_success 'cleanup active jobs with sched-simple' '
    flux module load sched-simple &&
    cleanup_active_jobs &&
    flux module remove sched-simple
'

test_done

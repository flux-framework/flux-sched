#!/bin/sh

test_description='Test the state recovery of qmanager and resource modules:

No recovery case: 1. the format of running jobs is rv1_nosched, and both
resource and qmanager restart.  (i.e., qmanager without
resource-recovery-on-load=false)
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

test_expect_success 'recovery: loading flux-sched modules works (rv1_nosched)' '
    flux module remove sched-simple &&
    flux module load resource load-whitelist=node,core,gpu \
match-format=rv1_nosched &&
    flux module load qmanager
'

test_expect_success 'recovery: submit a job (rv1_nosched)' '
    jobid1=$(flux job submit basic.json) &&
    flux job wait-event -t 2 ${jobid1} start
'

test_expect_success 'recovery: qmanager w/o an option must fail (rv1_nosched)' '
    flux module reload -f resource load-whitelist=node,core,gpu \
match-format=rv1_nosched &&
    test_must_fail flux module reload -f qmanager
'

test_expect_success 'removing resource and qmanager modules' '
    flux module remove resource
'

# Reload the core scheduler so that rc3 won't hang waiting for
# queue to become idle after jobs are canceled.
test_expect_success 'load sched-simple module' '
    flux module load sched-simple
'

test_done

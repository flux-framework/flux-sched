#!/bin/sh

test_description='Fluxion takes into account priority and t_submit'

. `dirname $0`/sharness.sh

hwloc_basepath=`readlink -e ${SHARNESS_TEST_SRCDIR}/data/hwloc-data`
# 1 node, 2 sockets, 44 cores (22 per socket), 4 gpus (2 per socket)
excl_1N1B="${hwloc_basepath}/004N/exclusive/04-brokers-sierra2"

skip_all_unless_have jq

test_under_flux 1

test_expect_success 'priority: hwloc reload works' '
    flux hwloc reload ${excl_1N1B} &&
    flux module remove sched-simple &&
    flux module reload resource
'

test_expect_success 'priority: loading fluxion modules works' '
    load_resource load-allowlist=cluster,node,core,gpu &&
    load_qmanager
'

test_expect_success 'priority: a full-size job can be scheduled and run' '
    jobid1=$(flux mini submit -N 1 -n 1 -c 44 -g 4 -t 1h \
--priority 16 sleep 3600) &&
    flux job wait-event -t 10 ${jobid1} start
'

test_expect_success 'priority: 2 jobs with higher priority will not run' '
    jobid2=$(flux mini submit -N 1 -n 1 -c 44 -g 4 -t 1h \
--priority 17 sleep 3600) &&
    jobid3=$(flux mini submit -N 1 -n 1 -c 44 -g 4 -t 1h \
--priority 18 sleep 3600) &&
    test_must_fail flux job wait-event -t 1 ${jobid2} start
'

test_expect_success 'priority: canceling the first job starts the last job' '
    flux job cancel ${jobid1} &&
    flux job wait-event -t 10 ${jobid3} start
'

test_expect_success 'priority: cancel all jobs' '
    flux job cancel ${jobid2} &&
    flux job cancel ${jobid3} &&
    flux job wait-event -t 10 ${jobid2} clean &&
    flux job wait-event -t 10 ${jobid3} release
'

test_expect_success 'priority: removing fluxion modules' '
    remove_qmanager &&
    remove_resource
'

# Reload the core scheduler so that rc3 won't hang waiting for
# queue to become idle after jobs are canceled.
test_expect_success 'priority: load sched-simple module' '
    flux module load sched-simple
'

test_done


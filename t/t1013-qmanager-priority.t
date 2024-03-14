#!/bin/sh

test_description='Fluxion takes into account urgency and t_submit'

. `dirname $0`/sharness.sh

hwloc_basepath=`readlink -e ${SHARNESS_TEST_SRCDIR}/data/hwloc-data`
# 1 node, 2 sockets, 44 cores (22 per socket), 4 gpus (2 per socket)
excl_1N1B="${hwloc_basepath}/001N/exclusive/01-brokers-sierra2"

skip_all_unless_have jq

export FLUX_SCHED_MODULE=none
test_under_flux 1

test_expect_success 'load test resources' '
    load_test_resources ${excl_1N1B}
'

test_expect_success 'priority: loading fluxion modules works' '
    load_resource &&
    load_qmanager
'

test_expect_success 'priority: a full-size job can be scheduled and run' '
    jobid1=$(flux submit -N 1 -n 1 -c 44 -g 4 -t 1h \
--urgency 16 sleep 3600) &&
    flux job wait-event -t 10 ${jobid1} start
'

test_expect_success 'priority: 2 jobs with higher urgency will not run' '
    jobid2=$(flux submit -N 1 -n 1 -c 44 -g 4 -t 1h \
--urgency 17 sleep 3600) &&
    jobid3=$(flux submit -N 1 -n 1 -c 44 -g 4 -t 1h \
--urgency 18 sleep 3600) &&
    test_must_fail flux job wait-event -t 1 ${jobid2} start
'

test_expect_success 'priority: canceling the first job starts the last job' '
    flux cancel ${jobid1} &&
    flux job wait-event -t 10 ${jobid3} start
'

test_expect_success 'priority: submit job with higher urgency' '
    jobid4=$(flux submit -N 1 -n 1 -c 44 -g 4 -t 1h \
--urgency 20 sleep 3600) &&
    test_must_fail flux job wait-event -t 1 ${jobid4} start
'

test_expect_success 'priority: change older job to higher urgency' '
    flux job urgency ${jobid2} 22 &&
    flux job wait-event -t 10 -c 2 ${jobid2} priority
'

test_expect_success 'priority: canceling the running job starts the earlier job' '
    flux cancel ${jobid3} &&
    flux job wait-event -t 10 ${jobid2} start
'

test_expect_success 'priority: cancel all jobs' '
    flux cancel ${jobid4} &&
    flux cancel ${jobid2} &&
    flux job wait-event -t 10 ${jobid4} clean &&
    flux job wait-event -t 10 ${jobid2} release
'

test_expect_success 'cleanup active jobs' '
    cleanup_active_jobs
'

test_expect_success 'priority: removing fluxion modules' '
    remove_qmanager &&
    remove_resource
'

test_done


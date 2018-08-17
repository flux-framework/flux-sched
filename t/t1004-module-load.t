#!/bin/sh
#set -x

test_description='Test the ability to load and unload sched plugins on the fly

Ensure sched can load a plugin, submit some jobs, unload the plugin.
load another plugin, and not lose any jobs.
'
. `dirname $0`/sharness.sh

basepath=`readlink -e ${SHARNESS_TEST_SRCDIR}/data/hwloc-data`
# each of the 4 brokers manages an exclusive set of cores (4) of the cab node
excl_1N4B=$basepath/001N/exclusive/04-brokers
excl_1N4B_nc=4
qd=512

#
# test_under_flux is under sharness.d/
#
test_under_flux 4

#
# print only with --debug
#
test_debug '
    echo ${basepath} &&
    echo ${excl_1N4B} &&
    echo ${excl_1N4B_nc}
'

test_expect_success 'module-load: sched loads the fcfs plugin' '
    adjust_session_info 4 &&
    flux hwloc reload ${excl_1N4B} &&
    flux module load sched sched-once=true sched-params=queue-depth=${qd} &&
    flux module list sched &&
    timed_wait_job 5
'

test_expect_success 'module-load: submit and verify jobs to the fcfs plugin' '
    submit_1N_nproc_sleep_jobs ${excl_1N4B_nc} 0 &&
    timed_sync_wait_job 10 &&
    verify_1N_nproc_sleep_jobs ${excl_1N4B_nc}
'

test_expect_success 'module-load: submit extra jobs to the fcfs plugin' '
    adjust_session_info 4 &&
    submit_1N_nproc_sleep_jobs ${excl_1N4B_nc} 0
'

test_expect_success 'module-load: sched unloads the fcfs plugin' '
    flux module remove sched.fcfs &&
    flux module list sched
'

test_expect_success 'module-load: sched loads the backfill plugin' '
    flux module load sched.backfill &&
    flux module list sched
'

test_expect_success 'module-load: sched unloads the backfill plugin' '
    flux module remove sched.backfill &&
    flux module list sched
'

test_expect_success 'module-load: sched loads the topo plugin' '
    flux module load sched.topo &&
    flux module list sched
'

test_expect_success 'module-load: sched unloads the topo plugin' '
    flux module remove sched.topo &&
    flux module list sched
'

test_expect_success 'module-load: loads backfill with an argument meant for sched' '
    test_must_fail flux module load sched.backfill sched-once=true &&
    flux module remove sched.backfill
'

test_expect_success 'module-load: loads backfill with too deep reservation' '
    test_must_fail flux module load sched.backfill reserve-depth=$((qd+1000)) &&
    flux module remove sched.backfill
'

test_expect_success 'module-load: loads with slightly invalid reservation depth' '
    test_must_fail flux module load sched.backfill reserve-depth=$((qd+2)) &&
    flux module remove sched.backfill
'

test_expect_success 'module-load: sched loads the backfill plugin with arguments' '
    flux module load sched.backfill reserve-depth=-1 &&
    flux module list sched
'

test_expect_success 'module-load: no jobs are lost' '
    for i in `seq $sched_start_jobid $sched_end_jobid`
    do
        state=$(flux kvs get -j $(job_kvs_path $i).state)
        if test $state != "submitted"; then
            return 48
        fi
    done &&
    flux module remove sched
'

test_done

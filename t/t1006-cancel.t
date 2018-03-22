#!/bin/sh
#set -x

test_description='Test cancel

Ensure flux-wreck-cancel can cancel pending jobs.
'
. `dirname $0`/sharness.sh

basepath=`readlink -e ${SHARNESS_TEST_SRCDIR}/data/hwloc-data`
# each of the 4 brokers manages an exclusive set of cores (4) of the cab node
excl_1N4B=$basepath/001N/exclusive/04-brokers
excl_1N4B_nc=4

#
# test_under_flux is under sharness.d/
#
test_under_flux 4

test_debug '
    echo ${basepath} &&
    echo ${excl_1N4B} &&
    echo ${excl_1N4B_nc}
'

test_expect_success 'cancel: cancelling a pending job works' '
    adjust_session_info 4 &&
    flux hwloc reload ${excl_1N4B} &&
    flux module load sched sched-once=true &&
    timed_wait_job 5 &&
    submit_1N_nproc_sleep_jobs ${excl_1N4B_nc} 0 &&
    timed_sync_wait_job 10 &&
    verify_1N_nproc_sleep_jobs ${excl_1N4B_nc} &&
    adjust_session_info 4 &&
    timed_wait_job 8 submitted &&
    submit_1N_nproc_sleep_jobs ${excl_1N4B_nc} 0 &&
    timed_sync_wait_job 10 &&
    flux wreck cancel 7 &&
    state=$(flux kvs get -j $(job_kvs_path 7).state) &&
    test ${state} = "cancelled"
'

test_expect_success 'cancel: attempt to cancel nonexistent jobs must fail' '
    test_must_fail flux wreck cancel 10 &&
    test_must_fail flux wreck cancel 100
'

test_expect_success 'cancel: attempt to cancel completed jobs must fail' '
    test_must_fail flux wreck cancel 1 &&
    state=$(flux kvs get -j $(job_kvs_path 1).state) &&
    test ${state} = "complete" &&
    state=$(flux kvs get -j $(job_kvs_path 2).state) &&
    test_must_fail flux wreck cancel 2 &&
    test ${state} = "complete" &&
    flux module remove sched
'

test_done

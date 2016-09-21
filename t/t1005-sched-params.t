#!/bin/sh
#set -x

test_description='Test the basics of scheduling optimization parameters

Ensure jobs are correctly scheduled under different values of
scheduling optimization parameters and their combinations
'

. `dirname $0`/sharness.sh

basepath=`readlink -e ${SHARNESS_TEST_SRCDIR}/data/hwloc-data`
# each of the 4 brokers manages a full cab node exclusively
excl_4N4B=$basepath/004N/exclusive/04-brokers
excl_4N4B_nc=16

#
# test_under_flux is under sharness.d/
#
test_under_flux 4

#
# print only with --debug
#
test_debug '
    echo ${basepath} &&
    echo ${excl_4N4B} &&
    echo ${excl_4N4B_nc}
'

test_expect_success 'sched-params: works with a short queue depth (4)' '
    adjust_session_info 4 &&
    flux hwloc reload ${excl_4N4B} &&
    flux module load sched sched-once=true sched-params=queue-depth=4 &&
    timed_wait_job 5 &&
    submit_1N_nproc_sleep_jobs ${excl_4N4B_nc} 0 &&
    timed_sync_wait_job 10 &&
    verify_1N_nproc_sleep_jobs ${excl_4N4B_nc} 
'

test_expect_success 'sched-params: works with queue-depth=16' '
    adjust_session_info 4 &&
    flux module remove sched &&
    flux hwloc reload ${excl_4N4B} &&
    flux module load sched sched-once=true sched-params=queue-depth=16 &&
    timed_wait_job 5 &&
    submit_1N_nproc_sleep_jobs ${excl_4N4B_nc} 0 &&
    timed_sync_wait_job 10 &&
    verify_1N_nproc_sleep_jobs ${excl_4N4B_nc} 
'

test_expect_success 'sched-params: works with a long queue depth (2048)' '
    adjust_session_info 4 &&
    flux module remove sched &&
    flux hwloc reload ${excl_4N4B} &&
    flux module load sched sched-once=true sched-params=queue-depth=2048 &&
    timed_wait_job 5 &&
    submit_1N_nproc_sleep_jobs ${excl_4N4B_nc} 0 &&
    timed_sync_wait_job 10 &&
    verify_1N_nproc_sleep_jobs ${excl_4N4B_nc} 
'

test_expect_success 'sched-params: works with delay-sched=true' '
    adjust_session_info 4 &&
    flux module remove sched &&
    flux hwloc reload ${excl_4N4B} &&
    flux module load sched sched-once=true sched-params=delay-sched=true &&
    timed_wait_job 5 &&
    submit_1N_nproc_sleep_jobs ${excl_4N4B_nc} 0 &&
    timed_sync_wait_job 10 &&
    verify_1N_nproc_sleep_jobs ${excl_4N4B_nc} 
'

test_expect_success 'sched-params: works with delay-sched=false' '
    adjust_session_info 4 &&
    flux module remove sched &&
    flux hwloc reload ${excl_4N4B} &&
    flux module load sched sched-once=true sched-params=delay-sched=false &&
    timed_wait_job 5 &&
    submit_1N_nproc_sleep_jobs ${excl_4N4B_nc} 0 &&
    timed_sync_wait_job 10 &&
    verify_1N_nproc_sleep_jobs ${excl_4N4B_nc} 
'

test_expect_success 'sched-params: delay can be combined with a short depth' '
    adjust_session_info 4 &&
    flux module remove sched &&
    flux hwloc reload ${excl_4N4B} &&
    flux module load sched sched-once=true \
sched-params=delay-sched=true,queue-depth=16 &&
    timed_wait_job 5 &&
    submit_1N_nproc_sleep_jobs ${excl_4N4B_nc} 0 &&
    timed_sync_wait_job 10 &&
    verify_1N_nproc_sleep_jobs ${excl_4N4B_nc} 
'

test_expect_success 'sched-params: no-delay combines with a long depth' '
    adjust_session_info 4 &&
    flux module remove sched &&
    flux hwloc reload ${excl_4N4B} &&
    flux module load sched sched-once=true \
sched-params=queue-depth=2048,delay-sched=false &&
    timed_wait_job 5 &&
    submit_1N_nproc_sleep_jobs ${excl_4N4B_nc} 0 &&
    timed_sync_wait_job 10 &&
    verify_1N_nproc_sleep_jobs ${excl_4N4B_nc} 
'

test_done

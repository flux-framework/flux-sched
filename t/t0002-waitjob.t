#!/bin/sh
#set -x

test_description='Test flux-waitjob 

Ensure flux-waitjob works as expected.
'
. `dirname $0`/sharness.sh

#
# test_under_flux is under sharness.d/
#
test_under_flux 4

test_expect_success  'waitjob: load sched module' '
    flux module load sched
'

test_expect_success  'waitjob: works when the job has not started' '
    adjust_session_info 1 &&
    timed_wait_job 5 &&
    flux submit -N 4 -n 4 hostname &&
    timed_sync_wait_job 5
'

test_expect_success 'waitjob: works when the job has already completed' '
    timed_wait_job 5 &&
    timed_sync_wait_job 5
'

test_expect_success 'waitjob: works when the job started but has not completed' '
    adjust_session_info 1 &&
    flux submit -N 4 -n 4 sleep 2 &&
    timed_wait_job 3 &&
    timed_sync_wait_job 3
'

test_expect_success  'waitjob: remove sched module' '
    flux module remove sched
'

test_done

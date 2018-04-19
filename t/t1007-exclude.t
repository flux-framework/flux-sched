#!/bin/sh
#set -x

test_description='Test node exclude/include command

Ensure flux-wreck-exclude can exclude a node from being scheduled.
Ensure flux-wreck-include can include a node back to be scheduled.
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

test_debug '
    echo ${basepath} &&
    echo ${excl_4N4B} &&
    echo ${excl_4N4B_nc}
'

test_expect_success 'excluding a node with no job allocated or reserved works' '
    adjust_session_info 4 &&
    flux hwloc reload ${excl_4N4B} &&
    flux module load sched sched-once=true node-excl=true &&
    flux wreck exclude cab1234 &&
    timed_wait_job 5 submitted &&
    flux submit -N 1 sleep 0 &&
    flux submit -N 1 sleep 0 &&
    flux submit -N 1 sleep 0 &&
    flux submit -N 1 sleep 0 &&
    timed_sync_wait_job 10 &&
    state=$(flux kvs get -j $(job_kvs_path 4).state) &&
    test ${state} = "submitted"
'

test_expect_success 'including an excluded node works' '
    timed_wait_job 5 &&
    flux wreck include cab1234 &&
    timed_sync_wait_job 10 &&
    state=$(flux kvs get -j $(job_kvs_path 4).state) &&
    test ${state} = "complete"
'

test_expect_success 'excluding a node does not kill the jobs' '
    adjust_session_info 1 &&
    flux module remove sched &&
    flux module load sched sched-once=true node-excl=true &&
    timed_wait_job 5 running &&
    flux submit -N 1 sleep 120 &&
    timed_sync_wait_job 10 &&
    flux wreck exclude cab1234 &&
    state=$(flux kvs get -j $(job_kvs_path 5).state) &&
    test ${state} = "running" &&
    flux wreck cancel -f 5 &&
    state=$(flux kvs get -j $(job_kvs_path 5).state) &&
    test ${state} = "complete" &&
    flux wreck include cab1234
'

test_expect_success '-k option kills the jobs using the node' '
    adjust_session_info 1 &&
    timed_wait_job 5 running &&
    flux submit -N 1 sleep 120 &&
    timed_sync_wait_job 10 &&
    flux wreck exclude --kill cab1235 &&
    state=$(flux kvs get -j $(job_kvs_path 6).state) &&
    test ${state} = "complete" &&
    flux wreck include cab1235
'

test_expect_success 'excluding a node with reservations works' '
    adjust_session_info 4 &&
    flux module remove sched &&
    flux module load sched sched-once=true node-excl=true plugin=sched.backfill &&
    timed_wait_job 5 submitted &&
    flux submit -N 4 sleep 0 &&
    flux submit -N 4 sleep 0 &&
    flux submit -N 4 sleep 0 &&
    flux submit -N 4 sleep 0 &&
    timed_sync_wait_job 10 &&
    flux wreck exclude -k cab1235 &&
    state=$(flux kvs get -j $(job_kvs_path 7).state) &&
    test ${state} = "complete"
'

test_expect_success 'attempting to exclude or include an invalid node must fail' '
    flux module remove sched &&
    flux module load sched node-excl=true &&
    test_must_fail flux wreck exclude foo &&
    test_must_fail flux wreck exclude -k bar &&
    test_must_fail flux wreck include foo
'

test_expect_success 'unloaded sched module' '
    flux module remove sched
'

test_done

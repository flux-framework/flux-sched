#!/bin/sh
#set -x

test_description='Stress the system by submitting large numbers of jobs'

. `dirname $0`/sharness.sh

if test "$TEST_LONG" = "t"; then
    test_set_prereq LONGTEST
fi

tdir=`readlink -e ${SHARNESS_TEST_SRCDIR}/../`
schedsrv=`readlink -e ${SHARNESS_TEST_SRCDIR}/../sched/schedsrv.so`
basepath=`readlink -e ${SHARNESS_TEST_SRCDIR}/data/hwloc-data`
excl_4N4B=$basepath/004N/exclusive/04-brokers
excl_4N4B_m_RDL=$basepath/004N/exclusive/cab.hwloc1.lua
excl_4N4B_nc=16

#
# test_under_flux is under sharness.d/
#
test_under_flux 4 $tdir
set_tdir $tdir
set_instance_size 4

#
# print only with --debug
#
test_debug '
    echo ${tdir} &&
    echo ${schedsrv} &&
    echo ${basepath} &&
    echo ${excl_4N4B} &&
    echo ${excl_4N4B_m_RDL} &&
    echo ${excl_4N4B_nc}
'

test_expect_success LONGTEST 'stress: submit and execute 1000 sleep 0 jobs (max allowed: 1h)' '
    adjust_session_info 1000 &&
    flux hwloc reload ${excl_4N4B} &&
    flux module load ${schedsrv} rdl-conf=${excl_4N4B_m_RDL} &&
    date > begin.$(get_session)
    timed_wait_job 5 &&
    submit_1N_nproc_sleep_jobs ${excl_4N4B_nc} 0 &&
    timed_sync_wait_job 3600 &&
    date > end.$(get_session)
'

test_expect_success LONGTEST 'stress: repeat second time (max allowed: 1h)' '
    adjust_session_info 1000 &&
    flux module remove sched &&
    flux hwloc reload ${excl_4N4B} &&
    flux module load ${schedsrv} rdl-conf=${excl_4N4B_m_RDL} &&
    date > begin.$(get_session)
    timed_wait_job 5 &&
    submit_1N_nproc_sleep_jobs ${excl_4N4B_nc} 0 &&
    timed_sync_wait_job 3600 &&
    date > end.$(get_session)
'

test_expect_success LONGTEST 'stress: repeat third time (max allowed: 1h)' '
    adjust_session_info 1000 &&
    flux module remove sched &&
    flux hwloc reload ${excl_4N4B} &&
    flux module load ${schedsrv} rdl-conf=${excl_4N4B_m_RDL} &&
    date > begin.$(get_session)
    timed_wait_job 5 &&
    submit_1N_nproc_sleep_jobs ${excl_4N4B_nc} 0 &&
    timed_sync_wait_job 3600 &&
    date > end.$(get_session)
'

test_expect_success LONGTEST 'stress: repeat 4th time (max allowed: 1h)' '
    adjust_session_info 1000 &&
    flux module remove sched &&
    flux hwloc reload ${excl_4N4B} &&
    flux module load ${schedsrv} rdl-conf=${excl_4N4B_m_RDL} &&
    date > begin.$(get_session)
    timed_wait_job 5 &&
    submit_1N_nproc_sleep_jobs ${excl_4N4B_nc} 0 &&
    timed_sync_wait_job 3600 &&
    date > end.$(get_session)
'

test_expect_success LONGTEST 'stress: repeat 5th time (max allowed: 1h)' '
    adjust_session_info 1000 &&
    flux module remove sched &&
    flux hwloc reload ${excl_4N4B} &&
    flux module load ${schedsrv} rdl-conf=${excl_4N4B_m_RDL} &&
    date > begin.$(get_session)
    timed_wait_job 5 &&
    submit_1N_nproc_sleep_jobs ${excl_4N4B_nc} 0 &&
    timed_sync_wait_job 3600 &&
    date > end.$(get_session)
'

test_expect_success LONGTEST 'stress: repeat 6th time (max allowed: 1h)' '
    adjust_session_info 1000 &&
    flux module remove sched &&
    flux hwloc reload ${excl_4N4B} &&
    flux module load ${schedsrv} rdl-conf=${excl_4N4B_m_RDL} &&
    date > begin.$(get_session)
    timed_wait_job 5 &&
    submit_1N_nproc_sleep_jobs ${excl_4N4B_nc} 0 &&
    timed_sync_wait_job 3600 &&
    date > end.$(get_session)
'

test_expect_success LONGTEST 'stress: repeat 7th time (max allowed: 1h)' '
    adjust_session_info 1000 &&
    flux module remove sched &&
    flux hwloc reload ${excl_4N4B} &&
    flux module load ${schedsrv} rdl-conf=${excl_4N4B_m_RDL} &&
    date > begin.$(get_session)
    timed_wait_job 5 &&
    submit_1N_nproc_sleep_jobs ${excl_4N4B_nc} 0 &&
    timed_sync_wait_job 3600 &&
    date > end.$(get_session)
'

test_expect_success LONGTEST 'stress: repeat 8th time (max allowed: 1h)' '
    adjust_session_info 1000 &&
    flux module remove sched &&
    flux hwloc reload ${excl_4N4B} &&
    flux module load ${schedsrv} rdl-conf=${excl_4N4B_m_RDL} &&
    date > begin.$(get_session)
    timed_wait_job 5 &&
    submit_1N_nproc_sleep_jobs ${excl_4N4B_nc} 0 &&
    timed_sync_wait_job 3600 &&
    date > end.$(get_session)
'

test_expect_success LONGTEST 'stress: repeat 9th time (max allowed: 1h)' '
    adjust_session_info 1000 &&
    flux module remove sched &&
    flux hwloc reload ${excl_4N4B} &&
    flux module load ${schedsrv} rdl-conf=${excl_4N4B_m_RDL} &&
    date > begin.$(get_session)
    timed_wait_job 5 &&
    submit_1N_nproc_sleep_jobs ${excl_4N4B_nc} 0 &&
    timed_sync_wait_job 3600 &&
    date > end.$(get_session)
'

test_expect_success LONGTEST 'stress: repeat 10th time (max allowed: 1h)' '
    adjust_session_info 1000 &&
    flux module remove sched &&
    flux hwloc reload ${excl_4N4B} &&
    flux module load ${schedsrv} rdl-conf=${excl_4N4B_m_RDL} &&
    date > begin.$(get_session)
    timed_wait_job 5 &&
    submit_1N_nproc_sleep_jobs ${excl_4N4B_nc} 0 &&
    timed_sync_wait_job 3600 &&
    date > end.$(get_session)
'

test_done

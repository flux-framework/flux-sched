#!/bin/sh
#set -x

test_description='Test the basics of rs2rank in a small scale instance

Ensure rs2rank correctly targets the broker ranks that  manage and control
the nodes allocated to a job.
'
. `dirname $0`/sharness.sh

test "$TRAVISHAPPY" = "t" && test_set_prereq TRAVISHAPPY
if ! test_have_prereq TRAVISHAPPY; then
    skip_all='Travis not happy yet!'
    test_done
fi

basepath=`readlink -e ${SHARNESS_TEST_SRCDIR}/data/hwloc-data`
# each of the 4 brokers manages an exclusive set of cores (4) of the cab node 
excl_1N4B=$basepath/001N/exclusive/04-brokers
excl_1N4B_nc=4
# full 16-core cab node resources controlled by 4 brokers
shrd_1N4B=$basepath/001N/shared/04-brokers
shrd_1N4B_nc=16
# each of the 4 brokers manages a full cab node exclusively
excl_4N4B=$basepath/004N/exclusive/04-brokers
excl_4N4B_m_RDL=$basepath/004N/exclusive/cab.hwloc1.lua
excl_4N4B_um_RDL=$basepath/004N/exclusive/cab.hwloc2.lua
excl_4N4B_um_RDL2=$basepath/004N/exclusive/cab.hwloc3.lua
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
    echo ${excl_1N4B} &&
    echo ${excl_1N4B_nc} &&
    echo ${shrd_1N4B} &&
    echo ${shrd_1N4B_nc} && 
    echo ${excl_4N4B} &&
    echo ${excl_4N4B_m_RDL} &&
    echo ${excl_4N4B_um_RDL} &&
    echo ${excl_4N4B_um_RDL2} &&
    echo ${excl_4N4B_nc}
'

test_expect_success 'rs2rank: multiple ranks manage a node in a shared fashion' '
    adjust_session_info 4 &&
    flux hwloc reload ${shrd_1N4B} &&
    flux module load sched sched-once=true &&
    timed_wait_job 5 &&
    submit_1N_nproc_sleep_jobs ${shrd_1N4B_nc} 0 &&
    timed_sync_wait_job 10 &&
    verify_1N_nproc_sleep_jobs ${shrd_1N4B_nc} 
'

test_expect_success 'rs2rank: each manages an exclusive set of cores of a node' '
    adjust_session_info 4 &&
    flux module remove sched &&
    flux hwloc reload ${excl_1N4B} &&
    flux module load sched sched-once=true &&
    timed_wait_job 5 &&
    submit_1N_nproc_sleep_jobs ${excl_1N4B_nc} 0 &&
    timed_sync_wait_job 10 &&
    verify_1N_nproc_sleep_jobs ${excl_1N4B_nc} 
'

test_expect_success 'rs2rank: each manages a node exclusively' '
    adjust_session_info 4 &&
    flux module remove sched &&
    flux hwloc reload ${excl_4N4B} &&
    flux module load sched sched-once=true &&
    timed_wait_job 5 &&
    submit_1N_nproc_sleep_jobs ${excl_4N4B_nc} 0 &&
    timed_sync_wait_job 10 &&
    verify_1N_nproc_sleep_jobs ${excl_4N4B_nc} 
'

test_expect_success 'rs2rank: works with a matched RDL' '
    adjust_session_info 4 &&
    flux module remove sched &&
    flux hwloc reload ${excl_4N4B} &&
    flux module load sched rdl-conf=${excl_4N4B_m_RDL} sched-once=true &&
    timed_wait_job 5 &&
    submit_1N_nproc_sleep_jobs ${excl_4N4B_nc} 0 &&
    timed_sync_wait_job 10 &&
    verify_1N_nproc_sleep_jobs ${excl_4N4B_nc} 
'

test_expect_success 'rs2rank: works with an inconsistent RDL (fewer cores)' '
    adjust_session_info 4 &&
    flux module remove sched &&
    flux hwloc reload ${excl_4N4B} &&
    flux module load sched rdl-conf=${excl_4N4B_um_RDL} sched-once=true &&
    timed_wait_job 5 &&
    submit_1N_nproc_sleep_jobs ${excl_4N4B_nc} 0 &&
    timed_sync_wait_job 10 &&
    verify_1N_nproc_sleep_jobs ${excl_4N4B_nc} 
'

test_expect_success 'rs2rank: works with an inconsistent RDL (fewer nodes)' '
    adjust_session_info 4 &&
    flux module remove sched &&
    flux hwloc reload ${excl_4N4B} &&
    flux module load sched rdl-conf=${excl_4N4B_um_RDL2} sched-once=true &&
    timed_wait_job 5 &&
    submit_1N_nproc_sleep_jobs ${excl_4N4B_nc} 0 &&
    timed_sync_wait_job 10 &&
    verify_1N_nproc_sleep_jobs ${excl_4N4B_nc} 
'

test_done

#!/bin/sh
#set -x

test_description='Test the basics of rs2rank in a larger instance

Ensure rs2rank correctly targets the broker ranks that manage and control
the nodes allocated to a job.
'
. `dirname $0`/sharness.sh

if ! test_have_prereq LONGTEST; then
    skip_all='LONGTEST not set, skipping...'
    test_done
fi

tdir=`readlink -e ${SHARNESS_TEST_SRCDIR}/../`
schedsrv=`readlink -e ${SHARNESS_TEST_SRCDIR}/../sched/.libs/schedsrv.so`
rdlconf=`readlink -e ${SHARNESS_TEST_SRCDIR}/../conf/hype.lua`
basepath=`readlink -e ${SHARNESS_TEST_SRCDIR}/data/hwloc-data`
# each of the 64 brokers manages a full cab node exclusively
excl_64N64B=$basepath/064N/exclusive/64-brokers
excl_64N64B_nc=16

#
# test_under_flux is under sharness.d/
#
test_under_flux 64 $tdir
set_tdir $tdir
set_instance_size 64


test_expect_success LONGTEST 'rs2rank: works with 64-N instance' '
    adjust_session_info 64 &&
    flux hwloc reload ${excl_64N64B} &&
    flux module load ${schedsrv} sched-once=true &&
    timed_wait_job 5 &&
    submit_1N_nproc_sleep_jobs ${excl_64N64B_nc} 0 &&
    timed_sync_wait_job 30 &&
    verify_1N_nproc_sleep_jobs ${excl_64N64B_nc} 
'

test_done

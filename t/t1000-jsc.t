#!/bin/sh
#set -x

test_description='Test JSC with schedsrv 

Ensure JSC works as expected with schedsrv.
'
. `dirname $0`/sharness.sh

tdir=`readlink -e ${SHARNESS_TEST_SRCDIR}/../`
schedsrv=`readlink -e ${SHARNESS_TEST_SRCDIR}/../sched/.libs/schedsrv.so`
rdlconf=`readlink -e ${SHARNESS_TEST_SRCDIR}/../conf/hype.lua`

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
	echo ${rdlconf}
'

tr1="null->null"
tr2="null->reserved"
tr3="reserved->submitted"
tr4="submitted->allocated"
tr5="allocated->runrequest"
tr6="runrequest->starting"
tr7="starting->running"
tr8="running->complete"
trans="$tr1
$tr2
$tr3
$tr4
$tr5
$tr6
$tr7
$tr8"

test_expect_success 'jsc: expected job-event sequence for single-job scheduling' '
    adjust_session_info 1 && 
    flux module load ${schedsrv} rdl-conf=${rdlconf} &&
    p=$(timed_run_flux_jstat output) &&
    timed_wait_job 5 &&
    flux submit -N 4 -n 4 hostname &&
    cat >expected1 <<-EOF &&
$trans
EOF
    timed_sync_wait_job 5 &&
    cp output.$(get_session) output.$(get_session).cp &&
    kill -INT $p &&
    test_cmp expected1 output.$(get_session).cp 
'

test_expect_success 'jsc: expected job-event sequence for multiple-job scheduling' '
    adjust_session_info 5 && 
    p=$(timed_run_flux_jstat output) &&
    timed_wait_job 5 &&
    flux submit -N 4 -n 4 sleep 1 &&
    flux submit -N 4 -n 4 sleep 1 &&
    flux submit -N 4 -n 4 sleep 1 &&
    flux submit -N 4 -n 4 sleep 1 &&
    flux submit -N 4 -n 4 sleep 1 &&
    cat >expected2 <<-EOF &&
$trans
$trans
$trans
$trans
$trans
EOF
    timed_sync_wait_job 15 &&
    cp output.$(get_session) output.$(get_session).cp &&
    sort expected2 > expected2.sort &&
    sort output.$(get_session).cp > output.$(get_session).cp.sort &&
    kill -INT $p &&
    test_cmp expected2.sort output.$(get_session).cp.sort 
'

test_expect_success 'jsc: expected single-job-event sequence in hwloc reader mode' '
    adjust_session_info 1 && 
    flux module remove sched &&
    flux module load ${schedsrv} &&
    p=$(timed_run_flux_jstat output) &&
    timed_wait_job 5 &&
    flux submit -N 4 -n 4 hostname &&
    cat >expected3 <<-EOF &&
$trans
EOF
    timed_sync_wait_job 5 &&
    cp output.$(get_session) output.$(get_session).cp &&
    kill -INT $p &&
    test_cmp expected3 output.$(get_session).cp 
'

test_expect_success 'jsc: expected multi-job-event sequence in hwloc reader mode' '
    adjust_session_info 5 && 
    p=$(timed_run_flux_jstat output) &&
    timed_wait_job 5 &&
    flux submit -N 4 -n 4 sleep 1 &&
    flux submit -N 4 -n 4 sleep 1 &&
    flux submit -N 4 -n 4 sleep 1 &&
    flux submit -N 4 -n 4 sleep 1 &&
    flux submit -N 4 -n 4 sleep 1 &&
    cat >expected4 <<-EOF &&
$trans
$trans
$trans
$trans
$trans
EOF
    timed_sync_wait_job 15 &&
    cp output.$(get_session) output.$(get_session).cp &&
    sort expected4 > expected4.sort &&
    sort output.$(get_session).cp > output.$(get_session).cp.sort &&
    kill -INT $p &&
    test_cmp expected4.sort output.$(get_session).cp.sort 
'

test_done

#!/bin/sh
#set -x

test_description='Test flux-waitjob 

Ensure flux-waitjob works as expected.
'

#
# variables
#
dn=`dirname $0` 
tdir=`readlink -e $dn/../`
schedsrv=`readlink -e $dn/../sched/schedsrv.so`
rdlconf=`readlink -e $dn/../conf/hype.lua`

#
# source sharness from the directore where this test
# file resides
#
. ${dn}/sharness.sh

#
# print only with --debug
#
test_debug '
	echo ${tdir} &&
	echo ${schedsrv} &&
	echo ${rdlconf}
'
#
# test_under_flux is under sharness.d/
#
test_under_flux 4 $tdir

timed_wait_job () {
    sess=$1
    jobid=$2
    to=$3
    rc=0
    flux -x$tdir/sched waitjob -s wo.st.$sess -c wo.end.$sess $jobid &
    while [ ! -f wo.st.$sess -a $to -ge 0 ]
    do
        sleep 1
        to=`expr $to - 1`
    done
    if [ $to -lt 0 ]; then
        rc=48 
    fi
    return $rc
}

timed_sync_wait_job () {
    sess=$1
    to=$2 
    rc=0
    while [ ! -f wo.end.$sess -a $to -ge 0 ]
    do
        sleep 1
        to=`expr $to - 1`
    done
    if [ $to -lt 0 ]; then
        rc=48 
    fi
    return $rc
}

test_expect_success 'waitjob: works when the job has not started' '
    flux module load ${schedsrv} rdl-conf=${rdlconf} &&
    timed_wait_job 1 1 5 &&
    flux -x$tdir/sched submit -N 4 -n 4 hostname &&
    timed_sync_wait_job 1 5
'

test_expect_success 'waitjob: works when the job has already completed' '
    timed_wait_job 2 1 5 &&
    timed_sync_wait_job 1 5
'

test_expect_success 'waitjob: works when the job started but has not completed' '
    flux -x$tdir/sched submit -N 4 -n 4 sleep 5 &&
    timed_wait_job 3 2 3 &&
    timed_sync_wait_job 3 6
'

test_done

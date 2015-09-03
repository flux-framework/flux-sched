#!/bin/sh
#set -x

test_description='Test JSC with schedsrv 

Ensure JSC works as expected with schedsrv.
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


#
# test_under_flux is under sharness.d/
#
test_under_flux 4 $tdir

run_flux_jstat () {
    sess=$1
    rm -f jstat$sess.pid
    (
        # run this in a subshell
        flux jstat -o output.$sess notify &
        p=$!
        cat <<HEREDOC > jstat$sess.pid
$p
HEREDOC
        wait $p
        #rm -f output.$sess
    )&
    return 0
}

sync_flux_jstat () {
    sess=$1
    while [ ! -f output.$sess ]
    do
        sleep 2
    done
    p=`cat jstat$sess.pid`
    echo $p
}

wait_job () {
    sess=$1
    jobid=$2
    rm -f waitjob$sess.pid
    (
        # run this in a subshell
        flux -x$tdir/sched waitjob -o waitout.$sess $jobid &
        p=$!
        cat <<HEREDOC > waitjob$sess.pid
$p
HEREDOC
        wait $p
        #rm -f output.$sess
    )&
    return 0
}

sync_wait_job () {
    sess=$1
    while [ ! -f waitout.$sess ]
    do
        sleep 2
    done
    return 0
}

test_expect_success 'jsc: expected job-event sequence for single-job scheduling' '
    flux module load ${schedsrv} rdl-conf=${rdlconf} &&
    run_flux_jstat 1 &&
    p=$( sync_flux_jstat 1) &&
    wait_job 1 1 &&
    flux -x$tdir/sched submit -N 4 -n 4 hostname &&
    cat >expected1 <<-EOF &&
$trans
EOF
    sync_wait_job 1 &&
    cp output.1 output.1.cp &&
    kill -INT $p &&
    test_cmp expected1 output.1.cp 
'

test_expect_success 'jsc: expected job-event sequence for multiple-job scheduling' '
    run_flux_jstat 2 &&
    p=$( sync_flux_jstat 2) &&
    wait_job 2 6 &&
    flux -x$tdir/sched submit -N 4 -n 4 sleep 2 &&
    flux -x$tdir/sched submit -N 4 -n 4 sleep 2 &&
    flux -x$tdir/sched submit -N 4 -n 4 sleep 2 &&
    flux -x$tdir/sched submit -N 4 -n 4 sleep 2 &&
    flux -x$tdir/sched submit -N 4 -n 4 sleep 2 &&
    cat >expected2 <<-EOF &&
$trans
$trans
$trans
$trans
$trans
EOF
    sync_wait_job 2 &&
    cp output.2 output.2.cp &&
    sort expected2 > expected2.sort &&
    sort output.2.cp > output.2.cp.sort &&
    kill -INT $p &&
    test_cmp expected2.sort output.2.cp.sort 
'
test_done

#!/bin/sh
#set -x

test_description='Test easy scheduler in simulator
'

#
# variables
#
dn=`dirname $0`
tdir=`readlink -e $dn/../`

sched=`readlink -e $dn/../simulator/sim_sched_easy.so`
rdlconf=`readlink -e $dn/../conf/hype-io.lua`

submit=`readlink -e $dn/../simulator/submit.so`
jobdata=`readlink -e $dn/data/hype-test.csv`

sim_exec=`readlink -e $dn/../simulator/sim_exec.so`
sim=`readlink -e $dn/../simulator/sim.so`

expected_order=`readlink -e $dn/easy_expected`

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
	echo ${sched} &&
	echo ${rdlconf} &&
    echo ${submit} &&
    echo ${jobdata} &&
    echo ${execsrv} &&
    echo ${sim} &&
'

#
# test_under_flux is under sharness.d/
#
test_under_flux 1 $tdir

test_expect_success 'loading sim works' '
	flux module load ${sim} exit-on-complete=false save-path=/tmp/
'
test_expect_success 'loading submit works' '
	flux module load ${submit} job-csv=${jobdata}
'
test_expect_success 'loading exec works' '
	flux module load ${sim_exec}
'
test_expect_success 'loading sched works' '
	flux module load ${sched} rdl-conf=${rdlconf}
'

while flux kvs get lwj.12.complete_time 2>&1 | grep -q "No such file"; do echo "Waiting for last job"; sleep 2; done
sleep 1
flux kvs dir -r lwj | grep runrequest | sed 's/\./ /g' | sort -k 5 -k 2n | cut -d ' ' -f 2 > actual

#test_expect_success 'jobs scheduled in correct order' '
#    diff -u ${expected_order} ./actual
#'

test_done

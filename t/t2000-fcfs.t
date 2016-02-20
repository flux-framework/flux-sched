#!/bin/bash
#set -x

test_description='Test fcfs scheduler in simulator
'

# source sharness from the directore where this test
# file resides
#
. $(dirname $0)/sharness.sh
FLUX_MODULE_PATH="${SHARNESS_BUILD_DIRECTORY}/simulator/.libs:${FLUX_MODULE_PATH}"

rdlconf=$(readlink -e "${SHARNESS_TEST_SRCDIR}/../conf/hype-io.lua")
jobdata=$(readlink -e "${SHARNESS_TEST_SRCDIR}/data/job-traces/hype-test.csv")
expected_order=$(readlink -e "${SHARNESS_TEST_SRCDIR}/data/emulator-data/fcfs_expected")


#
# print only with --debug
#
test_debug '
	echo ${rdlconf} &&
    echo ${submit} &&
    echo ${jobdata} &&
    echo ${sim_exec} &&
    echo ${sim} &&
    echo ${expected_order}
'

#
# test_under_flux is under sharness.d/
#
test_under_flux 1

test_expect_success 'loading sim works' '
	flux module load sim exit-on-complete=false
'
test_expect_success 'loading submit works' '
	flux module load submit job-csv=${jobdata}
'
test_expect_success 'loading exec works' '
	flux module load sim_exec
'
test_expect_success 'loading sched works' '
	flux module load sched rdl-conf=${rdlconf} in-sim=true plugin=sched.plugin1
'

while flux kvs get lwj.12.complete_time 2>&1 | grep -q "No such file"; do sleep 0.5; done
sleep 0.5
for x in $(seq 1 12); do echo "$x $(flux kvs get lwj.$x.starting_time)"; done | sort -k 2n -k 1n | cut -d ' ' -f 1 > actual

test_expect_success 'jobs scheduled in correct order' '
    diff -u ${expected_order} ./actual
'

test_done

#!/bin/bash
#set -x

test_description='Test fcfs scheduler with queue-depth=1 in simulator
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
	echo rdlconf=${rdlconf} &&
    echo jobdata=${jobdata} &&
    echo expected_order=${expected_order}
'

#
# test_under_flux is under sharness.d/
#
test_under_flux 1

test_expect_success 'sim: started successfully with queue-depth=1' '
    adjust_session_info 12 &&
    timed_wait_job 5 &&
    flux module load sim exit-on-complete=false &&
    flux module load submit job-csv=${jobdata} &&
    flux module load sim_exec &&
	flux module load sched rdl-conf=${rdlconf} in-sim=true plugin=sched.fcfs sched-params=queue-depth=1
'

test_expect_success 'sim: scheduled and ran all jobs with queue-depth=1' '
    timed_sync_wait_job 60
'

for x in $(seq 1 12); do echo "$x $(flux kvs get $(job_kvs_path $x).starting_time)"; done | sort -k 2n -k 1n | cut -d ' ' -f 1 > actual

test_expect_success 'jobs scheduled in correct order with queue-depth=1' '
    diff -u ${expected_order} ./actual
'

test_done

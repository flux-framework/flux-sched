#!/bin/sh
#set -x

test_description='Test fcfs io-aware scheduler in simulator
'

# source sharness from the directore where this test
# file resides
#
. $(dirname $0)/sharness.sh

FLUX_MODULE_PATH_PREPEND="$FLUX_MODULE_PATH_PREPEND:$(sched_build_path simulator/.libs)"

rdlconf=$(readlink -e "${SHARNESS_TEST_SRCDIR}/../conf/hype-io.lua")
jobdata=$(readlink -e "${SHARNESS_TEST_SRCDIR}/data/job-traces/hype-io-test.csv")
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

test_expect_success 'sim: started successfully' '
    adjust_session_info 12 &&
    timed_wait_job 5 &&
    flux module load sim exit-on-complete=false &&
    flux module load submit job-csv=${jobdata} &&
    flux module load sim_exec &&
	flux module load sched rdl-conf=${rdlconf} in-sim=true plugin=sched.fcfs
'

test_expect_success 'sim: scheduled and ran all jobs' '
    timed_sync_wait_job 60
'

for x in $(seq 1 12); do echo "$x $(flux kvs get $(job_kvs_path $x).starting_time)"; done | sort -k 2n -k 1n | cut -d ' ' -f 1 > actual

test_expect_success 'jobs scheduled in correct order' '
    test_cmp ${expected_order} ./actual
'

test_expect_success 'sim: unloaded' '
    flux module remove sim_exec &&
    flux module remove submit &&
    flux module remove sim &&
    flux module remove sched
'

test_done

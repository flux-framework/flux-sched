#!/bin/sh
#set -x

test_description='Test easy scheduler in simulator
'

# source sharness from the directore where this test
# file resides
#
. $(dirname $0)/sharness.sh

rdlconf=$(sched_src_path "conf/hype-io.lua")
jobdata=$(readlink -e "${SHARNESS_TEST_SRCDIR}/data/job-traces/hype-test.csv")
expected_order=$(readlink -e "${SHARNESS_TEST_SRCDIR}/data/emulator-data/easy_expected")

FLUX_MODULE_PATH_PREPEND="$FLUX_MODULE_PATH_PREPEND:$(sched_build_path simulator/.libs)"


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
	flux module load sched rdl-conf=${rdlconf} in-sim=true plugin=sched.backfill plugin-opts=reserve-depth=1
'

while flux kvs get $(job_kvs_path 12).complete_time 2>&1 | grep -q "No such file"; do sleep 0.5; done
sleep 0.5
for x in $(seq 1 12); do echo "$x $(flux kvs get $(job_kvs_path $x).submit_time)"; done | sort -k 2n -k 1n
for x in $(seq 1 12); do echo "$x $(flux kvs get $(job_kvs_path $x).starting_time)"; done | sort -k 2n -k 1n | cut -d ' ' -f 1 > actual

test_expect_success 'jobs scheduled in correct order' '
   diff -u ${expected_order} ./actual
'

test_done

#!/bin/sh

test_description='Basic python scheduler test with fluxion pool'

# Append --logfile option if FLUX_TESTS_LOGFILE is set in environment:
test -n "$FLUX_TESTS_LOGFILE" && set -- "$@" --logfile
. $(dirname $0)/sharness.sh

test_under_flux 4 job

test_expect_success 'reload sched-simple with fluxion:rv1exec pool' '
	flux module reload sched-simple pool-class=fluxion:rv1exec
'

test_expect_success 'run a simple job' '
	flux run hostname
'

test_expect_success 'run job requesting 2 cores' '
	flux run -n1 -c2 hostname
'

test_expect_success 'run job requesting 2 nodes' '
	flux run -N2 hostname
'

test_expect_success 'unsatisfiable request fails' '
	jobid=$(flux submit -n1 -c100 hostname) &&
	flux job wait-event --timeout=5.0 $jobid exception
'

test_expect_success 'run multiple concurrent jobs' '
	flux submit --cc=1-10 hostname >job.ids &&
	for id in $(cat job.ids); do flux job wait-event $id clean; done
'

test_expect_success 'resource status: mark node down and verify exclusion' '
	flux resource drain 0 &&
	jobid=$(flux submit -N1 hostname) &&
	flux job wait-event --timeout=10.0 $jobid alloc &&
	flux job info $jobid R | jq -e ".execution.R_lite[0].rank != \"0\""
'

test_expect_success 'resource status: mark node up and verify inclusion' '
	flux resource undrain 0 &&
	flux queue drain &&
	flux queue idle &&
	jobid=$(flux submit -N4 hostname) &&
	flux job wait-event --timeout=10.0 $jobid alloc &&
	flux job info $jobid R | jq -e ".execution.R_lite[0].rank == \"0-3\""
'

test_expect_success 'cancel and free work correctly' '
	jobid=$(flux submit --wait-event=alloc sleep 300) &&
	flux job wait-event $jobid alloc &&
	flux cancel $jobid &&
	flux job wait-event $jobid clean &&
	flux run hostname
'

test_expect_success 'reload with standard pool works again' '
	flux module reload sched-simple &&
	flux run hostname
'

test_done

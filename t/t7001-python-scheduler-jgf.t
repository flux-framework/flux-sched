#!/bin/sh

test_description='Test Python sched-simple with fluxion JGF pool'

# Append --logfile option if FLUX_TESTS_LOGFILE is set in environment:
test -n "$FLUX_TESTS_LOGFILE" && set -- "$@" --logfile
. $(dirname $0)/sharness.sh

test_under_flux 4 job

test_expect_success 'unload sched-simple so resources can be updated' '
	flux module remove sched-simple
'

test_expect_success 'generate JGF from current R using flux ion-R' '
	flux kvs get resource.R | flux ion-R encode >R_jgf.json &&
	test_debug "jq . R_jgf.json"
'

test_expect_success 'verify JGF has all 4 nodes' '
	test $(jq "[.scheduling.graph.nodes[] | select(.metadata.type==\"node\")] | length" R_jgf.json) -eq 4
'

test_expect_success 'verify JGF has 8 cores total (2 per node)' '
	test $(jq "[.scheduling.graph.nodes[] | select(.metadata.type==\"core\")] | length" R_jgf.json) -eq 8
'

test_expect_success 'reload resource with JGF-enabled R' '
	flux resource reload R_jgf.json
'

test_expect_success 'load sched-simple with fluxion JGF pool' '
	flux module load sched-simple pool-class=fluxion:jgf
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

test_expect_success 'queue is idle before concurrent jobs' '
	flux queue idle --timeout=10s
'

test_expect_success 'submit first job and verify it completes and frees resources' '
	jobid=$(flux submit hostname) &&
	flux job wait-event --timeout=5.0 $jobid clean &&
	flux queue idle --timeout=10s
'

test_expect_success 'submit second job to verify resources were freed' '
	jobid=$(flux submit hostname) &&
	flux job wait-event --timeout=5.0 $jobid clean &&
	flux queue idle --timeout=10s
'

test_expect_success 'submit 2 concurrent jobs (less than total resources)' '
	flux submit --cc=1-2 hostname >job.ids.small &&
	for id in $(cat job.ids.small); do flux job wait-event --timeout=5.0 $id clean; done
'

test_expect_success 'queue is idle after small concurrent batch' '
	flux queue idle --timeout=10s
'

test_expect_success 'run multiple concurrent jobs that exceed resources' '
	flux submit --cc=1-10 hostname >job.ids &&
	for id in $(cat job.ids); do flux job wait-event --timeout=5.0 $id clean; done
'

test_expect_success 'allocated R contains execution section' '
	jobid=$(flux submit hostname) &&
	flux job wait-event $jobid alloc &&
	flux job info $jobid R | jq -e ".execution.R_lite"
'

test_expect_success 'reload with standard pool works again' '
	flux module reload sched-simple &&
	flux run hostname
'

test_done

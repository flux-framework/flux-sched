#!/bin/sh

test_description='Test Python scheduler hello callback with running jobs'

# Append --logfile option if FLUX_TESTS_LOGFILE is set in environment:
test -n "$FLUX_TESTS_LOGFILE" && set -- "$@" --logfile
. $(dirname $0)/sharness.sh

export TEST_UNDER_FLUX_CORES_PER_RANK=2
test_under_flux 4 job

test_expect_success 'reload sched-simple with fluxion:rv1exec pool' '
	flux module reload sched-simple pool-class=fluxion:rv1exec
'

test_expect_success 'submit a long-running job' '
	jobid=$(flux submit --wait-event=alloc sleep 300) &&
	flux job wait-event --timeout=5.0 $jobid alloc
'

test_expect_success 'reload scheduler while job is running' '
	flux module reload sched-simple pool-class=fluxion:rv1exec
'

test_expect_success 'running job can still be found' '
	flux jobs $jobid
'

test_expect_success 'can schedule new job while first is running' '
	flux run hostname
'

test_expect_success 'cancel the long-running job' '
	flux cancel $jobid &&
	flux job wait-event --timeout=5.0 $jobid clean
'

test_expect_success 'can run job after cancellation' '
	flux run hostname
'

test_expect_success 'submit job that uses all resources' '
	jobid=$(flux submit --wait-event=alloc -N4 sleep 300) &&
	flux job wait-event --timeout=5.0 $jobid alloc
'

test_expect_success 'reload scheduler with all resources allocated' '
	flux module reload sched-simple pool-class=fluxion:rv1exec
'

test_expect_success 'new job should queue (not enough resources)' '
	jobid2=$(flux submit hostname) &&
	test_expect_code 1 flux job wait-event --timeout=1.0 $jobid2 alloc
'

test_expect_success 'cancel first job to free resources' '
	flux cancel $jobid &&
	flux job wait-event --timeout=5.0 $jobid clean
'

test_expect_success 'queued job should now run' '
	flux job wait-event --timeout=5.0 $jobid2 alloc &&
	flux job wait-event --timeout=5.0 $jobid2 clean
'

###
# Repeat with JGF pool
###

test_expect_success 'unload sched-simple for JGF reconfiguration' '
	flux module remove sched-simple
'

test_expect_success 'generate JGF from current R' '
	flux kvs get resource.R | flux ion-R encode >R_jgf.json
'

test_expect_success 'reload resource with JGF-enabled R' '
	flux resource reload R_jgf.json
'

test_expect_success 'load sched-simple with fluxion:jgf pool' '
	flux module load sched-simple pool-class=fluxion:jgf
'

test_expect_success 'submit a long-running job (JGF)' '
	jobid=$(flux submit --wait-event=alloc sleep 300) &&
	flux job wait-event --timeout=5.0 $jobid alloc
'

test_expect_success 'reload scheduler while job is running (JGF)' '
	flux module reload sched-simple pool-class=fluxion:jgf
'

test_expect_success 'running job can still be found (JGF)' '
	flux jobs $jobid
'

test_expect_success 'can schedule new job while first is running (JGF)' '
	flux run hostname
'

test_expect_success 'cancel the long-running job (JGF)' '
	flux cancel $jobid &&
	flux job wait-event --timeout=5.0 $jobid clean
'

test_expect_success 'can run job after cancellation (JGF)' '
	flux run hostname
'

test_expect_success 'submit job that uses all resources (JGF)' '
	jobid=$(flux submit --wait-event=alloc -N4 sleep 300) &&
	flux job wait-event --timeout=5.0 $jobid alloc
'

test_expect_success 'reload scheduler with all resources allocated (JGF)' '
	flux module reload sched-simple pool-class=fluxion:jgf
'

test_expect_success 'new job should queue (not enough resources) (JGF)' '
	jobid2=$(flux submit hostname) &&
	test_expect_code 1 flux job wait-event --timeout=1.0 $jobid2 alloc
'

test_expect_success 'cancel first job to free resources (JGF)' '
	flux cancel $jobid &&
	flux job wait-event --timeout=5.0 $jobid clean
'

test_expect_success 'queued job should now run (JGF)' '
	flux job wait-event --timeout=5.0 $jobid2 alloc &&
	flux job wait-event --timeout=5.0 $jobid2 clean
'

test_expect_success 'reload with standard pool' '
	flux module reload sched-simple
'

test_done

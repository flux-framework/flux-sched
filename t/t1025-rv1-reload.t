#!/bin/sh
#

test_description='Check that fluxion (rv1_nosched) does not kill jobs on reload'

. `dirname $0`/sharness.sh

export FLUX_SCHED_MODULE=none
test_under_flux 1

test_expect_success 'configure fluxion with rv1_nosched' '
	cat >config.toml <<-EOT &&
	[sched-fluxion-resource]
	match-format = "rv1_nosched"
	EOT
	flux config load config.toml
'
test_expect_success 'add testqueue property to rank 0 (for later)' '
	flux resource R | flux R set-property testqueue:0 >R &&
	flux resource reload R
'
# N.B. double booked jobs get a fatal exception on alloc
test_expect_success 'prepare to detect double booked resources' '
	flux jobtap load alloc-check.so
'
test_expect_success 'load fluxion modules' '
	load_resource &&
	load_qmanager_sync
'
#
# Ensure jobs keep running across scheduler reload (no queues)
#
test_expect_success 'submit a sleep inf job and wait for alloc' '
	flux submit -n1 --flags=debug --wait-event=alloc sleep inf >job.id
'
test_expect_success 'reload fluxion modules' '
	remove_qmanager &&
	reload_resource &&
	load_qmanager
'
test_expect_success 'the job is still running' '
	state=$(flux jobs -n -o {state} $(cat <job.id)) &&
	test $state = RUN
'
test_expect_success 'run a pile of jobs to check for double booking' '
	flux submit --wait --cc=1-32 -n1 true
'
test_expect_success 'cancel the original job and wait for it to be inactive' '
	flux cancel $(cat <job.id) &&
	flux job wait-event $(cat <job.id) clean
'
#
# Ensure jobs keep running across scheduler reload (with one queue)
#
test_expect_success 'configure testqueue' '
	cat >config2.toml <<-EOT &&
	[sched-fluxion-resource]
	match-format = "rv1_nosched"
	[queues.testqueue]
	requires = ["testqueue"]
	EOT
	flux config load config2.toml
'
test_expect_success 'reload fluxion modules to get new queue config' '
	remove_qmanager &&
	reload_resource &&
	load_qmanager
'
test_expect_success 'start testqueue' '
	flux queue start -q testqueue
'
test_expect_success 'submit a sleep inf job to testqueue and wait for alloc' '
	flux submit -vv --wait-event=alloc -n1 -q testqueue sleep inf >job2.id
'
test_expect_success 'reload fluxion modules' '
	remove_qmanager &&
	reload_resource &&
	load_qmanager
'
test_expect_success 'the job is still running' '
	state=$(flux jobs -n -o {state} $(cat <job2.id)) &&
	test $state = RUN
'
test_expect_success 'run a pile of jobs to check for double booking' '
	flux submit --wait --cc=1-32 -n1 -q testqueue true
'
#
# A running job that was submitted to testqueue should get a fatal
# exception when scheduler is reloaded with no queues configured.
#
# N.B. alloc-check won't accept a scheduler-restart exception in lieu of free
# event so unload it to avoid false positives (flux-framework/flux-core#5889)
test_expect_success 'unload alloc-check' '
	flux jobtap remove alloc-check.so
'
test_expect_success 'configure without queues' '
	flux config load config.toml
'
test_expect_success 'reload fluxion modules' '
	remove_qmanager &&
	reload_resource &&
	load_qmanager
'
test_expect_success 'running job received a fatal exception' '
	flux job wait-event -v -t 10s $(cat <job2.id) exception
'
test_expect_success 'wait for job to clean up' '
	flux job wait-event -v -t 10s $(cat <job2.id) clean
'
test_expect_success 'submit a sleep inf job to anon queue and wait for alloc' '
	flux submit -vv --wait-event=alloc -n1 sleep inf >job3.id
'
#
# A running job that was submitted to the anon queue should get a fatal
# exception when scheduler is reloaded with testqueue configured.
#
test_expect_success 'configure with testqueue' '
	flux config load config2.toml
'
test_expect_success 'reload fluxion modules' '
	remove_qmanager &&
	reload_resource &&
	load_qmanager
'
test_expect_success 'running job received a fatal exception' '
	flux job wait-event -v -t 10s $(cat <job3.id) exception
'
test_expect_success 'wait for job to clean up' '
	flux job wait-event -v -t 10s $(cat <job3.id) clean
'
test_expect_success 'clean up' '
	cleanup_active_jobs
'
test_expect_success 'remove fluxion modules' '
	remove_qmanager &&
	remove_resource
'

test_done

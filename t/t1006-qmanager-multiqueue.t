#!/bin/sh

test_description='Test multiple queue support within qmanager'

. `dirname $0`/sharness.sh

mkdir -p config

export FLUX_SCHED_MODULE=none
test_under_flux 1 full -o,--config-path=$(pwd)/config

test_expect_success 'qmanager: load resource' '
	load_resource prune-filters=ALL:core subsystems=containment policy=low
'

test_expect_success 'qmanager: loading qmanager with multiple queues' '
	cat >config/queues.toml <<-EOT &&
	[queues.all]
	[queues.batch]
	[queues.debug]

	[policy.jobspec.defaults.system]
	queue = "all"
	EOT
	flux config reload &&
	flux queue start --all &&
	load_qmanager
'

get_policy() {
	jq -e -r .params.$1.\"queue-policy\"
}

test_expect_success 'qmanager: all,batch,debug are fcfs queues' '
	test $(flux qmanager-params | get_policy all) = "fcfs" &&
	test $(flux qmanager-params | get_policy batch) = "fcfs" &&
	test $(flux qmanager-params | get_policy debug) = "fcfs"
'

test_expect_success 'qmanager: job can be submitted to queue=all' '
	flux run -n1 --queue=all true
'

test_expect_success 'qmanager: job can be submitted to queue=batch' '
	flux run -n1 --queue=batch true
'

test_expect_success 'qmanager: job can be submitted to queue=debug' '
	flux run -n1 --queue=debug true
'

test_expect_success 'qmanager: job enqueued into default queue' '
	flux run -n1 true
'

test_expect_success 'reconfigure qmanager with queues with different policies' '
	cat >config/queues.toml <<-EOT &&
	[queues.queue1]
	[queues.queue2]
	[queues.queue3]

	[policy.jobspec.defaults.system]
	queue = "queue3"

	[sched-fluxion-qmanager]
	queue-policy-per-queue = "queue1:easy queue2:hybrid queue3:fcfs"
	EOT
	flux config reload &&
	flux queue start --all &&
	reload_qmanager
'

test_expect_success 'qmanager: queues have expected policies' '
	test $(flux qmanager-params | get_policy queue1) = "easy" &&
	test $(flux qmanager-params | get_policy queue2) = "hybrid" &&
	test $(flux qmanager-params | get_policy queue3) = "fcfs"
'

test_expect_success 'qmanager: job can be submitted to queue=queue3 (fcfs)' '
	flux run -n1 --queue=queue3 true
'

test_expect_success 'qmanager: job can be submitted to queue=queue2 (hybrid)' '
	flux run -n1 --queue=queue2 true
'

test_expect_success 'qmanager: job submitted to queue=queue1 (conservative)' '
	flux run -n1 --queue=queue1 true
'

test_expect_success 'qmanager: job enqueued into default queue' '
	flux run -n1 true
'

test_expect_success 'qmanager: job is denied when submitted to unknown queue' '
	test_must_fail flux run -n 1 --queue=foo \
	    hostname 2>unknown.err &&
	grep "Invalid queue" unknown.err
'

test_expect_success 'qmanager: incorrect queue policy can be caught' '
	flux dmesg -C &&
	cat >config/queues.toml <<-EOT &&
	[queues.queue1]
	[queues.queue2]
	[queues.queue3]

	[sched-fluxion-qmanager]
	queue-policy-per-queue = "queue1:easy queue2:foo queue3:fcfs"
	EOT
	flux config reload &&
	flux queue start --all &&
	reload_qmanager &&
	flux dmesg | grep "Unknown queuing policy"
'
test_expect_success 'qmanager: whee, what happened' '
	flux dmesg &&
	flux qmanager-params
'

test_expect_success 'qmanager: fcfs was used instead of unknown policy' '
	test $(flux qmanager-params | get_policy queue1) = "easy" &&
	test $(flux qmanager-params | get_policy queue2) = "fcfs" &&
	test $(flux qmanager-params | get_policy queue3) = "fcfs"
'

test_expect_success 'unload qmanager and deconfigure queues' '
	remove_qmanager &&
	cp /dev/null config/queues.toml &&
	flux config reload
'
test_expect_success 'submit job with no queue' '
	flux submit /bin/true >noqueue.jobid
'
test_expect_success 'reconfigure with one queue and load qmanager' '
	cat >config/queues.toml <<-EOT &&
	[queues.foo]
	EOT
	flux config reload &&
	flux queue start --all &&
	load_qmanager
'
test_expect_success 'job submitted with no queue gets fatal exception' '
	test_must_fail flux job attach $(cat noqueue.jobid) 2>noqueue.err &&
	grep "job.exception" noqueue.err | grep "type=alloc severity=0"
'
test_expect_success 'unload qmanager' '
	remove_qmanager
'
test_expect_success 'submit job with queue' '
	flux submit --queue=foo /bin/true >withqueue.jobid
'
test_expect_success 'deconfigure queues and load qmanager' '
	cp /dev/null config/queues.toml &&
	flux config reload &&
	load_qmanager
'
test_expect_success 'job submitted with queue gets fatal exception' '
	test_must_fail flux job attach $(cat withqueue.jobid) 2>foo.err &&
	grep "job.exception" foo.err | grep "type=alloc severity=0 queue"
'

test_expect_success 'cleanup active jobs' '
	cleanup_active_jobs
'

test_expect_success 'removing resource and qmanager modules' '
	remove_qmanager &&
	remove_resource
'

test_done


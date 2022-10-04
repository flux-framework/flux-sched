#!/bin/sh

test_description='Test multiple queue support within qmanager'

. `dirname $0`/sharness.sh

mkdir -p config

export FLUX_SCHED_MODULE=none
test_under_flux 1 full -o,--config-path=$(pwd)/config

get_queue() {
	queue=$1 &&
	jobid=$(flux job id $2) &&
	flux dmesg | grep ${queue} | grep ${jobid} | awk '{print $5}' \
	    | awk -F= '{print $2}'
}

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
	load_qmanager
'

test_expect_success 'qmanager: job can be submitted to queue=all' '
	jobid=$(flux mini submit -n 1 --queue=all hostname) &&
	flux job wait-event -t 10 ${jobid} finish &&
	queue=$(get_queue alloc ${jobid}) &&
	test ${queue} = all &&
	queue=$(get_queue free ${jobid}) &&
	test ${queue} = all &&
	flux dmesg -C
'

test_expect_success 'qmanager: job can be submitted to queue=batch' '
	jobid=$(flux mini submit -n 1 --queue=batch hostname) &&
	flux job wait-event -t 10 ${jobid} finish &&
	queue=$(get_queue alloc ${jobid}) &&
	test ${queue} = batch &&
	queue=$(get_queue free ${jobid}) &&
	test ${queue} = batch &&
	flux dmesg -C
'

test_expect_success 'qmanager: job can be submitted to queue=debug' '
	jobid=$(flux mini submit -n 1 --queue=debug hostname) &&
	flux job wait-event -t 10 ${jobid} finish &&
	queue=$(get_queue alloc ${jobid}) &&
	test ${queue} = debug &&
	queue=$(get_queue free ${jobid}) &&
	test ${queue} = debug &&
	flux dmesg -C
'

test_expect_success 'qmanager: job enqueued into implicitly default queue' '
	jobid=$(flux mini submit -n 1 hostname) &&
	flux job wait-event -t 10 ${jobid} finish &&
	queue=$(get_queue alloc ${jobid}) &&
	test ${queue} = all &&
	queue=$(get_queue free ${jobid}) &&
	test ${queue} = all &&
	flux dmesg -C
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
	reload_qmanager
'

test_expect_success 'qmanager: job can be submitted to queue=queue3 (fcfs)' '
	jobid=$(flux mini submit -n 1 --queue=queue3 hostname) &&
	flux job wait-event -t 10 ${jobid} finish &&
	queue=$(get_queue alloc ${jobid}) &&
	test ${queue} = queue3 &&
	queue=$(get_queue free ${jobid}) &&
	test ${queue} = queue3 &&
	flux dmesg -C
'

test_expect_success 'qmanager: job can be submitted to queue=queue2 (hybrid)' '
	jobid=$(flux mini submit -n 1 --queue=queue2 hostname) &&
	flux job wait-event -t 10 ${jobid} finish &&
	queue=$(get_queue alloc ${jobid}) &&
	test ${queue} = queue2 &&
	queue=$(get_queue free ${jobid}) &&
	test ${queue} = queue2 &&
	flux dmesg -C
'

test_expect_success 'qmanager: job submitted to queue=queue1 (conservative)' '
	jobid=$(flux mini submit -n 1 --queue=queue1 hostname) &&
	flux job wait-event -t 10 ${jobid} finish &&
	queue=$(get_queue alloc ${jobid}) &&
	test ${queue} = queue1 &&
	queue=$(get_queue free ${jobid}) &&
	test ${queue} = queue1 &&
	flux dmesg -C
'

test_expect_success 'qmanager: job enqueued into explicitly default queue' '
	jobid=$(flux mini submit -n 1 hostname) &&
	flux job wait-event -t 10 ${jobid} finish &&
	queue=$(get_queue alloc ${jobid}) &&
	test ${queue} = queue3 &&
	queue=$(get_queue free ${jobid}) &&
	test ${queue} = queue3 &&
	flux dmesg -C
'

test_expect_success 'qmanager: job is denied when submitted to unknown queue' '
	test_must_fail flux mini run -n 1 --queue=foo \
	    hostname 2>unknown.err &&
	grep "Invalid queue" unknown.err
'

test_expect_success 'qmanager: incorrect queue policy can be caught' '
	cat >config/queues.toml <<-EOT &&
	[queues.queue1]
	[queues.queue2]
	[queues.queue3]

	[sched-fluxion-qmanager]
	queue-policy-per-queue = "queue1:easy queue2:foo queue3:fcfs"
	EOT
	flux config reload &&
	reload_qmanager &&
	flux dmesg | grep "Unknown queuing policy"
'

test_expect_success 'unload qmanager and deconfigure queues' '
	remove_qmanager &&
	cp /dev/null config/queues.toml &&
	flux config reload
'
test_expect_success 'submit job with no queue' '
	flux mini submit /bin/true >noqueue.jobid
'
test_expect_success 'reconfigure with one queue and load qmanager' '
	cat >config/queues.toml <<-EOT &&
	[queues.foo]
	EOT
	flux config reload &&
	load_qmanager
'
test_expect_success 'job submitted with no queue gets fatal exception' '
	test_must_fail flux job attach $(cat noqueue.jobid) 2>noqueue.err &&
	grep "job.exception type=alloc severity=0" noqueue.err
'
test_expect_success 'unload qmanager' '
	remove_qmanager
'
test_expect_success 'submit job with queue' '
	flux mini submit --queue=foo /bin/true >withqueue.jobid
'
test_expect_success 'deconfigure queues and load qmanager' '
	cp /dev/null config/queues.toml &&
	flux config reload &&
	load_qmanager
'
test_expect_success 'job submitted with queue gets fatal exception' '
	test_must_fail flux job attach $(cat withqueue.jobid) 2>foo.err &&
	grep "job.exception type=alloc severity=0 queue" foo.err
'

test_expect_success 'cleanup active jobs' '
	cleanup_active_jobs
'

test_expect_success 'removing resource and qmanager modules' '
	remove_qmanager &&
	remove_resource
'

test_done


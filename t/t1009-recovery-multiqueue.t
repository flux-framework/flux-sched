#!/bin/sh

test_description='Test the full state recovery of qmanager for multiple queues'

. `dirname $0`/sharness.sh

mkdir -p config

hwloc_basepath=`readlink -f ${SHARNESS_TEST_SRCDIR}/data/hwloc-data`
# 1 brokers, each (exclusively) have: 1 node, 2 sockets, 16 cores (8 per socket)
excl_1N1B="${hwloc_basepath}/001N/exclusive/01-brokers"

export FLUX_SCHED_MODULE=none
test_under_flux 1 full -o,--config-path=$(pwd)/config

check_requeue() {
	local jobid=$(flux job id ${1})
	local correct_queue=${2}

	flux ion-resource info ${jobid} | grep "ALLOCATED"
	if [ $? -ne 0 ]
	then
	    return $?
	fi
	local queue=$(flux dmesg | grep requeue | grep ${jobid} |\
	    awk "{print \$5}" | awk -F= "{print \$2}")
	test ${queue} = ${correct_queue}
}

test_expect_success 'recovery: generate test jobspecs' '
	flux run --dry-run -N 1 -n 8 -t 1h \
	    --queue=batch sleep 3600 > basic.batch.json &&
	flux run --dry-run -N 1 -n 8 -t 1h \
	    --queue=debug sleep 3600 > basic.debug.json
'

test_expect_success 'load test resources' '
	load_test_resources ${excl_1N1B}
'

test_expect_success 'recovery: loading fluxion resource module' '
	load_resource match-format=rv1 policy=high
'

test_expect_success 'qmanager: configure qmanager with two queues' '
	cat >config/queues.toml <<-EOT &&
	[queues.batch]
	[queues.debug]

	[policy.jobspec.defaults.system]
	queue = "batch"
	EOT
	flux config reload &&
	flux queue start --all &&
	load_qmanager
'

# jobid1 - 2 will be scheduled; jobid 3 - 4 pending
test_expect_success 'recovery: submit to occupy resources fully (rv1)' '
	jobid1=$(flux job submit basic.batch.json) &&
	jobid2=$(flux job submit basic.debug.json) &&
	jobid3=$(flux job submit basic.batch.json) &&
	jobid4=$(flux job submit basic.debug.json) &&
	flux job wait-event -t 10 ${jobid2} start &&
	flux job wait-event -t 10 ${jobid4} submit
'

test_expect_success 'recovery: works when both modules restart (rv1)' '
	flux dmesg -C &&
	remove_qmanager &&
	reload_resource match-format=rv1 policy=high &&
	load_qmanager_sync &&
	check_requeue ${jobid1} batch &&
	check_requeue ${jobid2} debug &&
	test_must_fail flux job wait-event -t 0.5 ${jobid3} start &&
	test_expect_code 3 flux ion-resource info ${jobid3}
'

test_expect_success 'recovery: a cancel leads to a job schedule (rv1)' '
	flux cancel ${jobid2} &&
	(flux job wait-event -t 10 ${jobid3} start ||
	 flux job wait-event -t 10 ${jobid4} start)
'

test_expect_success 'recovery: cancel all jobs (rv1_nosched)' '
	flux cancel ${jobid1} &&
	flux cancel ${jobid3} &&
	flux cancel ${jobid4} &&
	flux job wait-event -t 10 ${jobid4} release
'

test_expect_success 'cleanup active jobs' '
	cleanup_active_jobs
'

test_expect_success 'removing resource and qmanager modules' '
	remove_qmanager &&
	remove_resource
'

test_done

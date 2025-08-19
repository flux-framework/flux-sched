#!/bin/sh

test_description='Test qmanager service in simulated mode'

. `dirname $0`/sharness.sh

hwloc_basepath=`readlink -f ${SHARNESS_TEST_SRCDIR}/data/hwloc-data`
# 1 brokers, each (exclusively) have: 1 node, 2 sockets, 16 cores (8 per socket)
excl_1N1B="${hwloc_basepath}/001N/exclusive/01-brokers"

export FLUX_SCHED_MODULE=none
test_under_flux 1

job_kvsdir()    { flux job id --to=kvs $1; }
exec_eventlog() { flux kvs get -r $(job_kvsdir $1).guest.exec.eventlog; }
exec_test()     { ${jq} '.attributes.system.exec.test = {}'; }
exec_testattr() {
    ${jq} --arg key "$1" --arg value $2 \
        '.attributes.system.exec.test[$key] = $value'
}

test_expect_success 'qmanager: generate jobspec for a simple test job' '
    flux submit -n1 -t 100s --dry-run hostname > basic.json
'

test_expect_success 'load test resources' '
    load_test_resources ${excl_1N1B}
'

test_expect_success 'qmanager: loading resource and qmanager modules works' '
    flux module load sched-fluxion-resource prune-filters=ALL:core \
subsystems=containment policy=low &&
    load_qmanager
'

test_expect_success 'qmanager: basic job runs in simulated mode' '
    jobid=$(flux job submit basic.json) &&
    flux job wait-event -t 10 ${jobid} start &&
    flux job wait-event -t 10 ${jobid} finish &&
    flux job wait-event -t 10 ${jobid} release &&
    flux job wait-event -t 10 ${jobid} clean
'

test_expect_success 'qmanager: canceling job during execution works' '
    jobid=$(flux run --dry-run -t 100m hostname | \
        exec_test | flux job submit) &&
    flux job wait-event -vt 10 ${jobid} start &&
    flux cancel ${jobid} &&
    flux job wait-event -t 10 ${jobid} exception &&
    flux job wait-event -t 10 ${jobid} finish | grep status=15 &&
    flux job wait-event -t 10 ${jobid} release &&
    flux job wait-event -t 10 ${jobid} clean &&
    exec_eventlog $jobid | grep "complete" | grep "\"status\":15"
'

test_expect_success 'qmanager: exception during initialization is supported' '
    flux run --dry-run hostname | \
      exec_testattr mock_exception init > ex1.json &&
    jobid=$(flux job submit ex1.json) &&
    flux job wait-event -t 10 ${jobid} exception > exception.1.out &&
    test_debug "flux job eventlog ${jobid}" &&
    grep "type=\"exec\"" exception.1.out &&
    grep "mock initialization exception generated" exception.1.out &&
    test_must_fail flux job wait-event -qt 10 ${jobid} finish
'

test_expect_success 'qmanager: exception during run is supported' '
	flux run --dry-run hostname | \
	  exec_testattr mock_exception run > ex2.json &&
	jobid=$(flux job submit ex2.json) &&
	flux job wait-event -t 10 ${jobid} exception > exception.2.out &&
	grep "type=\"exec\"" exception.2.out &&
	grep "mock run exception generated" exception.2.out &&
	flux job wait-event -qt 10 ${jobid} clean &&
	flux job eventlog ${jobid} > eventlog.${jobid}.out &&
	grep "finish status=15" eventlog.${jobid}.out
'

test_expect_success 'qmanager: unsatisfiable jobspec rejected' '
    jobid=$(flux run --dry-run -N 64 -n 64 hostname | \
        exec_test | flux job submit) &&
    flux job wait-event -t 10 ${jobid} clean &&
    flux job wait-event -t 10 ${jobid} exception | grep "unsatisfiable"
'

test_expect_success 'qmanager: check stats' '
    flux module stats sched-fluxion-qmanager | tee stats.json &&
    jq -e ".queues.default.action_counts.rejected == 1" stats.json &&
    jq -e ".queues.default.action_counts.complete >= 4" stats.json
'

test_expect_success 'cleanup active jobs' '
    cleanup_active_jobs
'

test_expect_success 'removing resource and qmanager modules' '
    remove_qmanager &&
    remove_resource
'

test_done

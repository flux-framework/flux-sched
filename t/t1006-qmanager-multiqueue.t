#!/bin/sh

test_description='Test multiple queue support within qmanager'

ORIG_HOME=${HOME}

. `dirname $0`/sharness.sh

export FLUX_SCHED_MODULE=none
test_under_flux 1

#
# sharness modifies $HOME environment variable, but this interferes
# with python's package search path, in particular its user site package.
#
HOME=${ORIG_HOME}

conf_base=${SHARNESS_TEST_SRCDIR}/conf.d

get_queue() {
    queue=$1 &&
    jobid=$(flux job id $2) &&
    flux dmesg | grep ${queue} | grep ${jobid} | awk '{print $5}' \
        | awk -F= '{print $2}'
}

test_expect_success 'qmanager: loading qmanager with multiple queues' '
    load_resource prune-filters=ALL:core \
subsystems=containment policy=low &&
    load_qmanager "queues=all batch debug"
'

test_expect_success 'qmanager: job can be submitted to queue=all' '
    jobid=$(flux mini submit -n 1 --setattr system.queue=all hostname) &&
    flux job wait-event -t 10 ${jobid} finish &&
    queue=$(get_queue alloc ${jobid}) &&
    test ${queue} = all &&
    queue=$(get_queue free ${jobid}) &&
    test ${queue} = all &&
    flux dmesg -C
'

test_expect_success 'qmanager: job can be submitted to queue=batch' '
    jobid=$(flux mini submit -n 1 --setattr system.queue=batch hostname) &&
    flux job wait-event -t 10 ${jobid} finish &&
    queue=$(get_queue alloc ${jobid}) &&
    test ${queue} = batch &&
    queue=$(get_queue free ${jobid}) &&
    test ${queue} = batch &&
    flux dmesg -C
'

test_expect_success 'qmanager: job can be submitted to queue=debug' '
    jobid=$(flux mini submit -n 1 --setattr system.queue=debug hostname) &&
    flux job wait-event -t 10 ${jobid} finish &&
    queue=$(get_queue alloc ${jobid}) &&
    test ${queue} = debug &&
    queue=$(get_queue free ${jobid}) &&
    test ${queue} = debug &&
    flux dmesg -C
'

# when default-queue=queue-name is not given the lexicographically
# earliest queue name becomes default
test_expect_success 'qmanager: job enqueued into implicitly default queue' '
    jobid=$(flux mini submit -n 1 hostname) &&
    flux job wait-event -t 10 ${jobid} finish &&
    queue=$(get_queue alloc ${jobid}) &&
    test ${queue} = all &&
    queue=$(get_queue free ${jobid}) &&
    test ${queue} = all &&
    flux dmesg -C
'

test_expect_success 'qmanager: qmanager with queues with different policies' '
    flux module reload -f sched-fluxion-qmanager queues="queue1 queue2 queue3" \
queue-policy-per-queue="queue1:easy queue2:hybrid queue3:fcfs" default-queue=queue3
'

test_expect_success 'qmanager: job can be submitted to queue=queue3 (fcfs)' '
    jobid=$(flux mini submit -n 1 --setattr system.queue=queue3 hostname) &&
    flux job wait-event -t 10 ${jobid} finish &&
    queue=$(get_queue alloc ${jobid}) &&
    test ${queue} = queue3 &&
    queue=$(get_queue free ${jobid}) &&
    test ${queue} = queue3 &&
    flux dmesg -C
'

test_expect_success 'qmanager: job can be submitted to queue=queue2 (hybrid)' '
    jobid=$(flux mini submit -n 1 --setattr system.queue=queue2 hostname) &&
    flux job wait-event -t 10 ${jobid} finish &&
    queue=$(get_queue alloc ${jobid}) &&
    test ${queue} = queue2 &&
    queue=$(get_queue free ${jobid}) &&
    test ${queue} = queue2 &&
    flux dmesg -C
'

test_expect_success 'qmanager: job submitted to queue=queue1 (conservative)' '
    jobid=$(flux mini submit -n 1 --setattr system.queue=queue1 hostname) &&
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
    test_must_fail flux mini run -n 1 --setattr system.queue=foo hostname &&
    flux dmesg -C
'

test_expect_success 'qmanager: incorrect default-queue name can be caught' '
    flux module reload -f sched-fluxion-qmanager queues="queue1 queue2 queue3" \
default-queue=foo &&
    flux dmesg | grep "Unknown default queue (foo)"
'

test_expect_success 'qmanager: incorrect queue-policy-per-queue can be caught' '
    flux module reload -f sched-fluxion-qmanager queues="queue1 queue2 queue3" \
queue-policy-per-queue="queue1:easy queue2:foo queue3:fcfs" &&
    flux dmesg | grep "Unknown queuing policy"
'

test_expect_success 'cleanup active jobs' '
    cleanup_active_jobs
'

test_expect_success 'removing resource and qmanager modules' '
    remove_qmanager &&
    remove_resource
'

test_done


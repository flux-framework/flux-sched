#!/bin/sh

test_description='Test job annotation

qmanager annotates jobs with a sched.t_estimate (time in seconds since
epoch when job will start).  The annotation is provided by qmanager to
the job manager in streaming responses to the sched.alloc RPC.

After each scheduling loop, if the start estimate has changed since the
last estimate, an alloc response is sent with the new value.  Upon resource
allocation, the final alloc response is sent with the estimate set to JSON
null (which means delete key).

Note that annotation updates are not posted to the KVS eventlogs, but are
recorded in the job-manager and job-list modules, and may be accessed with
`flux job list-ids` and `flux jobs`, respectively.

See RFC 27 for more details on alloc response and annotations.
'

. `dirname $0`/sharness.sh

hwloc_basepath=`readlink -e ${SHARNESS_TEST_SRCDIR}/data/hwloc-data`
# 1 brokers, each (exclusively) have: 1 node, 2 sockets, 16 cores (8 per socket)
excl_1N1B="${hwloc_basepath}/001N/exclusive/01-brokers"

export FLUX_SCHED_MODULE=none
test_under_flux 1

has_annotation() {
    flux job list-ids $1 | jq -e ".annotations.sched.t_estimate > 0"
}
hasnt_annotation() {
    test_must_fail has_annotation $1
}

print_t_estimate() {
    flux jobs -o '{sched.t_estimate!D:>10h}'
}

test_expect_success 'load test resources' '
    load_test_resources ${excl_1N1B}
'

test_expect_success 'annotation: loading qmanager (queue-policy=easy)' '
    load_resource prune-filters=ALL:core \
subsystems=containment policy=low &&
    load_qmanager queue-policy=easy
'

test_expect_success 'annotation: works with EASY policy' '
    jobid1=$(flux submit -n 8 -t 360s sleep 300) &&
    jobid2=$(flux submit -n 16 -t 360s sleep 300) && # reserved
    jobid3=$(flux submit -n 16 -t 360s sleep 300) && # skipped
    jobid4=$(flux submit -n 16 -t 360s sleep 300) && # skipped
    jobid5=$(flux submit -n 2 -t 180s sleep 100) &&

    flux job wait-event -t 10 ${jobid5} start &&
    hasnt_annotation ${jobid1} &&
    has_annotation ${jobid2} &&
    hasnt_annotation ${jobid3} &&
    hasnt_annotation ${jobid4} &&
    hasnt_annotation ${jobid5} &&
    print_t_estimate
'

test_expect_success 'annotation: cancel all active jobs 1' '
    active_jobs=$(flux job list --state=active | jq .id) &&
    for job in ${active_jobs}; do flux cancel ${job}; done &&
    for job in ${active_jobs}; do flux job wait-event -t 10 ${job} clean; done
'

test_expect_success 'annotation: loading qmanager (queue-policy=hybrid)' '
    remove_qmanager &&
    reload_resource prune-filters=ALL:core \
subsystems=containment policy=low load-allowlist=cluster,node,core &&
    load_qmanager queue-policy=hybrid policy-params=reservation-depth=2
'

test_expect_success 'annotation: works with HYBRID policy' '
    jobid1=$(flux submit -n 8 -t 360s sleep 300) &&
    jobid2=$(flux submit -n 16 -t 360s sleep 300) && # reserved
    jobid3=$(flux submit -n 16 -t 360s sleep 300) && # reserved
    jobid4=$(flux submit -n 16 -t 360s sleep 300) && # skipped
    jobid5=$(flux submit -n 2 -t 180s sleep 100) &&

    flux job wait-event -t 10 ${jobid5} start &&
    hasnt_annotation ${jobid1} &&
    has_annotation ${jobid2} &&
    has_annotation ${jobid3} &&
    hasnt_annotation ${jobid4} &&
    hasnt_annotation ${jobid5}
'

test_expect_success 'annotation: cancel all active jobs 2' '
    active_jobs=$(flux job list --state=active | jq .id) &&
    for job in ${active_jobs}; do flux cancel ${job}; done &&
    for job in ${active_jobs}; do flux job wait-event -t 10 ${job} clean; done
'

test_expect_success 'annotation: loading qmanager (queue-policy=conservative)' '
    remove_qmanager &&
    reload_resource prune-filters=ALL:core \
subsystems=containment policy=low load-allowlist=cluster,node,core &&
    load_qmanager queue-policy=conservative
'

test_expect_success 'annotation: works with CONSERVATIVE policy' '
    jobid1=$(flux submit -n 8 -t 360s sleep 300) &&
    jobid2=$(flux submit -n 16 -t 360s sleep 300) && # reserved
    jobid3=$(flux submit -n 16 -t 360s sleep 300) && # reserved
    jobid4=$(flux submit -n 16 -t 360s sleep 300) && # reserved
    jobid5=$(flux submit -n 2 -t 180s sleep 100) &&

    flux job wait-event -t 10 ${jobid5} start &&
    hasnt_annotation ${jobid1} &&
    has_annotation ${jobid2} &&
    has_annotation ${jobid3} &&
    has_annotation ${jobid4} &&
    hasnt_annotation ${jobid5}
'

test_expect_success 'annotation: cancel all active jobs 3' '
    active_jobs=$(flux job list --state=active | jq .id) &&
    for job in ${active_jobs}; do flux cancel ${job}; done &&
    for job in ${active_jobs}; do flux job wait-event -t 10 ${job} clean; done
'

test_expect_success 'annotation: loading qmanager (queue-policy=fcfs)' '
    remove_qmanager &&
    reload_resource prune-filters=ALL:core \
subsystems=containment policy=low load-allowlist=cluster,node,core &&
    load_qmanager
'

test_expect_success 'annotation: works with FCFS policy' '
    jobid1=$(flux submit -n 8 -t 360s sleep 300) &&
    jobid2=$(flux submit -n 16 -t 360s sleep 300) && # block
    jobid3=$(flux submit -n 16 -t 360s sleep 300) &&
    jobid4=$(flux submit -n 16 -t 360s sleep 300) &&
    jobid5=$(flux submit -n 2 -t 180s sleep 100) &&

    flux job wait-event -t 10 ${jobid1} start &&
    flux job wait-event -t 10 ${jobid5} submit &&
    hasnt_annotation ${jobid1} &&
    hasnt_annotation ${jobid2} &&
    hasnt_annotation ${jobid3} &&
    hasnt_annotation ${jobid4} &&
    hasnt_annotation ${jobid5} &&
    active_jobs=$(flux job list --state=active | jq .id) &&
    for job in ${active_jobs}; do flux cancel ${job}; done &&
    for job in ${active_jobs}; do flux job wait-event -t 10 ${job} clean; done
'

test_expect_success 'cleanup active jobs' '
    cleanup_active_jobs
'

test_expect_success 'removing resource and qmanager modules' '
    remove_qmanager &&
    remove_resource
'

test_done

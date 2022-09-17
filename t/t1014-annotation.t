#!/bin/sh

test_description='Test job annotation'

. `dirname $0`/sharness.sh

hwloc_basepath=`readlink -e ${SHARNESS_TEST_SRCDIR}/data/hwloc-data`
# 1 brokers, each (exclusively) have: 1 node, 2 sockets, 16 cores (8 per socket)
excl_1N1B="${hwloc_basepath}/001N/exclusive/01-brokers"

skip_all_unless_have jq

export FLUX_SCHED_MODULE=none
test_under_flux 1

nonexistent_annotation(){
    jobid=$(flux job id ${1}) &&
    ann=$(flux job list -A | grep ${jobid} | jq 'has("annotations")') &&
    test "${ann}" = "false"
}

validate_sched_annotation(){
    jobid=$(flux job id ${1}) &&
    start_time_is_zero=${2} &&
    ann=$(flux job list -A | grep ${jobid} | jq -c '.annotations') &&
    t_est=$(echo ${ann} | jq '.sched.t_estimate') &&
    if test x"${start_time_is_zero}" = x"TRUE";
    then
        test "${t_est}" = "null"
    else
        test "${t_est}" != "0"
    fi
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
    jobid1=$(flux mini submit -n 8 -t 360s sleep 300) &&
    jobid2=$(flux mini submit -n 16 -t 360s sleep 300) && # reserved
    jobid3=$(flux mini submit -n 16 -t 360s sleep 300) && # skipped
    jobid4=$(flux mini submit -n 16 -t 360s sleep 300) && # skipped
    jobid5=$(flux mini submit -n 2 -t 180s sleep 100) &&

    flux job wait-event -t 10 ${jobid5} start &&
    validate_sched_annotation ${jobid1} TRUE &&
    validate_sched_annotation ${jobid2} FALSE &&
    nonexistent_annotation ${jobid3} &&
    nonexistent_annotation ${jobid4} &&
    validate_sched_annotation ${jobid5} TRUE &&
    print_t_estimate
'

test_expect_success 'annotation: cancel all active jobs 1' '
    active_jobs=$(flux job list --state=active | jq .id) &&
    for job in ${active_jobs}; do flux job cancel ${job}; done &&
    for job in ${active_jobs}; do flux job wait-event -t 10 ${job} clean; done
'

test_expect_success 'annotation: loading qmanager (queue-policy=hybrid)' '
    remove_qmanager &&
    reload_resource prune-filters=ALL:core \
subsystems=containment policy=low load-allowlist=cluster,node,core &&
    load_qmanager queue-policy=hybrid policy-params=reservation-depth=2
'

test_expect_success 'annotation: works with HYBRID policy' '
    jobid1=$(flux mini submit -n 8 -t 360s sleep 300) &&
    jobid2=$(flux mini submit -n 16 -t 360s sleep 300) && # reserved
    jobid3=$(flux mini submit -n 16 -t 360s sleep 300) && # reserved
    jobid4=$(flux mini submit -n 16 -t 360s sleep 300) && # skipped
    jobid5=$(flux mini submit -n 2 -t 180s sleep 100) &&

    flux job wait-event -t 10 ${jobid5} start &&
    validate_sched_annotation ${jobid1} TRUE &&
    validate_sched_annotation ${jobid2} FALSE &&
    validate_sched_annotation ${jobid3} FALSE &&
    nonexistent_annotation ${jobid4} &&
    validate_sched_annotation ${jobid5} TRUE
'

test_expect_success 'annotation: cancel all active jobs 2' '
    active_jobs=$(flux job list --state=active | jq .id) &&
    for job in ${active_jobs}; do flux job cancel ${job}; done &&
    for job in ${active_jobs}; do flux job wait-event -t 10 ${job} clean; done
'

test_expect_success 'annotation: loading qmanager (queue-policy=conservative)' '
    remove_qmanager &&
    reload_resource prune-filters=ALL:core \
subsystems=containment policy=low load-allowlist=cluster,node,core &&
    load_qmanager queue-policy=conservative
'

test_expect_success 'annotation: works with CONSERVATIVE policy' '
    jobid1=$(flux mini submit -n 8 -t 360s sleep 300) &&
    jobid2=$(flux mini submit -n 16 -t 360s sleep 300) && # reserved
    jobid3=$(flux mini submit -n 16 -t 360s sleep 300) && # reserved
    jobid4=$(flux mini submit -n 16 -t 360s sleep 300) && # reserved
    jobid5=$(flux mini submit -n 2 -t 180s sleep 100) &&

    flux job wait-event -t 10 ${jobid5} start &&
    validate_sched_annotation ${jobid1} TRUE &&
    validate_sched_annotation ${jobid2} FALSE &&
    validate_sched_annotation ${jobid3} FALSE &&
    validate_sched_annotation ${jobid4} FALSE &&
    validate_sched_annotation ${jobid5} TRUE
'

test_expect_success 'annotation: cancel all active jobs 3' '
    active_jobs=$(flux job list --state=active | jq .id) &&
    for job in ${active_jobs}; do flux job cancel ${job}; done &&
    for job in ${active_jobs}; do flux job wait-event -t 10 ${job} clean; done
'

test_expect_success 'annotation: loading qmanager (queue-policy=fcfs)' '
    remove_qmanager &&
    reload_resource prune-filters=ALL:core \
subsystems=containment policy=low load-allowlist=cluster,node,core &&
    load_qmanager
'

test_expect_success 'annotation: works with FCFS policy' '
    jobid1=$(flux mini submit -n 8 -t 360s sleep 300) &&
    jobid2=$(flux mini submit -n 16 -t 360s sleep 300) && # block
    jobid3=$(flux mini submit -n 16 -t 360s sleep 300) &&
    jobid4=$(flux mini submit -n 16 -t 360s sleep 300) &&
    jobid5=$(flux mini submit -n 2 -t 180s sleep 100) &&

    flux job wait-event -t 10 ${jobid1} start &&
    flux job wait-event -t 10 ${jobid5} submit &&
    validate_sched_annotation ${jobid1} TRUE &&
    nonexistent_annotation ${jobid2} &&
    nonexistent_annotation ${jobid3} &&
    nonexistent_annotation ${jobid4} &&
    nonexistent_annotation ${jobid5} &&
    active_jobs=$(flux job list --state=active | jq .id) &&
    for job in ${active_jobs}; do flux job cancel ${job}; done &&
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

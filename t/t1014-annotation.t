#!/bin/sh

test_description='Test job annotation'

. `dirname $0`/sharness.sh

hwloc_basepath=`readlink -e ${SHARNESS_TEST_SRCDIR}/data/hwloc-data`
# 4 brokers, each (exclusively) have: 1 node, 2 sockets, 16 cores (8 per socket)
excl_4N4B="${hwloc_basepath}/004N/exclusive/04-brokers"

skip_all_unless_have jq

test_under_flux 1

nonexistent_annotation(){
    jobid=$(flux job id ${1}) &&
    ann=$(flux job list -A | grep ${jobid} | jq 'has("annotations")') &&
    test "${ann}" = "false"
}

validate_sched_annotation(){
    jobid=$(flux job id ${1}) &&
    queue_name=${2} &&
    start_time_is_zero=${3} &&
    ann=$(flux job list -A | grep ${jobid} | jq -c '.annotations') &&
    queue=$(echo ${ann} | jq '.sched.queue') &&
    t_est=$(echo ${ann} | jq '.sched.t_estimate') &&
    test "\"${queue_name}\"" = "${queue}" &&
    if test x"${start_time_is_zero}" = x"TRUE";
    then
        test "${t_est}" = "0"
    else
        test "${t_est}" != "0"
    fi
}

print_queue() {
    flux jobs -o '{sched.queue:>10h}'
}

print_t_estimate() {
    flux jobs -o '{sched.t_estimate!D:>10h}'
}

test_expect_success 'annotation: hwloc reload works' '
    flux hwloc reload ${excl_4N4B}
'

test_expect_success 'annotation: loading qmanager (queue-policy=easy)' '
    flux module remove sched-simple &&
    load_resource prune-filters=ALL:core \
subsystems=containment policy=low load-allowlist=cluster,node,core &&
    load_qmanager queue-policy=easy
'

test_expect_success 'annotation: works with EASY policy' '
    jobid1=$(flux mini submit -n 8 -t 360 sleep 300) &&
    jobid2=$(flux mini submit -n 16 -t 360 sleep 300) && # reserved
    jobid3=$(flux mini submit -n 16 -t 360 sleep 300) && # skipped
    jobid4=$(flux mini submit -n 16 -t 360 sleep 300) && # skipped
    jobid5=$(flux mini submit -n 2 -t 180 sleep 100) &&

    flux job wait-event -t 10 ${jobid5} start &&
    validate_sched_annotation ${jobid1} default TRUE &&
    validate_sched_annotation ${jobid2} default FALSE &&
    nonexistent_annotation ${jobid3} &&
    nonexistent_annotation ${jobid4} &&
    validate_sched_annotation ${jobid5} default TRUE &&
    print_queue &&
    print_t_estimate
'

test_expect_success 'annotation: cancel all active jobs 1' '
    active_jobs=$(flux job list --state=active | jq .id) &&
    for job in ${active_jobs}; do flux job cancel ${job}; done &&
    for job in ${active_jobs}; do flux job wait-event -t 10 ${job} clean; done
'

test_expect_success 'annotation: loading qmanager (queue-policy=hybrid)' '
    remove_resource  &&
    load_resource prune-filters=ALL:core \
subsystems=containment policy=low load-allowlist=cluster,node,core &&
    load_qmanager queue-policy=hybrid policy-params=reservation-depth=2
'

test_expect_success 'annotation: works with HYBRID policy' '
    jobid1=$(flux mini submit -n 8 -t 360 sleep 300) &&
    jobid2=$(flux mini submit -n 16 -t 360 sleep 300) && # reserved
    jobid3=$(flux mini submit -n 16 -t 360 sleep 300) && # reserved
    jobid4=$(flux mini submit -n 16 -t 360 sleep 300) && # skipped
    jobid5=$(flux mini submit -n 2 -t 180 sleep 100) &&

    flux job wait-event -t 10 ${jobid5} start &&
    validate_sched_annotation ${jobid1} default TRUE &&
    validate_sched_annotation ${jobid2} default FALSE &&
    validate_sched_annotation ${jobid3} default FALSE &&
    nonexistent_annotation ${jobid4} &&
    validate_sched_annotation ${jobid5} default TRUE
'

test_expect_success 'annotation: cancel all active jobs 2' '
    active_jobs=$(flux job list --state=active | jq .id) &&
    for job in ${active_jobs}; do flux job cancel ${job}; done &&
    for job in ${active_jobs}; do flux job wait-event -t 10 ${job} clean; done
'

test_expect_success 'annotation: loading qmanager (queue-policy=conservative)' '
    remove_resource  &&
    load_resource prune-filters=ALL:core \
subsystems=containment policy=low load-allowlist=cluster,node,core &&
    load_qmanager queue-policy=conservative
'

test_expect_success 'annotation: works with CONSERVATIVE policy' '
    jobid1=$(flux mini submit -n 8 -t 360 sleep 300) &&
    jobid2=$(flux mini submit -n 16 -t 360 sleep 300) && # reserved
    jobid3=$(flux mini submit -n 16 -t 360 sleep 300) && # reserved
    jobid4=$(flux mini submit -n 16 -t 360 sleep 300) && # reserved
    jobid5=$(flux mini submit -n 2 -t 180 sleep 100) &&

    flux job wait-event -t 10 ${jobid5} start &&
    validate_sched_annotation ${jobid1} default TRUE &&
    validate_sched_annotation ${jobid2} default FALSE &&
    validate_sched_annotation ${jobid3} default FALSE &&
    validate_sched_annotation ${jobid4} default FALSE &&
    validate_sched_annotation ${jobid5} default TRUE
'

test_expect_success 'annotation: cancel all active jobs 3' '
    active_jobs=$(flux job list --state=active | jq .id) &&
    for job in ${active_jobs}; do flux job cancel ${job}; done &&
    for job in ${active_jobs}; do flux job wait-event -t 10 ${job} clean; done
'

test_expect_success 'annotation: loading qmanager (queue-policy=fcfs)' '
    remove_resource  &&
    load_resource prune-filters=ALL:core \
subsystems=containment policy=low load-allowlist=cluster,node,core &&
    load_qmanager
'

test_expect_success 'annotation: works with FCFS policy' '
    jobid1=$(flux mini submit -n 8 -t 360 sleep 300) &&
    jobid2=$(flux mini submit -n 16 -t 360 sleep 300) && # block
    jobid3=$(flux mini submit -n 16 -t 360 sleep 300) &&
    jobid4=$(flux mini submit -n 16 -t 360 sleep 300) &&
    jobid5=$(flux mini submit -n 2 -t 180 sleep 100) &&

    flux job wait-event -t 10 ${jobid1} start &&
    flux job wait-event -t 10 ${jobid5} submit &&
    validate_sched_annotation ${jobid1} default TRUE &&
    nonexistent_annotation ${jobid2} &&
    nonexistent_annotation ${jobid3} &&
    nonexistent_annotation ${jobid4} &&
    nonexistent_annotation ${jobid5} &&
    active_jobs=$(flux job list --state=active | jq .id) &&
    for job in ${active_jobs}; do flux job cancel ${job}; done &&
    for job in ${active_jobs}; do flux job wait-event -t 10 ${job} clean; done
'

test_expect_success 'removing resource and qmanager modules' '
    remove_qmanager &&
    remove_resource
'

test_done

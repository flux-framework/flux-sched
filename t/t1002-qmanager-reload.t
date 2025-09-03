#!/bin/sh

test_description='Test qmanager service reloading'

. `dirname $0`/sharness.sh

hwloc_basepath=`readlink -f ${SHARNESS_TEST_SRCDIR}/data/hwloc-data`
# 1 brokers, each (exclusively) have: 1 node, 2 sockets, 16 cores (8 per socket)
excl_1N1B="${hwloc_basepath}/001N/exclusive/01-brokers"

export FLUX_SCHED_MODULE=none
test_under_flux 1

submit_jobs()   {
    local id
    for i in `seq 1 $2`
    do
        id=$(flux job submit $1)
    done
    echo ${id}
}

test_expect_success 'qmanager: generate jobspec for a simple test job' '
    flux submit -n 1 -t 100s --dry-run sleep 10 > basic.json
'

test_expect_success 'load test resources' '
    load_test_resources ${excl_1N1B}
'

test_expect_success 'qmanager: loading resource and qmanager modules works' '
    load_resource prune-filters=ALL:core subsystems=containment policy=low &&
    load_qmanager
'

test_expect_success 'qmanager: submit 2 more as many jobs as there are cores ' '
    jobid1=$(submit_jobs basic.json 16) &&
    jobid2=$(flux job submit basic.json) &&
    jobid3=$(flux job submit basic.json) &&
    flux job wait-event -t 10 ${jobid3} submit &&
    flux job list > jobs.list &&
    echo ${jobid1} > jobid1.out &&
    echo ${jobid2} > jobid2.out &&
    echo ${jobid3} > jobid3.out
'

test_expect_success 'qmanager: restart keeps the main job queue intact' '
    remove_qmanager &&
    load_qmanager &&
    flux job list > jobs.list2 &&
    diff jobs.list2 jobs.list
'

test_expect_success 'qmanager: handle the alloc resubmitted by job-manager' '
    jobid1=$(cat jobid1.out) &&
    jobid2=$(cat jobid2.out) &&
    jobid3=$(cat jobid3.out) &&
    flux cancel ${jobid1} &&
    flux job wait-event -t 10 ${jobid2} start
'

test_expect_success 'qmanager: canceling a pending job works' '
    jobid3=$(cat jobid3.out) &&
    flux cancel ${jobid3} &&
    flux job wait-event -t 10 ${jobid3} exception
'

test_expect_success 'cleanup active jobs' '
    cleanup_active_jobs
'

test_expect_success 'removing resource and qmanager modules' '
    remove_qmanager &&
    remove_resource
'

test_done

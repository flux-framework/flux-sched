#!/bin/sh

test_description='Test fluxion with feasibility validator of job-validator'

. `dirname $0`/sharness.sh

hwloc_basepath=`readlink -e ${SHARNESS_TEST_SRCDIR}/data/hwloc-data`
# 1 brokers, each (exclusively) have: 1 node, 2 sockets, 16 cores (8 per socket)
excl_1N1B="${hwloc_basepath}/001N/exclusive/01-brokers"

export FLUX_SCHED_MODULE=none
test_under_flux 1

test_expect_success 'feasibility: loading test resources works' '
    load_test_resources ${excl_1N1B}
'

test_expect_success 'feasibility: loading resource and qmanager modules works' '
    flux module load sched-fluxion-resource prune-filters=ALL:core \
subsystems=containment policy=low &&
    load_qmanager
'

test_expect_success 'feasibility: --plugins=feasibility works ' '
    flux run -n 999 --dry-run hostname | \
        flux job-validator --jobspec-only --plugins=feasibility \
        | jq -e ".errnum != 0"
'

test_expect_success 'feasibility: loading job-ingest with feasibilty works' '
    flux module reload job-ingest validator-plugins=feasibility
 '

 test_expect_success 'feasibility: unsatisfiable jobs are rejected' '
    test_must_fail flux submit -n 170 hostname 2>err1 &&
    grep "Unsatisfiable request" err1 &&
    test_must_fail flux submit -N 2 -n2 hostname 2>err2 &&
    grep "Unsatisfiable request" err2 &&
    test_must_fail flux submit -g 1 hostname 2>err3 &&
    grep -i "Unsatisfiable request" err3
 '

test_expect_success 'feasibility: satisfiable jobs are accepted' '
    jobid=$(flux submit -n 8 hostname) &&
    flux job wait-event -t 10 ${jobid} start &&
    flux job wait-event -t 10 ${jobid} finish &&
    flux job wait-event -t 10 ${jobid} release &&
    flux job wait-event -t 10 ${jobid} clean
'

test_expect_success 'feasibility: load job-ingest with two validators' '
    flux module reload job-ingest validator-plugins=feasibility,jobspec
 '

 test_expect_success 'feasibility: unsatisfiable jobs are rejected' '
    test_must_fail flux submit -n 170 hostname 2>err4 &&
    grep "Unsatisfiable request" err4 &&
    test_must_fail flux submit -N 2 -n2 hostname 2>err5 &&
    grep "Unsatisfiable request" err5 &&
    test_must_fail flux submit -g 1 hostname 2>err6 &&
    grep -i "Unsatisfiable request" err6
 '

test_expect_success 'feasibility: satisfiable jobs are accepted' '
    jobid=$(flux submit -n 8 hostname) &&
    flux job wait-event -t 10 ${jobid} start &&
    flux job wait-event -t 10 ${jobid} finish &&
    flux job wait-event -t 10 ${jobid} release &&
    flux job wait-event -t 10 ${jobid} clean
'

test_expect_success 'feasibility: cleanup active jobs' '
    cleanup_active_jobs
'

test_expect_success 'feasibility: removing resource and qmanager modules' '
    remove_qmanager &&
    remove_resource
'

test_done


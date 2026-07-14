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

test_expect_success 'feasibility: loading resource, feasibility, and qmanager modules works' '
    flux module load sched-fluxion-resource prune-filters=ALL:core \
subsystems=containment policy=low &&
    load_feasibility &&
    load_qmanager
'

test_expect_success 'feasibility: --plugins=feasibility works ' '
    flux run -n 999 --dry-run hostname | \
        flux job-validator --jobspec-only --plugins=feasibility \
        | jq -e ".errnum != 0"
'

run_sat_unsat() {
    local label="$1"

    test_expect_success "feasibility: unsatisfiable jobs are rejected ($label): 170 cores" '
       test_must_fail flux submit -n 170 hostname 2>"err_${label}_cores" &&
       grep "Unsatisfiable request" err_${label}_cores
    '

    test_expect_success "feasibility: unsatisfiable jobs are rejected ($label): 2 nodes" '
       test_must_fail flux submit -N 2 -n2 hostname 2>"err_${label}_nodes" &&
       grep "Unsatisfiable request" err_${label}_nodes
    '

    test_expect_success "feasibility: unsatisfiable jobs are rejected ($label): 1 gpu" '
       test_must_fail flux submit -g 1 hostname 2>"err_${label}_gpu" &&
       grep "Unsatisfiable request" err_${label}_gpu
    '

    test_expect_success "feasibility: satisfiable jobs are accepted ($label)" '
        jobid=$(flux submit -n 8 hostname) &&
        flux job wait-event -t 10 ${jobid} start &&
        flux job wait-event -t 10 ${jobid} finish &&
        flux job wait-event -t 10 ${jobid} release &&
        flux job wait-event -t 10 ${jobid} clean
    '
}

test_expect_success 'feasibility: loading job-ingest with feasibilty works' '
    flux module reload job-ingest validator-plugins=feasibility
 '

run_sat_unsat feasibility

test_expect_success 'feasibility: load job-ingest with two validators' '
    flux module reload job-ingest validator-plugins=feasibility,jobspec
 '

run_sat_unsat feasibility_jobspec

test_expect_success 'feasibility: cleanup active jobs' '
    cleanup_active_jobs
'

test_expect_success 'feasibility: removing resource, feasibility, and qmanager modules' '
    remove_qmanager &&
    remove_feasibility &&
    remove_resource
'

test_done


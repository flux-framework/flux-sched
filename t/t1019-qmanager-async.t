#!/bin/sh

test_description='Coverage for async RPC between fluxion modules'

. `dirname $0`/sharness.sh

hwloc_basepath=`readlink -f ${SHARNESS_TEST_SRCDIR}/data/hwloc-data`
# 1 brokers, each (exclusively) have: 1 node, 2 sockets, 16 cores (8 per socket)
excl_1N1B="${hwloc_basepath}/001N/exclusive/01-brokers"

export FLUX_SCHED_MODULE=none
test_under_flux 1

submit_jobs() {
    for i in `seq 1 $1`
    do
        flux job submit $2
    done
}

test_expect_success 'qmanager: generate jobspec for a simple test job' '
    flux submit -n16 -t 100s --dry-run sleep 100 > basic.json
'

test_expect_success 'load test resources' '
    load_test_resources ${excl_1N1B}
'

test_expect_success 'qmanager: loading resource and qmanager modules works' '
    flux module load sched-fluxion-resource prune-filters=ALL:core \
subsystems=containment policy=low &&
    load_qmanager queue-policy=easy
'

test_expect_success 'qmanager: many basic job submitted' '
    jobid=$(flux job submit basic.json) &&
    flux job wait-event -t 10 ${jobid} start &&
    submit_jobs 30 basic.json
'

# Statistically speaking some cancels hit qmanager
# when its sched-loop is active (with 30 jobs pending).
# We don't use flux queue stop because that can lead
# to duplicate cancel requests
test_expect_success 'qmanager: rapidly cancel all jobs' '
    flux cancel --all &&
    flux queue idle
'

test_expect_success 'removing resource and qmanager modules' '
    remove_qmanager &&
    remove_resource
'

test_done

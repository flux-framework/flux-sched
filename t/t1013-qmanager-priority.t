#!/bin/sh

test_description='Fluxion takes into account urgency and t_submit'

. `dirname $0`/sharness.sh

export TEST_UNDER_FLUX_QUORUM=1
export TEST_UNDER_FLUX_START_MODE=leader

hwloc_basepath=`readlink -f ${SHARNESS_TEST_SRCDIR}/data/hwloc-data`
# 1 node, 2 sockets, 44 cores (22 per socket), 4 gpus (2 per socket)
excl_1N1B="${hwloc_basepath}/001N/exclusive/01-brokers-sierra2"

export FLUX_SCHED_MODULE=none
test_under_flux 10 system

test_expect_success 'load test resources' '
    load_test_resources ${excl_1N1B}
'

test_expect_success 'priority: loading fluxion modules works' '
    load_resource &&
    load_qmanager
'

test_expect_success 'priority: a full-size job can be scheduled and run' '
    jobid1=$(flux submit -N 1 -n 1 -c 44 -g 4 -t 1h \
--urgency 16 sleep 3600) &&
    flux job wait-event -t 10 ${jobid1} start
'

test_expect_success 'priority: 2 jobs with higher urgency will not run' '
    jobid2=$(flux submit -N 1 -n 1 -c 44 -g 4 -t 1h \
--urgency 17 sleep 3600) &&
    jobid3=$(flux submit -N 1 -n 1 -c 44 -g 4 -t 1h \
--urgency 18 sleep 3600) &&
    test_must_fail flux job wait-event -t 1 ${jobid2} start
'

test_expect_success 'priority: canceling the first job starts the last job' '
    flux cancel ${jobid1} &&
    flux job wait-event -t 10 ${jobid3} start
'

test_expect_success 'priority: submit job with higher urgency' '
    jobid4=$(flux submit -N 1 -n 1 -c 44 -g 4 -t 1h \
--urgency 20 sleep 3600) &&
    test_must_fail flux job wait-event -t 1 ${jobid4} start
'

test_expect_success 'priority: change older job to higher urgency' '
    flux job urgency ${jobid2} 22 &&
    flux job wait-event -t 10 -c 2 ${jobid2} priority
'

test_expect_success 'priority: canceling the running job starts the earlier job' '
    flux cancel ${jobid3} &&
    flux job wait-event -t 10 ${jobid2} start
'

test_expect_success 'priority: cancel all jobs' '
    flux cancel ${jobid4} &&
    flux cancel ${jobid2} &&
    flux job wait-event -t 10 ${jobid4} clean &&
    flux job wait-event -t 10 ${jobid2} release
'

test_expect_success 'cleanup active jobs' '
    flux cancel --all &&
    flux queue idle
'

test_expect_success 'update configuration' '
flux config load <<-'EOF'
[[resource.config]]
hosts = "fake[0-10]"
cores = "0-63"
gpus = "0-3"

[[resource.config]]
hosts = "fake[0-10]"
properties = ["compute"]

[sched-fluxion-qmanager]
queue-policy = "conservative"

[sched-fluxion-resource]
match-policy = "firstnodex"
prune-filters = "ALL:core,ALL:gpu,cluster:node,rack:node"
match-format = "rv1_nosched"
EOF
'

test_expect_success 'reload resource with monitor-force-up' '
    flux module reload -f resource noverify monitor-force-up
'
test_expect_success 'load fluxion modules' '
    flux module load sched-fluxion-resource &&
    flux module load sched-fluxion-qmanager
'
test_expect_success 'wait for fluxion to be ready' '
  flux python -c \
    "import flux, json; print(flux.Flux().rpc(\"sched.resource-status\").get_str())"
'
test_expect_success 'start a single long job to reserve a node' '
    jobid1=$(flux submit -N1 \
        -t 300s \
        --flags=waitable \
        --setattr=exec.test.run_duration=30s \
        sleep 300) &&
    flux job wait-event -t 10 ${jobid1} start
'
test_expect_success 'submit a job with normal priority that needs all resources' '
  # must not have a time, so it reserves until the end of time
    jobid2=$(flux submit -N11 \
        --setattr=exec.test.run_duration=30s \
        sleep 300)
'
test_expect_success 'submit a job with low priority that only needs one node, must end up blocked' '
    jobid3=$(flux submit -N1 \
        --flags=waitable \
        --urgency=8 \
        -t 400s \
        --setattr=exec.test.run_duration=30s \
        sleep 400) &&
    test_must_fail flux job wait-event -t 1 ${jobid3} start
'
test_expect_success 'priority: change older job to lower urgency' '
    flux job urgency ${jobid2} 7 &&
    flux job wait-event -t 10 ${jobid2} priority
'
test_expect_success 'wait for blocked job to start' '
    flux job wait-event -t 1 ${jobid3} start
'

test_expect_success 'cleanup active jobs' '
    cleanup_active_jobs
'

test_expect_success 'priority: removing fluxion modules' '
    remove_qmanager &&
    remove_resource
'

test_done


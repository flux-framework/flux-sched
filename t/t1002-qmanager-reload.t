#!/bin/sh

test_description='Test qmanager service reloading'

. `dirname $0`/sharness.sh

hwloc_basepath=`readlink -e ${SHARNESS_TEST_SRCDIR}/data/hwloc-data`
# 4 brokers, each (exclusively) have: 1 node, 2 sockets, 16 cores (8 per socket)
excl_4N4B="${hwloc_basepath}/004N/exclusive/04-brokers"

test_under_flux 1

#  Set path to jq(1)
#
jq=$(which jq 2>/dev/null)
if test -z "$jq"; then
    skip_all='jq not found. Skipping all tests'
    test_done
fi

exec_test()     { ${jq} '.attributes.system.exec.test = {}'; }
submit_jobs()   {
    local id
    for i in `seq 1 $2`
    do
        id=$(flux job submit $1)
    done
    echo ${id}
}

test_expect_success 'qmanager: generate jobspec for a simple test job' '
    flux jobspec srun -n1 -t 100 hostname | exec_test > basic.json
'

test_expect_success 'qmanager: hwloc reload works' '
    flux hwloc reload ${excl_4N4B}
'

test_expect_success 'qmanager: loading resource and qmanager modules works' '
    flux module remove sched-simple &&
    flux module load resource prune-filters=ALL:core \
subsystems=containment policy=low load-whitelist=node,socket,core,gpu &&
    flux module load qmanager
'

test_expect_success 'qmanager: submit 2 more as many jobs as there are cores ' '
    jobid1=$(submit_jobs basic.json 16) &&
    jobid2=$(flux job submit basic.json) &&
    jobid3=$(flux job submit basic.json) &&
    flux job wait-event -t 2 ${jobid3} submit &&
    flux job list > jobs.list &&
    echo ${jobid1} > jobid1.out &&
    echo ${jobid2} > jobid2.out &&
    echo ${jobid3} > jobid3.out
'

test_expect_success 'qmanager: restart keeps the main job queue intact' '
    flux module remove qmanager &&
    flux module load qmanager &&
    flux job list > jobs.list2 &&
    diff jobs.list2 jobs.list
'

test_expect_success 'qmanager: handle the alloc resubmitted by job-manager' '
    jobid1=$(cat jobid1.out) &&
    jobid2=$(cat jobid2.out) &&
    jobid3=$(cat jobid3.out) &&
    flux job cancel ${jobid1} &&
    flux job wait-event -t 2 ${jobid2} start
'

test_expect_success 'qmanager: canceling a pending job works' '
    jobid3=$(cat jobid3.out) &&
    flux job cancel ${jobid3} &&
    flux job wait-event -t 2 ${jobid3} exception
'

test_expect_success 'removing resource and qmanager modules' '
    flux module remove qmanager &&
    flux module remove resource
'

# Reload the core scheduler so that rc3 won't hang waiting for
# queue to become idle after jobs are canceled.
test_expect_success 'load sched-simple module' '
    flux module load sched-simple
'

test_done

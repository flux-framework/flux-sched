#!/bin/sh

test_description='Test the correctness of various queuing optimization'

. `dirname $0`/sharness.sh

hwloc_basepath=`readlink -e ${SHARNESS_TEST_SRCDIR}/data/hwloc-data`
# 1 brokers, each (exclusively) have: 1 node, 2 sockets, 16 cores (8 per socket)
excl_1N1B="${hwloc_basepath}/001N/exclusive/01-brokers"

export FLUX_SCHED_MODULE=none
test_under_flux 1

selection_type() {
    flux job list-ids $1 | jq -r ".annotations.sched.selection_type"
}

exec_test()     { ${jq} '.attributes.system.exec.test = {}'; }

test_expect_success 'qmanager: generate jobspecs of varying requirements' '
    flux run --job-name=1 --dry-run -n8 -t 60m hostname | exec_test > C08.T3600.json && #1
    flux run --job-name=2 --dry-run -n10 -t 60m hostname | exec_test > C10.T3600.json && #2
    flux run --job-name=3 --dry-run -n12 -t 60m hostname | exec_test > C12.T3600.json && #3
    flux run --job-name=4 --dry-run -n14 -t 60m hostname | exec_test > C14.T3600.json && #4
    flux run --job-name=5 --dry-run -n16 -t 60m hostname | exec_test > C16.T3600.json && #5
    flux run --job-name=6 --dry-run -n6 -t 121m hostname | exec_test > C06.T7260.json && #6
    flux run --job-name=7 --dry-run -n4 -t 179m hostname | exec_test > C04.T10800.json && #7
    flux run --job-name=8 --dry-run -n4 -t 239m hostname | exec_test > C04.T14400.json && #8
    flux run --job-name=9 --dry-run -n2 -t 239m hostname | exec_test > C02.T14400.json && #9
    flux run --job-name=10 --dry-run -n2 -t 299m hostname | exec_test > C02.T18000.json && #10
    flux run --job-name=11 --dry-run -n2 -t 59m hostname | exec_test > C02.T3600.json #11
'

test_expect_success 'load test resources' '
    load_test_resources ${excl_1N1B}
'

test_expect_success 'qmanager: loading with easy+queue-depth=5' '
    load_resource prune-filters=ALL:core subsystems=containment policy=low &&
    load_qmanager queue-policy=easy queue-params=queue-depth=5
'

test_expect_success 'qmanager: EASY policy conforms to queue-depth=5' '
    jobid1=$(flux job submit C08.T3600.json) && # 8 allocated 8 free until 3600
    jobid2=$(flux job submit C10.T3600.json) && # reserved, switching to BF-only
    jobid3=$(flux job submit C12.T3600.json) && # can not reserve, EBUSY on ALLOC#1
    jobid4=$(flux job submit C14.T3600.json) && # can not reserve, EBUSY on ALLOC#2
    jobid5=$(flux job submit C16.T3600.json) && # can not reserve, EBUSY on ALLOC#3
    jobid6=$(flux job submit C06.T7260.json) && # BF A, 14 allocated 2 free until 3600
    jobid7=$(flux job submit C04.T10800.json) && # BF B when 6 completes
    jobid8=$(flux job submit C04.T14400.json) && # BF C when 7 completes
    jobid9=$(flux job submit C02.T14400.json) && # BF D when 7 completes on second loop
    jobid10=$(flux job submit C02.T18000.json) && # Cannot start because qd
    jobid11=$(flux job submit C02.T3600.json) && # Cannot start because qd

    flux job wait-event -t 10 ${jobid1} start &&
    flux job wait-event -t 10 ${jobid6} start &&
    flux cancel ${jobid6} &&
    flux job wait-event -t 10 ${jobid7} start &&
    flux cancel ${jobid7} &&
    flux job wait-event -t 10 ${jobid8} start &&
    test $(flux job list --states=active | wc -l) -eq 9 &&
    test $(flux job list --states=running | wc -l) -eq 3
'
test_expect_success 'qmanager: selection_type is immediate and backfill' '
    test "$(selection_type ${jobid1})" = "immediate" &&
    test "$(selection_type ${jobid6})" = "backfill" &&
    test "$(selection_type ${jobid7})" = "backfill" &&
    test "$(selection_type ${jobid8})" = "backfill"
'
test_expect_success 'qmanager: selection_type is reserved' '
    flux cancel ${jobid1} &&
    flux job wait-event -t 10 ${jobid2} start &&
    test "$(selection_type ${jobid2})" = "reserved"
'
test_expect_success 'qmanager: cancel all active jobs' '
    active_jobs=$(flux job list --states=active | jq .id) &&
    for job in ${active_jobs}; do flux cancel ${job}; done &&
    for job in ${active_jobs}; do flux job wait-event -t 10 ${job} clean; done
'
test_expect_success 'qmanager: loading with hybrid+queue-depth=5' '
    remove_resource &&
    load_resource prune-filters=ALL:core subsystems=containment policy=low &&
    load_qmanager queue-policy=hybrid \
queue-params=queue-depth=5 policy-params=reservation-depth=3
'

test_expect_success 'qmanager: HYBRID policy conforms to queue-depth=5' '
    jobid1=$(flux job submit C08.T3600.json) && # immediate
    jobid2=$(flux job submit C10.T3600.json) && # reserved
    jobid3=$(flux job submit C12.T3600.json) && # reserved
    jobid4=$(flux job submit C14.T3600.json) && # reserved
    jobid5=$(flux job submit C16.T3600.json) &&
    jobid6=$(flux job submit C06.T7260.json) &&
    jobid7=$(flux job submit C04.T10800.json) && # Cannot start because of qd
    jobid8=$(flux job submit C04.T14400.json) &&
    jobid9=$(flux job submit C02.T14400.json) &&
    jobid10=$(flux job submit C02.T18000.json) &&
    jobid11=$(flux job submit C02.T3600.json) &&

    flux job wait-event -t 10 ${jobid1} start &&
    test $(flux job list --states=active | wc -l) -eq 11 &&
    test $(flux job list --states=running| wc -l) -eq 1
'
# hybrid reserves up to reservation-depth jobs; jobid1 runs on the first
# attempt (immediate).  With the reservations held, no later job can
# backfill without delaying them, so nothing else starts.
test_expect_success 'qmanager: HYBRID selection_type is immediate' '
    test "$(selection_type ${jobid1})" = "immediate"
'
# Cancel jobid1 to free its cores so the reserved jobid2 can finally
# start; because it had previously been reserved it is categorized as
# reserved.
test_expect_success 'qmanager: HYBRID selection_type is reserved' '
    flux cancel ${jobid1} &&
    flux job wait-event -t 10 ${jobid2} start &&
    test "$(selection_type ${jobid2})" = "reserved"
'
test_expect_success 'qmanager: cancel all active jobs' '
    active_jobs=$(flux job list --states=active | jq .id) &&
    for job in ${active_jobs}; do flux cancel ${job}; done &&
    for job in ${active_jobs}; do flux job wait-event -t 10 ${job} clean; done
'

test_expect_success 'qmanager: loading with conservative+queue-depth=5' '
    remove_resource &&
    load_resource prune-filters=ALL:core subsystems=containment policy=low &&
    load_qmanager queue-policy=conservative queue-params=queue-depth=5
'

test_expect_success 'qmanager: CONSERVATIVE policy conforms to queue-depth=5' '
    jobid1=$(flux job submit C08.T3600.json) && # immediate
    jobid2=$(flux job submit C10.T3600.json) && # reserved
    jobid3=$(flux job submit C12.T3600.json) && # reserved
    jobid4=$(flux job submit C14.T3600.json) && # reserved
    jobid5=$(flux job submit C16.T3600.json) && # reserved
    jobid6=$(flux job submit C06.T7260.json) && # reserved
    jobid7=$(flux job submit C04.T10800.json) && # Cannot start because of qd
    jobid8=$(flux job submit C04.T14400.json) &&
    jobid9=$(flux job submit C02.T14400.json) &&
    jobid10=$(flux job submit C02.T18000.json) &&
    jobid11=$(flux job submit C02.T3600.json) &&

    flux job wait-event -t 10 ${jobid1} start &&
    test $(flux job list --states=active | wc -l) -eq 11 &&
    test $(flux job list --states=running | wc -l) -eq 1
'
# conservative reserves every job it cannot run now, so jobid1 runs on the
# first attempt (immediate) and nothing can backfill past the reservations.
test_expect_success 'qmanager: CONSERVATIVE selection_type is immediate' '
    test "$(selection_type ${jobid1})" = "immediate"
'
# Cancel jobid1 to free its cores so the reserved jobid2 can finally
# start; because it had previously been reserved it is categorized as
# reserved.
test_expect_success 'qmanager: CONSERVATIVE selection_type is reserved' '
    flux cancel ${jobid1} &&
    flux job wait-event -t 10 ${jobid2} start &&
    test "$(selection_type ${jobid2})" = "reserved"
'
test_expect_success 'qmanager: cancel all active jobs' '
    active_jobs=$(flux job list --states=active | jq .id) &&
    for job in ${active_jobs}; do flux cancel ${job}; done &&
    for job in ${active_jobs}; do flux job wait-event -t 10 ${job} clean; done
'
test_expect_success 'cleanup active jobs' '
    cleanup_active_jobs
'
test_expect_success 'qmanager: all jobs under fcfs have sched.selection_type=immediate' '
    remove_qmanager &&
    reload_resource prune-filters=ALL:core \
subsystems=containment policy=low load-allowlist=cluster,node,core &&
    load_qmanager &&
    flux queue start --all &&
    jobid1=$(flux submit -n 8 -t 360s sleep 300) &&
    flux job wait-event -t 10 ${jobid1} start &&
    test "$(selection_type ${jobid1})" = "immediate" &&
    active_jobs=$(flux job list --states=active | jq .id) &&
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

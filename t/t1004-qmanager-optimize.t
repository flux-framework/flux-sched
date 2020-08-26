#!/bin/sh

test_description='Test the correctness of various queuing optimization'

. `dirname $0`/sharness.sh

hwloc_basepath=`readlink -e ${SHARNESS_TEST_SRCDIR}/data/hwloc-data`
# 4 brokers, each (exclusively) have: 1 node, 2 sockets, 16 cores (8 per socket)
excl_4N4B="${hwloc_basepath}/004N/exclusive/04-brokers"

skip_all_unless_have jq

test_under_flux 1

exec_test()     { ${jq} '.attributes.system.exec.test = {}'; }

test_expect_success 'qmanager: generate jobspecs of varying requirements' '
    flux jobspec srun  -n8 -t 60 hostname | exec_test > C08.T3600.json && #1
    flux jobspec srun -n10 -t 60 hostname | exec_test > C10.T3600.json && #2
    flux jobspec srun -n12 -t 60 hostname | exec_test > C12.T3600.json && #3
    flux jobspec srun -n14 -t 60 hostname | exec_test > C14.T3600.json && #4
    flux jobspec srun -n16 -t 60 hostname | exec_test > C16.T3600.json && #5
    flux jobspec srun -n6 -t 121 hostname | exec_test > C06.T7260.json && #6
    flux jobspec srun -n4 -t 179 hostname | exec_test > C04.T10800.json && #7
    flux jobspec srun -n4 -t 239 hostname | exec_test > C04.T14400.json && #8
    flux jobspec srun -n2 -t 239 hostname | exec_test > C02.T14400.json && #9
    flux jobspec srun -n2 -t 299 hostname | exec_test > C02.T18000.json && #10
    flux jobspec srun -n2 -t 59 hostname | exec_test > C02.T3600.json #11
'

test_expect_success 'qmanager: hwloc reload works' '
    flux hwloc reload ${excl_4N4B}
'

test_expect_success 'qmanager: loading with easy+queue-depth=5' '
    flux module remove sched-simple &&
    load_resource prune-filters=ALL:core \
subsystems=containment policy=low load-allowlist=cluster,node,core &&
    load_qmanager queue-policy=easy \
queue-params=queue-depth=5
'

test_expect_success 'qmanager: EASY policy conforms to queue-depth=5' '
    jobid1=$(flux job submit C08.T3600.json) &&
    jobid2=$(flux job submit C10.T3600.json) && # reserved
    jobid3=$(flux job submit C12.T3600.json) &&
    jobid4=$(flux job submit C14.T3600.json) &&
    jobid5=$(flux job submit C16.T3600.json) &&
    jobid6=$(flux job submit C06.T7260.json) && # BF A
    jobid7=$(flux job submit C04.T10800.json) && # BF B when 6 completes
    jobid8=$(flux job submit C04.T14400.json) && # BF C when 7 completes
    jobid9=$(flux job submit C02.T14400.json) && # Cannot start because qd
    jobid10=$(flux job submit C02.T18000.json) && # Cannot start because qd
    jobid11=$(flux job submit C02.T3600.json) &&

    flux job wait-event -t 10 ${jobid1} start &&
    flux job wait-event -t 10 ${jobid6} start &&
    flux job cancel ${jobid6} &&
    flux job wait-event -t 10 ${jobid7} start &&
    flux job cancel ${jobid7} &&
    flux job wait-event -t 10 ${jobid8} start &&
    test $(flux job list --states=active | wc -l) -eq 9 &&
    test $(flux job list --states=running | wc -l) -eq 2 &&
    active_jobs=$(flux job list --states=active | jq .id) &&
    for job in ${active_jobs}; do flux job cancel ${job}; done &&
    for job in ${active_jobs}; do flux job wait-event -t 10 ${job} clean; done
'

test_expect_success 'qmanager: loading with hybrid+queue-depth=5' '
    remove_resource &&
    load_resource prune-filters=ALL:core \
subsystems=containment policy=low load-allowlist=cluster,node,core &&
    load_qmanager queue-policy=hybrid \
queue-params=queue-depth=5 policy-params=reservation-depth=3
'

test_expect_success 'qmanager: HYBRID policy conforms to queue-depth=5' '
    jobid1=$(flux job submit C08.T3600.json) &&
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
    test $(flux job list --states=running| wc -l) -eq 1 &&
    active_jobs=$(flux job list --states=active | jq .id) &&
    for job in ${active_jobs}; do flux job cancel ${job}; done &&
    for job in ${active_jobs}; do flux job wait-event -t 10 ${job} clean; done
'

test_expect_success 'qmanager: loading with conservative+queue-depth=5' '
    remove_resource &&
    load_resource prune-filters=ALL:core \
subsystems=containment policy=low load-allowlist=cluster,node,core &&
    load_qmanager queue-policy=conservative \
queue-params=queue-depth=5
'

test_expect_success 'qmanager: CONSERVATIVE policy conforms to queue-depth=5' '
    jobid1=$(flux job submit C08.T3600.json) &&
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
    test $(flux job list --states=running | wc -l) -eq 1 &&
    active_jobs=$(flux job list --states=active | jq .id) &&
    for job in ${active_jobs}; do flux job cancel ${job}; done &&
    for job in ${active_jobs}; do flux job wait-event -t 10 ${job} clean; done
'

test_expect_success 'removing resource and qmanager modules' '
    remove_qmanager &&
    remove_resource
'

test_done

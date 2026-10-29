#!/bin/sh

test_description='Test the coschedule queuing policy'

. `dirname $0`/sharness.sh

hwloc_basepath=`readlink -e ${SHARNESS_TEST_SRCDIR}/data/hwloc-data`
# 1 broker, 1 node, 2 sockets, 16 cores
excl_1N1B="${hwloc_basepath}/001N/exclusive/01-brokers"

export FLUX_SCHED_MODULE=none
test_under_flux 1

exec_test()     { ${jq} '.attributes.system.exec.test = {}'; }

# cleanup_active_jobs stops the queue to drain it and leaves it stopped, so
# anything submitted after a cleanup is never handed to the scheduler and just
# waits. Start it again.
drain() {
    cleanup_active_jobs &&
        flux queue start --all --quiet
}

# A held job keeps its footprint and does not start until something releases
# it. One job waits while another arranges access to whatever it waits on.
held()          { ${jq} '.attributes.system.hold = 1'; }

test_expect_success 'coschedule: generate jobspecs' '
    flux run --dry-run -n1 -t 60m hostname | exec_test > C01.json &&
    flux run --dry-run -n8 -t 60m hostname | exec_test > C08.json &&
    flux run --dry-run -n8 -t 60m hostname | exec_test | held > H08.json
'

test_expect_success 'load test resources' '
    load_test_resources ${excl_1N1B}
'

test_expect_success 'coschedule: the policy loads and names itself' '
    load_resource prune-filters=ALL:core subsystems=containment policy=first &&
    load_qmanager queue-policy=coschedule &&
    test $(flux module stats sched-fluxion-qmanager \
           | ${jq} -r ".queues|to_entries[0].value.policy") = "coschedule"
'

test_expect_success 'coschedule: an ordinary job schedules as backfill would' '
    jobid=$(flux job submit C08.json) &&
    flux job wait-event -t 10 ${jobid} start &&
    drain
'

test_expect_success 'coschedule: a held job keeps its footprint and waits' '
    jobid=$(flux job submit H08.json) &&
    test_must_fail flux job wait-event -t 5 ${jobid} start &&
    test $(flux job list --states=running | wc -l) -eq 0 &&
    drain
'

test_expect_success 'coschedule: work packs around a held job' '
    heldid=$(flux job submit H08.json) &&
    other=$(flux job submit C01.json) &&
    flux job wait-event -t 20 ${other} start &&
    test_must_fail flux job wait-event -t 5 ${heldid} start &&
    drain
'

test_expect_success 'cleanup active jobs' '
    drain
'

test_expect_success 'removing resource and qmanager modules' '
    remove_qmanager &&
    remove_resource
'

# Only coschedule acts on the hold. Under any other policy a held job is an
# ordinary job and starts.
test_expect_success 'easy does not act on the hold, so the job starts' '
    load_resource prune-filters=ALL:core subsystems=containment policy=first &&
    load_qmanager queue-policy=easy &&
    jobid=$(flux job submit H08.json) &&
    flux job wait-event -t 20 ${jobid} start &&
    drain &&
    remove_qmanager &&
    remove_resource
'

test_done

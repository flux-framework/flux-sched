#!/bin/sh

test_description='Test Graph Data Store under the Fluxion Resource Module'

. `dirname $0`/sharness.sh

hwloc_basepath=`readlink -f ${SHARNESS_TEST_SRCDIR}/data/hwloc-data`
# 4 brokers, each (exclusively) have: 1 node, 2 sockets, 16 cores (8 per socket)
excl_4N4B="${hwloc_basepath}/004N/exclusive/04-brokers"

verify() {
    local of=$1
    echo "{\"[0-3]\": 17}" | jq ' ' > ref.out
    cat ${of} | grep Rank: | awk '{ print $6 $7}' | jq ' ' > cmp.out
    diff cmp.out ref.out
    return $?
}

export FLUX_SCHED_MODULE=none
test_under_flux 4

test_expect_success 'qmanager: load test resources' '
    load_test_resources ${excl_4N4B}
'

test_expect_success 'qmanager: loading resource and qmanager modules works' '
    load_resource prune-filters=ALL:core subsystems=containment policy=low
'

test_expect_success 'qmanager: graph stat as expected' '
    flux ion-resource stats > stat.out &&
    test_debug "cat stat.out" &&
    verify stat.out
'

test_expect_success 'removing resource and qmanager modules' '
    remove_resource
'

test_done

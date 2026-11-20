#!/bin/sh

test_description='Test the deferred allocate match operation

A deferred allocate is a plain allocate whose start is advanced to the
attributes.system.deferred_start time the jobspec asks for, or now when it
asks for none. The result is an allocation, not a reservation, so it keeps
its resources until it is cancelled and blocks an allocation that would
overlap it.
'

. `dirname $0`/sharness.sh

hwloc_basepath=`readlink -e ${SHARNESS_TEST_SRCDIR}/data/hwloc-data`
# 1 broker: 1 node, 2 sockets, 16 cores (8 per socket)
excl_1N1B="${hwloc_basepath}/001N/exclusive/01-brokers"

export FLUX_SCHED_MODULE=none
test_under_flux 1

test_expect_success 'deferred: generate a whole node jobspec' '
	flux run --dry-run -N1 -x -t 1h sleep 3600 > node.json
'

test_expect_success 'deferred: load test resources' '
	load_test_resources ${excl_1N1B}
'

test_expect_success 'deferred: load fluxion resource module' '
	load_resource prune-filters=ALL:core subsystems=containment policy=high
'

test_expect_success 'deferred: no deferred_start allocates now' '
	flux ion-resource match deferred_allocate node.json > now.out &&
	grep -q ALLOCATED now.out &&
	jobid=$(sed -n 2p now.out | awk "{print \$1}") &&
	flux ion-resource cancel ${jobid}
'

test_expect_success 'deferred: a deferred_start in the past allocates now' '
	jq ".attributes.system.deferred_start = 1" node.json > past.json &&
	flux ion-resource match deferred_allocate past.json > past.out &&
	grep -q ALLOCATED past.out &&
	jobid=$(sed -n 2p past.out | awk "{print \$1}") &&
	flux ion-resource cancel ${jobid}
'

test_expect_success 'deferred: a future deferred_start is allocated, not reserved' '
	jq ".attributes.system.deferred_start = (now | floor) + 1800" \
	    node.json > deferred.json &&
	flux ion-resource match deferred_allocate deferred.json > deferred.out &&
	grep -q ALLOCATED deferred.out &&
	test_must_fail grep -q RESERVED deferred.out
'

test_expect_success 'deferred: the future allocation blocks an overlapping allocation' '
	test_must_fail flux ion-resource match allocate node.json
'

test_expect_success 'deferred: cancelling the future allocation frees the node' '
	jobid=$(sed -n 2p deferred.out | awk "{print \$1}") &&
	flux ion-resource cancel ${jobid} &&
	flux ion-resource match allocate node.json > allocate.out &&
	grep -q ALLOCATED allocate.out
'

test_expect_success 'deferred: a malformed deferred_start is refused' '
	jq ".attributes.system.deferred_start = \"notatime\"" \
	    node.json > malformed.json &&
	test_must_fail flux ion-resource match deferred_allocate malformed.json
'

test_expect_success 'deferred: remove fluxion resource module' '
	remove_resource
'

test_done

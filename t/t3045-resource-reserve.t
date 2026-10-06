#!/bin/sh

test_description='Test the reserve only match operation

A reserve only match never takes the resources at the current instant. It
reserves them at the earliest future time even when they are free, so the
reservation blocks an allocation that would overlap it until it is cancelled.
'

. `dirname $0`/sharness.sh

hwloc_basepath=`readlink -e ${SHARNESS_TEST_SRCDIR}/data/hwloc-data`
# 1 broker: 1 node, 2 sockets, 16 cores (8 per socket)
excl_1N1B="${hwloc_basepath}/001N/exclusive/01-brokers"

export FLUX_SCHED_MODULE=none
test_under_flux 1

test_expect_success 'reserve: generate a whole node jobspec' '
	flux run --dry-run -N1 -x -t 1h sleep 3600 > node.json
'

test_expect_success 'reserve: load test resources' '
	load_test_resources ${excl_1N1B}
'

test_expect_success 'reserve: load fluxion resource module' '
	load_resource prune-filters=ALL:core subsystems=containment policy=high
'

test_expect_success 'reserve: a free node is reserved, not allocated' '
	flux ion-resource match reserve node.json > reserve.out &&
	grep -q RESERVED reserve.out &&
	test_must_fail grep -q ALLOCATED reserve.out
'

test_expect_success 'reserve: the reservation blocks an overlapping allocation' '
	test_must_fail flux ion-resource match allocate node.json
'

test_expect_success 'reserve: cancelling the reservation frees the node' '
	jobid=$(sed -n 2p reserve.out | awk "{print \$1}") &&
	flux ion-resource cancel ${jobid} &&
	flux ion-resource match allocate node.json > allocate.out &&
	grep -q ALLOCATED allocate.out
'

test_expect_success 'reserve: an impossible request is refused' '
	flux run --dry-run -N2 -t 1h sleep 3600 > two.json &&
	test_must_fail flux ion-resource match reserve two.json
'

test_expect_success 'reserve: remove fluxion resource module' '
	remove_resource
'

test_done

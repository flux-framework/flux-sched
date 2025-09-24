#!/bin/sh
#set -x

test_description='Test sched-fluxion-resource.update RPC using RV1

Ensure that the resource.update handler within the resource module works
'

. `dirname $0`/sharness.sh

hwloc_basepath=`readlink -f ${SHARNESS_TEST_SRCDIR}/data/hwloc-data`
# 1 brokers, each (exclusively) have: 1 node, 2 sockets, 16 cores (8 per socket)
excl_1N1B="${hwloc_basepath}/001N/exclusive/01-brokers"

#
# test_under_flux is under sharness.d/
#
export FLUX_SCHED_MODULE=none
test_under_flux 1

test_expect_success 'update: generate jobspec for a simple test job' '
    flux run --dry-run -N 1 -n 1 -t 1h hostname > basic.json
'

test_expect_success 'update: load test resources' '
    load_test_resources ${excl_1N1B}
'

test_expect_success 'update: loading sched-fluxion-resource works' '
    load_resource match-format=rv1 policy=high
'

test_expect_success 'update: resource.match-allocate works with a jobspec' '
    flux ion-resource match allocate ./basic.json | \
awk "NR==5{ print; }" > R1 &&
    flux ion-resource match allocate ./basic.json | \
awk "NR==5{ print; }" > R2 &&
    flux ion-resource match allocate ./basic.json | \
awk "NR==5{ print; }" > R3 &&
    flux ion-resource match allocate ./basic.json | \
awk "NR==5{ print; }" > R4
'

test_expect_success 'update: reloading sched-fluxion-resource works' '
    remove_resource &&
    load_resource match-format=rv1 policy=high
'

test_expect_success 'update: sched-fluxion-resource.update works with R1' '
    flux ion-resource update R1 5056216694784 | \
awk "NR==5{ print; }" > R1.rep &&
    diff R1 R1.rep
'

test_expect_success 'update: works on an existing jobid with same R' '
    flux ion-resource update R1 5056216694784 | \
awk "NR==5{ print; }" > R1.rep &&
    diff R1 R1.rep
'

test_expect_success 'update: sched-fluxion-resource.update works with R2' '
    flux ion-resource update R2 5056216694790 | \
awk "NR==5{ print; }" > R2.rep &&
    diff R2 R2.rep
'

test_expect_success 'update: sched-fluxion-resource.update works with R3' '
    flux ion-resource update R3 5056216694793 | \
awk "NR==5{ print; }" > R3.rep &&
    diff R3 R3.rep
'

test_expect_success 'update: sched-fluxion-resource.update works with R4' '
    flux ion-resource update R4 5056216694851 | \
awk "NR==5{ print; }" > R4.rep &&
    diff R4 R4.rep
'

test_expect_success 'update: must fail on an existing jobid with diff R' '
    test_must_fail flux ion-resource update R2 5056216694784
'

test_expect_success 'update: fluxion-resource.update fails on existing R' '
    test_must_fail flux ion-resource update R1 1234567890
'

test_expect_success 'removing resource works' '
    remove_resource
'

test_done

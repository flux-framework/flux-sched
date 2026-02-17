#!/bin/sh
#set -x

test_description='Test the basic functionality of flexible traverser

Ensure that the flexible traverser within the resource module works
'

. `dirname $0`/sharness.sh

grug="${SHARNESS_TEST_SRCDIR}/data/resource/grugs/medium.graphml"
tiny_grug="${SHARNESS_TEST_SRCDIR}/data/resource/grugs/tiny.graphml"
small_grug="${SHARNESS_TEST_SRCDIR}/data/resource/grugs/small.graphml"
jobspec_dir="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/flexible"

#
# test_under_flux is under sharness.d/
#
test_under_flux 1

#
# print only with --debug
#
test_debug '
    echo ${grug} &&
    echo ${jobspec} &&
    echo ${malform}
'

test_expect_success 'loading resource module with a tiny machine config works' '
    load_resource \
    load-file=${grug} prune-filters=ALL:core \
    load-format=grug subsystems=containment policy=high traverser=flexible
'

test_expect_success 'match-allocate works with flexible jobspecs' '
    flux ion-resource match allocate ${jobspec_dir}/test001.yaml &&
    flux ion-resource match allocate ${jobspec_dir}/test004.yaml &&
    flux ion-resource match allocate ${jobspec_dir}/test005.yaml &&
    flux ion-resource match allocate ${jobspec_dir}/test008.yaml
'

test_expect_success 'match-allocate works with top-level xor-slot jobspecs' '
    flux ion-resource match allocate ${jobspec_dir}/test009.yaml
'

test_expect_success 'match-allocate works with multi-option nested xor-slot jobspecs' '
    flux ion-resource match allocate ${jobspec_dir}/test010.yaml
'

test_expect_success 'match-allocate works with single-option nested xor-slot jobspecs' '
    flux ion-resource match allocate ${jobspec_dir}/test011.yaml
'

test_expect_success 'reloading flexible traverser with tiny graph and first policy works' '
    remove_resource &&
    load_resource \
    load-file=${tiny_grug} prune-filters=ALL:core \
    load-format=grug subsystems=containment policy=first traverser=flexible
'

test_expect_success 'match-allocate works when xor branches use different subtree prefixes' '
    flux ion-resource match allocate ${jobspec_dir}/test012.yaml &&
    flux ion-resource match allocate ${jobspec_dir}/test012.yaml
'

test_expect_success 'reloading flexible traverser with small gpu-free graph and low policy works' '
    remove_resource &&
    load_resource \
    load-file=${small_grug} prune-filters=ALL:core \
    load-format=grug subsystems=containment policy=low traverser=flexible
'

test_expect_success 'match-allocate skips unsatisfiable gpu xor branches on gpu-free graphs' '
    flux ion-resource match allocate ${jobspec_dir}/test013.yaml &&
    flux ion-resource match allocate ${jobspec_dir}/test013.yaml
'

test_expect_success 'removing resource works' '
    remove_resource
'

test_done

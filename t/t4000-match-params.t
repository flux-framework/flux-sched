#!/bin/sh

test_description='Test the module load options of resource-match service'


ORIG_HOME=${HOME}

. `dirname $0`/sharness.sh

#
# sharness modifies $HOME environment variable, but this interferes
# with python's package search path, in particular its user site package.
#
HOME=${ORIG_HOME}

grug="${SHARNESS_TEST_SRCDIR}/data/resource/grugs/tiny.graphml"
ne_grug="${SHARNESS_TEST_SRCDIR}/data/resource/grugs/ne.graphml"
xml="${SHARNESS_TEST_SRCDIR}/data/hwloc-data/001N/exclusive/04-brokers/0.xml"
ne_xml="${SHARNESS_TEST_SRCDIR}/data/hwloc-data/001N/exclusive/ne/0.xml"

#
# test_under_flux is under sharness.d/
#
export FLUX_SCHED_MODULE=none
test_under_flux 1

#
# print only with --debug
#
test_debug '
    echo ${grug} &&
    echo ${jobspec} &&
    echo ${malform}
'

# Unload the resource module
# If module is not loaded, suppress error.
unload_resource() {
    remove_resource 2>rmmod.out
    if test $? -ne 0; then
        grep -q "No such file or directory" rmmod.out && return 0
        cat rmmod.out >&2
        return 1
    fi
}

test_expect_success 'loading resource module with a tiny machine GRUG works' '
    unload_resource &&
    load_resource load-file=${grug} \
load-format=grug prune-filters=ALL:core policy=high
'

test_expect_success 'loading resource module with an XML works' '
    unload_resource &&
    load_resource load-file=${xml} \
load-format=hwloc prune-filters=ALL:core policy=high
'

test_expect_success 'loading resource module with no option works' '
    unload_resource &&
    load_resource prune-filters=ALL:core policy=high
'

test_expect_success 'loading resource module with a nonexistent GRUG fails' '
    unload_resource &&
    flux dmesg -C &&
    load_resource load-file=${ne_grug} load-format=grug \
prune-filters=ALL:core policy=high &&
    test_must_fail flux module stats sched-fluxion-resource &&
    flux dmesg > error1 &&
    test_must_fail grep -i Success error1
'

test_expect_success 'loading resource module with a nonexistent XML fails' '
    unload_resource &&
    flux dmesg -C &&
    load_resource load-file=${ne_xml} load-format=hwloc \
prune-filters=ALL:core policy=high &&
    test_must_fail flux module stats sched-fluxion-resource &&
    flux dmesg > error2 &&
    test_must_fail grep -i Success error2
'

test_expect_success 'loading resource module with incorrect reader fails' '
    unload_resource &&
    flux dmesg -C &&
    load_resource load-file=${xml} load-format=grug \
prune-filters=ALL:core policy=high &&
    test_must_fail flux module stats sched-fluxion-resource &&
    flux dmesg > error3 &&
    grep -i "Invalid argument" error3
'

test_expect_success 'loading resource module with known policies works' '
    unload_resource &&
    load_resource policy=high &&
    remove_resource &&
    load_resource policy=low &&
    remove_resource &&
    load_resource policy=first &&
    remove_resource &&
    load_resource policy=locality
'

test_expect_success 'loading resource module with unknown policies is tolerated' '
    unload_resource &&
    load_resource policy=foo &&
    remove_resource &&
    load_resource policy=bar
'

test_expect_success 'removing resource works' '
    remove_resource
'

test_done


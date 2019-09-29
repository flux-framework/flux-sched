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
test_under_flux 1

#
# print only with --debug
#
test_debug '
    echo ${grug} &&
    echo ${jobspec} &&
    echo ${malform}
'

test_expect_success 'loading resource module with a tiny machine GRUG works' '
    flux module remove resource &&
    flux module load -r 0 resource load-file=${grug} \
load-format=grug prune-filters=ALL:core
'

test_expect_success 'loading resource module with an XML works' '
    flux module remove resource &&
    flux module load -r 0 resource load-file=${xml} \
load-format=hwloc prune-filters=ALL:core
'

test_expect_success 'loading resource module with no option works' '
    flux module remove resource &&
    flux module load -r 0 resource prune-filters=ALL:core
'

test_expect_success 'loading resource module with a nonexistent GRUG fails' '
    flux module remove resource &&
    test_expect_code 1 flux module load -r 0 resource load-file=${ne_grug} \
load-format=grug prune-filters=ALL:core
'

test_expect_success 'loading resource module with a nonexistent XML fails' '
    test_expect_code 1 flux module load -r 0 resource load-file=${ne_xml} \
load-format=hwloc prune-filters=ALL:core
'

test_expect_success 'loading resource module with known policies works' '
    flux module load -r 0 resource policy=high &&
    flux module remove resource &&
    flux module load -r 0 resource policy=low &&
    flux module remove resource &&
    flux module load -r 0 resource policy=locality
'

test_expect_success 'loading resource module with unknown policies is tolerated' '
    flux module remove resource &&
    flux module load -r 0 resource policy=foo &&
    flux module remove resource &&
    flux module load -r 0 resource policy=bar
'

test_expect_success 'removing resource works' '
    flux module remove resource
'

test_done


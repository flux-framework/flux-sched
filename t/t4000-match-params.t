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
    flux module remove fluxion-resource &&
    flux module load -r 0 fluxion-resource grug-conf=${grug} \
prune-filters=ALL:core
'

test_expect_success 'loading resource module with an XML works' '
    flux module remove fluxion-resource &&
    flux module load -r 0 fluxion-resource hwloc-xml=${xml} \
prune-filters=ALL:core
'

test_expect_success 'loading resource module with a GRUG + XML works' '
    flux module remove fluxion-resource &&
    flux module load -r 0 fluxion-resource grug-conf=${grug} \
hwloc-xml=${xml} prune-filters=ALL:core
'

test_expect_success 'loading resource module with no option works' '
    flux module remove fluxion-resource &&
    flux module load -r 0 fluxion-resource prune-filters=ALL:core
'

test_expect_success 'loading resource module with a nonexistent GRUG fails' '
    flux module remove fluxion-resource &&
    test_expect_code 1 flux module load -r 0 fluxion-resource \
grug-conf=${ne_grug} prune-filters=ALL:core
'

test_expect_success 'loading resource module with a nonexistent XML fails' '
    test_expect_code 1 flux module load -r 0 fluxion-resource \
hwloc-xml=${ne_xml} prune-filters=ALL:core
'

test_expect_success 'loading resource module with a nonexistent GRUG+XML fails' '
    test_expect_code 1 flux module load -r 0 fluxion-resource \
grug-conf=${ne_grug} hwloc-xml=${ne_xml} prune-filters=ALL:core
'

test_expect_success 'loading resource module with known policies works' '
    flux module load -r 0 fluxion-resource policy=high &&
    flux module remove fluxion-resource &&
    flux module load -r 0 fluxion-resource policy=low &&
    flux module remove fluxion-resource &&
    flux module load -r 0 fluxion-resource policy=locality
'

test_expect_success 'loading resource module with unknown policies is tolerated' '
    flux module remove fluxion-resource &&
    flux module load -r 0 fluxion-resource policy=foo &&
    flux module remove fluxion-resource &&
    flux module load -r 0 fluxion-resource policy=bar
'

test_expect_success 'removing resource works' '
    flux module remove fluxion-resource
'

test_done


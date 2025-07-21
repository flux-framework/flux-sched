#!/bin/sh
#

test_description='Test fluxion does not block on R w/ no scheduling key
'
. `dirname $0`/sharness.sh

export TEST_UNDER_FLUX_QUORUM=1
export TEST_UNDER_FLUX_START_MODE=leader
export FLUX_RC_EXTRA=${SHARNESS_TEST_SRCDIR}/../etc
unset FLUXION_RESOURCE_RC_NOOP
unset FLUXION_QMANAGER_RC_NOOP
export FLUXION_RESOURCE_OPTIONS="load-allowlist=node,core,gpu load-format=hwloc"

# Comment in the following to generated scheduling key to R
# TEST_UNDER_FLUX_AUGMENT_R=t

test_under_flux 2 system

SCHED_MODULE=$(flux module list | awk '$NF == "sched" {print $1}')

test_expect_success 'fluxion immediately fails to be loaded with hwloc reader' '
    test_debug "echo sched service provided by ${SCHED_MODULE}" &&
    echo $SCHED_MODULE > out &&
    test "$SCHED_MODULE" != "sched-fluxion-qmanager"
'

test_done

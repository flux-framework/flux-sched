#!/bin/sh
#set -x

test_description='Test runlevel support with installed flux

Ensure sched runlevel support works with installed flux.
'
#
# source sharness from the directore where this test
# file resides
#
. $(dirname $0)/sharness.sh

if [ -z $FLUX_SCHED_TEST_INSTALLED ]; then
    skip_all='FLUX_SCHED_TEST_INSTALLED not set, skipping...'
    test_done
fi

#
# test_under_flux is under sharness.d/
#
SIZE=2
unset FLUX_SCHED_NOOP_RC
if [ test ${FLUX_FLUX_CO_INST} = co ]; then
    export FLUX_RC_EXTRA=${FLUX_SCHED_RC_PATH}
fi
test_under_flux ${SIZE}
test_expect_success 'sched: module remove/load works with installed sched' '
	flux module remove sched &&
	flux module load sched
'

test_done

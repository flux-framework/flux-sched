#!/bin/sh
#

test_description='Test system instance with late joining broker

Start up only the leader broker and verify that the system is
functional without a leaf node.  Then start the leaf node
and ensure that it wires up.
'

. `dirname $0`/sharness.sh

export TEST_UNDER_FLUX_QUORUM=0
export TEST_UNDER_FLUX_START_MODE=leader
export FLUX_RC_EXTRA=${SHARNESS_TEST_SRCDIR}/../etc
unset FLUXION_RESOURCE_RC_NOOP
unset FLUXION_QMANAGER_RC_NOOP
TEST_UNDER_FLUX_AUGMENT_R=t

test_under_flux 2 system

startctl="flux python ${SHARNESS_TEST_SRCDIR}/scripts/startctl.py"

SCHED_MODULE=$(flux module list | awk '$NF ~ /^\s*(\w+,)?sched(,\w+)?\s*$/ { print $1 }')

test_expect_success 'sched service provided by fluxion' '
	test_debug "echo sched service provided by ${SCHED_MODULE}" &&
	test "$SCHED_MODULE" = "sched-fluxion-qmanager"
'

test_expect_success 'flux resource list works' '
	flux resource list
'

test_expect_success 'broker.quorum was set to 0 by system test personality' '
	echo 0 >quorum.exp &&
	flux getattr broker.quorum >quorum.out &&
	test_cmp quorum.exp quorum.out
'
test_expect_success HAVE_JQ 'startctl shows rank 1 pids as -1' '
	test $($startctl status | jq -r ".procs[1].pid") = "-1"
'

test_expect_success 'resource list shows one down nodes' '
	echo 1 >down.exp &&
	flux resource list -n -s down -o {nnodes} >down.out &&
	test_cmp down.exp down.out
'

test_expect_success 'resource status shows no drained nodes' '
	echo 0 >drain.exp &&
	flux resource status -s drain -no {nnodes} >drain.out &&
	test_cmp drain.exp drain.out
'

test_expect_success 'flux exec -r 1 fails with EHOSTUNREACH' '
	test_must_fail run_timeout 30 flux exec -r 1 /bin/true 2>unreach.err &&
	grep "$(strerror_symbol EHOSTUNREACH)" unreach.err
'

test_expect_success 'single node job can run with only rank 0 up' '
	run_timeout 30 flux run -n1 /bin/true
'

test_expect_success 'two node job is accepted although it cannot run yet' '
	flux submit -N2 -n2 echo Hello >jobid
'

test_expect_success 'start rank 1' '
	$startctl run 1
'

test_expect_success HAVE_JQ 'startctl shows rank 1 pid not -1' '
	test $($startctl status | jq -r ".procs[1].pid") != "-1"
'

# Updated behavior: the first job is unsatisfiable and fails
test_expect_failure 'two node job can now run' '
	run_timeout 30 flux job attach $(cat jobid)
'

test_expect_success 'two node job is accepted and runs' '
	sleep 5 &&
	flux submit -N2 -n2 echo Hello >jobid
'

test_expect_success 'two node job can now run' '
	run_timeout 30 flux job attach $(cat jobid)
'

test_done

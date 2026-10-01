#!/bin/sh

test_description='Test Python scheduler partial release with housekeeping'

# Append --logfile option if FLUX_TESTS_LOGFILE is set in environment:
test -n "$FLUX_TESTS_LOGFILE" && set -- "$@" --logfile
. $(dirname $0)/sharness.sh

export TEST_UNDER_FLUX_CORES_PER_RANK=2
test_under_flux 4 job

TOTAL_NCORES=8

# Usage: hk_wait_for_running count
hk_wait_for_running () {
	count=0
	while test $(flux housekeeping list -no {id} | wc -l) -ne $1; do
		count=$(($count+1))
		test $count -eq 300 && return 1 # max 300 * 0.1s = 30s
		sleep 0.1
	done
}

###
# Cover fluxion:rv1exec
###

test_expect_success 'reload sched-simple with fluxion:rv1exec pool' '
	flux module reload sched-simple pool-class=fluxion:rv1exec
'

test_expect_success 'configure housekeeping with immediate partial release' '
	flux config load <<-EOF
	[job-manager.housekeeping]
	command = ["sleep", "0"]
	release-after = "0s"
	EOF
'

test_expect_success 'run job across all nodes with housekeeping' '
	flux run -N4 -n4 hostname
'

test_expect_success 'wait for housekeeping to complete' '
	hk_wait_for_running 0
'

test_expect_success 'can run another job after partial release' '
	flux run -N4 hostname
'

test_expect_success 'configure housekeeping that holds one node' '
	flux config load <<-EOF
	[job-manager.housekeeping]
	command = [
	    "sh",
	    "-c",
	    "test \$(flux getattr rank) -eq 0 && sleep 30 || exit 0"
	]
	release-after = "0s"
	EOF
'

test_expect_success 'run job across all nodes' '
	jobid=$(flux submit -N4 hostname) &&
	flux job wait-event --timeout=5.0 $jobid clean
'

test_expect_success 'one housekeeping job should be running' '
	hk_wait_for_running 1
'

test_expect_success 'can run job on 3 free nodes while housekeeping holds 1 node' '
	flux run -N3 hostname
'

test_expect_success 'kill housekeeping to free the held node' '
	flux housekeeping kill --all &&
	hk_wait_for_running 0
'

test_expect_success 'can run job on all 4 nodes after housekeeping completes' '
	flux run -N4 hostname
'

test_expect_success 'kill housekeeping' '
	flux housekeeping kill --all &&
	hk_wait_for_running 0
'

###
# Cover fluxion:jgf
###

test_expect_success 'unload sched-simple for JGF reconfiguration' '
	flux module remove sched-simple
'

test_expect_success 'generate JGF from current R' '
	flux kvs get resource.R | flux ion-R encode >R_jgf.json
'

test_expect_success 'reload resource with JGF-enabled R' '
	flux resource reload R_jgf.json
'

test_expect_success 'load sched-simple with fluxion:jgf pool' '
	flux module load sched-simple pool-class=fluxion:jgf
'

test_expect_success 'configure housekeeping with immediate partial release (JGF)' '
	flux config load <<-EOF
	[job-manager.housekeeping]
	command = ["sleep", "0"]
	release-after = "0s"
	EOF
'

test_expect_success 'run job across all nodes with housekeeping (JGF)' '
	flux run -N4 -n4 hostname
'

test_expect_success 'wait for housekeeping to complete (JGF)' '
	hk_wait_for_running 0
'

test_expect_success 'can run another job after partial release (JGF)' '
	flux run -N4 hostname
'

test_expect_success 'configure housekeeping that holds one node (JGF)' '
	flux config load <<-EOF
	[job-manager.housekeeping]
	command = [
	    "sh",
	    "-c",
	    "test \$(flux getattr rank) -eq 0 && sleep 30 || exit 0"
	]
	release-after = "0s"
	EOF
'

test_expect_success 'run job across all nodes (JGF)' '
	jobid=$(flux submit -N4 hostname) &&
	flux job wait-event --timeout=5.0 $jobid clean
'

test_expect_success 'one housekeeping job should be running (JGF)' '
	hk_wait_for_running 1
'

test_expect_success 'can run job on 3 free nodes while housekeeping holds 1 node (JGF)' '
	flux run -N3 hostname
'

test_expect_success 'kill housekeeping to free the held node (JGF)' '
	flux housekeeping kill --all &&
	hk_wait_for_running 0
'

test_expect_success 'can run job on all 4 nodes after housekeeping completes (JGF)' '
	flux run -N4 hostname
'

test_expect_success 'reload with standard pool again' '
	flux module reload sched-simple
'

test_done

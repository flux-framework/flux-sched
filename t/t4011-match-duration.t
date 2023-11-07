#!/bin/sh

test_description='Test that parent duration is inherited according to RFC14'

. `dirname $0`/sharness.sh

#
# test_under_flux is under sharness.d/
#
test_under_flux 1

export FLUX_URI_RESOLVE_LOCAL=t

# Ensure fluxion modules are loaded under flux-alloc(1)
test_expect_success 'set FLUX_RC_EXTRA so Fluxion modules are loaded under flux-alloc' '
	mkdir rc1.d &&
	cat <<-EOF >rc1.d/rc1-fluxion &&
	flux module unload -f sched-simple
	flux module load sched-fluxion-resource
	flux module load sched-fluxion-qmanager
	EOF
	mkdir rc3.d &&
	cat <<-EOF >rc3.d/rc3-fluxion &&
	flux module remove -f sched-fluxion-qmanager
	flux module remove -f sched-fluxion-resource
	flux module load sched-simple
	EOF
	chmod +x rc1.d/rc1-fluxion rc3.d/rc3-fluxion &&
	export FLUX_RC_EXTRA=$(pwd)
'
test_expect_success 'load fluxion modules in parent instance' '
	flux module remove sched-simple &&
	load_resource &&
	load_qmanager &&
	test_debug "flux dmesg -H | grep version"
'
test_expect_success HAVE_JQ 'parent expiration is inherited when duration=0' '
	cat >get_R.sh <<-EOT &&
	#!/bin/sh
	flux job info \$FLUX_JOB_ID R
	EOT
	chmod +x get_R.sh &&
	jobid=$(flux alloc -n1 -t5m --bg) &&
	test_debug "flux proxy $jobid flux dmesg -H | grep version" &&
	expiration=$(flux job info $jobid R | jq .execution.expiration) &&
        duration=$(flux job info $jobid R \
		| jq ".execution | .expiration - .starttime") &&
	test_debug "echo expiration of alloc job is $expiration duration=$duration" &&
	R1=$(flux proxy $jobid flux run -n1 ./get_R.sh) &&
	exp1=$(echo "$R1" | jq .execution.expiration) &&
	d1=$(echo "$R1" | jq ".execution | .expiration - .starttime") &&
	test_debug "echo expiration of job is $exp1 duration=$d1" &&
	echo $exp1 | jq ". == $expiration" &&
	sleep 1 &&
	R2=$(flux proxy $jobid flux run -n1 ./get_R.sh) &&
	exp2=$(echo "$R2" | jq .execution.expiration) &&
	d2=$(echo "$R2" | jq ".execution | .expiration - .starttime") &&
	test_debug "echo expiration of second job is $exp2 duration=$d2" &&
	echo $exp2 | jq ". == $expiration" &&
	flux shutdown --quiet $jobid
'
test_expect_success 'unload fluxion modules' '
	remove_qmanager &&
	remove_resource
'
test_done

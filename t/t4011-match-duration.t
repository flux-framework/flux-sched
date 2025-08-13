#!/bin/sh

test_description='Test that parent duration is inherited according to RFC14'

. `dirname $0`/sharness.sh

#
# test_under_flux is under sharness.d/
#
test_under_flux 2

flux setattr log-stderr-level 1
export FLUX_URI_RESOLVE_LOCAL=t
unset FLUX_MODPROBE_DISABLE

test -z "$FLUX_RC_USE_MODPROBE" && test_set_prereq OLDRC

dmesg_grep="flux python ${SHARNESS_TEST_SRCDIR}/scripts/dmesg-grep.py"

test_expect_success 'current scheduler is sched-simple' '
	flux module list | grep sched-simple
'

# Ensure fluxion modules are loaded under flux-alloc(1)
test_expect_success OLDRC 'set FLUX_RC_EXTRA so Fluxion modules are loaded under flux-alloc' '
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
test_expect_success 'launching a fluxion instance with sched-simple works' '
	jobid=$(flux alloc -N1 --bg) &&
	flux proxy $jobid flux dmesg -H | grep fluxion &&
	run_timeout 15 flux proxy $jobid flux run hostname &&
	flux shutdown --quiet $jobid &&
	flux job status $jobid
'
test_expect_success 'load fluxion modules in parent instance' '
	flux module remove sched-simple &&
	load_resource &&
	load_qmanager &&
	test_debug "flux dmesg -H | grep version"
'
# Usage: job_manager_get_R ID
# This needs to be a shell script since it will be run under flux-proxy(1):
cat <<'EOF' >job_manager_get_R
#!/bin/sh
flux python -c \
"import flux; \
 payload = {\"id\":$(flux job id $1),\"attrs\":[\"R\"]}; \
 print(flux.Flux().rpc(\"job-manager.getattr\", payload).get_str()) \
"
EOF
chmod +x job_manager_get_R

export PATH=$(pwd):$PATH

subinstance_get_R() {
	flux proxy $1 flux kvs get resource.R
}
subinstance_get_expiration() {
	subinstance_get_R $1 | jq .execution.expiration
}
# Usage: subinstance_get_duration ID JOBID
subinstance_get_job_duration() {
	flux proxy $1 job_manager_get_R $2 |
		jq '.R.execution | .expiration - .starttime'
}
subinstance_get_job_expiration() {
	flux proxy $1 job_manager_get_R $2 | jq '.R.execution.expiration'
}

test_expect_success 'parent expiration is inherited when duration=0' '
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
#  Check if running job updates are supported:
id=$(flux submit sleep inf)
flux update $id duration=1m && test_set_prereq FLUX_UPDATE_RUNNING
flux cancel $id

test_expect_success FLUX_UPDATE_RUNNING \
	'expiration update is detected by subinstance scheduler' '
	id=$(flux alloc --bg -t5m -n2) &&
	exp1=$(subinstance_get_expiration $id) &&
	test_debug "echo instance expiration is $exp1" &&
	id1=$(flux proxy $id flux submit sleep 300) &&
	duration1=$(subinstance_get_job_duration $id $id1) &&
	test_debug "echo initial duration of subinstance job1 is $duration1" &&
	echo $duration1 | jq -e ". <= 300" &&
	test_debug "echo updating duration of alloc job +5m" &&
	flux update $id duration=+5m &&
	test_debug "echo waiting for resource-update event" &&
	flux proxy $id flux kvs eventlog wait-event -vt 30 \
		resource.eventlog resource-update &&
	exp2=$(subinstance_get_expiration $id) &&
	test_debug "echo expiration updated from $exp1 to $exp2" &&
	echo $exp2 | jq -e ". == $exp1 + 300" &&
	flux proxy $id $dmesg_grep -vt 30 \
		\"sched.*resource expiration updated\" &&
	id2=$(flux proxy $id flux submit sleep 300) &&
	duration2=$(subinstance_get_job_duration $id $id2) &&
	test_debug "echo duration of subinstance job2 is $duration2" &&
	echo $duration2 | jq -e ". > 300" &&
	flux update $id duration=0 &&
	exp4=$(subinstance_get_expiration $id) &&
	test_debug "echo duration of subinstance is now $exp4" &&
	duration1=$(subinstance_get_job_duration $id $id1) &&
	test_debug "echo expiration of $id/$id1 is now $duration1" &&
	flux proxy $id flux cancel --all &&
	test_debug "echo a job can still be run in $id1" &&
	flux proxy $id flux run hostname &&
	flux shutdown --quiet $id
'
test_expect_success 'unload fluxion modules' '
	remove_qmanager &&
	remove_resource &&
	flux module load sched-simple
'
test_done

#!/usr/bin/env bash
#

test_description='
'

. `dirname $0`/sharness.sh

export TEST_UNDER_FLUX_QUORUM=1
export TEST_UNDER_FLUX_START_MODE=leader
rpc() {
  flux python -c \
    "import flux, json; print(flux.Flux().rpc(\"$1\").get_str())"
}

test_under_flux 16384 system

test_expect_success 'unload sched-simple' '
	flux module remove -f sched-simple
'

test_expect_success 'update configuration' '
	flux config load <<-'EOF'
	[[resource.config]]
	hosts = "fake[0-10]"
	cores = "0-63"
	gpus = "0-3"

	[[resource.config]]
	hosts = "fake[0-10]"
	properties = ["compute"]

	[sched-fluxion-qmanager]
	queue-policy = "easy"

	[sched-fluxion-resource]
	match-policy = "firstnodex"
	prune-filters = "ALL:core,ALL:gpu,cluster:node,rack:node"
	match-format = "rv1_nosched"
	EOF
'

test_expect_success 'reload resource with monitor-force-up' '
	flux module reload -f resource noverify monitor-force-up
'
test_expect_success 'load fluxion modules' '
	flux module load sched-fluxion-resource &&
	flux module load sched-fluxion-qmanager
'
test_expect_success 'wait for fluxion to be ready' '
  flux python -c \
    "import flux, json; print(flux.Flux().rpc(\"sched.resource-status\").get_str())"
'
test_expect_success 'create 3 jobs with the same constraint, so two are unreservable' '
	flux submit --cc=1-3 --quiet \
	    -N 1 --exclusive \
		--requires="host:fake[5]" \
		--progress --jps \
		--flags=waitable \
		--setattr=exec.test.run_duration=0.01s \
		sleep 0.25
'

test_expect_success 'ensure all three succeeded' '
  flux job wait -av
'
test_expect_success 'unload fluxion' '
	flux module remove sched-fluxion-qmanager &&
	flux module remove sched-fluxion-resource &&
	flux module load sched-simple
'
test_done
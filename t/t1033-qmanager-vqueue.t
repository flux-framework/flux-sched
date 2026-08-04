#!/bin/sh

test_description='Test RFC 33 virtual queue support in qmanager

A queue configured with a "parent" key is a virtual queue: it has no
internal queue of its own in qmanager, and its jobs are scheduled as
part of the parent queue internal queue, in the parent queue priority
order. This test requires a flux-core that accepts a "parent" key in
a queue config table; on a flux-core without virtual queue support,
all virtual-queue-specific tests are skipped.
'

. `dirname $0`/sharness.sh

mkdir -p config

export FLUX_SCHED_MODULE=none
test_under_flux 1 full -o,--config-path=$(pwd)/config


# Probe whether this flux-core accepts a "parent" key in a queue config
# table (i.e., RFC 33 virtual queue support). Note this must fail on any
# flux-core that predates virtual queues, since the "parent" key would
# be left unpacked by the queue config table parser. Reset the config
# afterwards so the probe queues do not linger.
test_expect_success 'probe for RFC 33 virtual queue support' '
	cat >vqueue_probe.toml <<-EOT &&
	[queues.batch]
	requires = ["batch"]
	[queues.expedite]
	parent = "batch"
	EOT
	if flux config load vqueue_probe.toml 2>probe.err; then
		test_set_prereq HAVE_VQUEUES &&
		flux config reload
	else
		grep parent probe.err
	fi
'

# N.B. the "batch" property must be set before fluxion acquires
# resources, since flux resource reload is refused while a scheduler
# holds them.
test_expect_success HAVE_VQUEUES 'set the "batch" property on resources' '
	flux kvs get resource.R >R.orig &&
	flux R set-property batch:0 <R.orig >R &&
	flux resource reload R
'

test_expect_success 'load resource module' '
	load_resource prune-filters=ALL:core subsystems=containment policy=low
'

test_expect_success HAVE_VQUEUES 'loading qmanager with a virtual queue' '
	cat >config/queues.toml <<-EOT &&
	[queues.batch]
	requires = ["batch"]
	[queues.expedite]
	parent = "batch"

	[policy.jobspec.defaults.system]
	queue = "batch"
	EOT
	flux config reload &&
	flux queue start --all &&
	load_qmanager
'

test_expect_success HAVE_VQUEUES 'virtual queue has no internal queue' '
	flux module stats sched-fluxion-qmanager >stats.out &&
	jq -e ".queues.batch" stats.out &&
	test_must_fail jq -e ".queues.expedite" stats.out
'

test_expect_success HAVE_VQUEUES 'job can be submitted to parent queue' '
	flux run -n1 --queue=batch true
'

test_expect_success HAVE_VQUEUES 'job can be submitted to virtual queue' '
	flux run -n1 --queue=expedite true
'

test_expect_success HAVE_VQUEUES 'virtual queue job did not create a phantom queue' '
	flux module stats sched-fluxion-qmanager >stats2.out &&
	test_must_fail jq -e ".queues.expedite" stats2.out
'

test_expect_success HAVE_VQUEUES 'fill the parent queue with a running job' '
	flux resource list -no {ncores} >ncores &&
	flux submit -n $(cat ncores) --queue=batch sleep 3600 >blocker.id &&
	flux job wait-event -t 30 $(cat blocker.id) start
'

# Submit full-size jobs so that at most one can run at a time, with
# urgencies chosen so that neither submit order nor queue name matches
# priority order: the vqueue job B outranks the parent queue job A
# submitted before it, which outranks the vqueue job C submitted after
# it. If parent and vqueue jobs were ordered in separate queues, or in
# submit order, the start order asserted below would differ.
test_expect_success HAVE_VQUEUES 'submit competing jobs to parent and vqueue' '
	flux submit -n $(cat ncores) --queue=batch --urgency=16 \
	    sleep 3600 >jobA.id &&
	flux submit -n $(cat ncores) --queue=expedite --urgency=17 \
	    sleep 3600 >jobB.id &&
	flux submit -n $(cat ncores) --queue=expedite --urgency=15 \
	    sleep 3600 >jobC.id
'

test_expect_success HAVE_VQUEUES 'parent and vqueue jobs pend in one internal queue' '
	flux module stats sched-fluxion-qmanager >stats3.out &&
	jq -r ".queues.batch.pending_queues | .pending + .pending_provisional | .[]" \
	    stats3.out >pending.out &&
	grep $(flux job id --to=f58plain $(cat jobA.id)) pending.out &&
	grep $(flux job id --to=f58plain $(cat jobB.id)) pending.out &&
	grep $(flux job id --to=f58plain $(cat jobC.id)) pending.out
'

# Each job needs every core and never exits on its own, so "started"
# means it won the priority comparison among all pending jobs: were the
# order wrong, the awaited job could never start and wait-event would
# time out.
test_expect_success HAVE_VQUEUES 'jobs run in priority order across parent and vqueue' '
	flux cancel $(cat blocker.id) &&
	flux job wait-event -t 30 $(cat jobB.id) start &&
	flux cancel $(cat jobB.id) &&
	flux job wait-event -t 30 $(cat jobA.id) start &&
	flux cancel $(cat jobA.id) &&
	flux job wait-event -t 30 $(cat jobC.id) start &&
	flux cancel $(cat jobC.id) &&
	flux job wait-event -t 30 $(cat jobC.id) clean
'

test_expect_success HAVE_VQUEUES 'unload qmanager and deconfigure queues' '
	remove_qmanager &&
	cp /dev/null config/queues.toml &&
	flux config reload
'

test_expect_success 'cleanup active jobs' '
	cleanup_active_jobs
'

test_expect_success 'removing resource and qmanager modules' '
	test_might_fail remove_qmanager &&
	remove_resource
'

test_done

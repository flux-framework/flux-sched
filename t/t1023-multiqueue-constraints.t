#!/bin/sh

test_description='Test queues with non-overlapping constraints

Partitioned node resources with batch and debug queues is expected
to be a common configuration.
'

. `dirname $0`/sharness.sh

export FLUX_SCHED_MODULE=none

mkdir -p conf.d

test_under_flux 4 full -o,--config-path=$(pwd)/conf.d

# Note: updating test to specify ALL:core. The default (ALL:core,ALL:node)
# causes requests of drained resources to be unsatisfiable rather than the
# expected behavior of blocking.
test_expect_success 'load queue configuration' '
	cat >conf.d/conf.toml <<-EOT &&
	[queues.debug]
	requires = ["debug"]
	[queues.batch]
	requires = ["batch"]

	[sched-fluxion-resource]
	match-policy = "lonodex"
	match-format = "rv1_nosched"
	prune-filters = "ALL:core"
	EOT
	flux config reload &&
	flux queue start --all
'

test_expect_success 'reload resource with properties set' '
	flux kvs get resource.R >R.orig &&
	flux R set-property debug:0 batch:1-3 bigmem:0-1 tinymem:2 <R.orig >R &&
	flux resource reload R
'
test_expect_success 'load fluxion modules' '
	load_resource &&
	load_qmanager
'
test_expect_success 'run a job in each queue' '
	flux run --queue=debug /bin/true &&
	flux run --queue=batch /bin/true
'
test_expect_success 'occupy resources in first queue' '
	flux submit --queue=debug sleep 300 >job1.out
'
test_expect_success 'run a job in second queue' '
	flux run --queue=batch /bin/true
'
test_expect_success 'create a backlog in first queue' '
	flux submit --queue=debug /bin/true
'
test_expect_success 'run a job in second queue' '
	flux run --queue=batch /bin/true
'
test_expect_success 'occupy resources in second queue' '
	flux submit --queue=batch -N3 sleep 300 >job2.out
'
test_expect_success 'cancel job occupying first queue' '
	flux cancel $(cat job1.out)
'
test_expect_success 'run a job in first queue' '
	flux run --queue=debug /bin/true
'
test_expect_success 'create a backlog in second queue' '
	flux submit --queue=batch /bin/true
'
test_expect_success 'run a job in first queue' '
	flux run --queue=debug /bin/true
'
test_expect_success 'cancel job occupying second queue' '
	flux cancel $(cat job2.out)
'
test_expect_success 'run a job in each queue' '
	flux run --queue=debug /bin/true &&
	flux run --queue=batch /bin/true
'
test_expect_success 'a job with redundant constraint works' '
	flux run --queue=debug --requires=debug /bin/true
'
test_expect_success 'a job with unsatisfiable constraint fails' '
	test_must_fail flux run --queue=debug --requires=tinymem /bin/true
'
test_expect_success 'a job with unsatisfiable node count fails' '
	test_must_fail flux run --queue=debug -N2 /bin/true
'
test_expect_success 'a job with satisfiable constraint works' '
	flux run --queue=batch --requires=tinymem /bin/true
'
test_expect_success 'a job with multiple constraints works in both queues' '
	flux run --queue=debug --requires=bigmem /bin/true &&
	flux run --queue=batch --requires=bigmem /bin/true
'
test_expect_success 'stop queues' '
	flux queue stop --all
'
test_expect_success 'consume wait status so far' '
	test_might_fail flux job wait --all
'
test_expect_success 'submit a held job to the first queue' '
	flux submit --flags=waitable \
	    --queue=debug --urgency=hold /bin/true >job3.out
'
test_expect_success 'submit a diverse set of jobs to both queues' '
	flux submit --flags=waitable \
	    --queue=debug --requires=bigmem /bin/true &&
	flux submit --flags=waitable --cc=1-10 -N2 \
	    --queue=batch /bin/true &&
	flux submit --flags=waitable \
	    --queue=debug --requires=bigmem --urgency=31 /bin/true &&
	flux submit --flags=waitable \
	    --queue=batch --requires=bigmem /bin/true &&
	flux submit --flags=waitable --cc=1-10 \
	    --queue=debug /bin/true &&
	flux submit --flags=waitable \
	    --queue=debug --requires=bigmem --urgency=1 /bin/true &&
	flux submit --flags=waitable \
	    --queue=debug /bin/true
'
test_expect_success 'drain a node' '
	flux resource drain 1 testing...
'
test_expect_success 'start queues - bunch of alloc requests arrive at once' '
	flux queue start --all
'
test_expect_success 'submit some additional work on top of that' '
	flux submit --flags=waitable --cc=1-10 \
	    --queue=batch /bin/true &&
	flux submit --flags=waitable --cc=1-10 \
	    --queue=debug /bin/true
'
test_expect_success 'undrain the node' '
	flux resource undrain 1
'
test_expect_success 'unhold the job' '
	flux job urgency $(cat job3.out) default
'
test_expect_success 'wait for all jobs to complete successfully' '
	flux job wait --all
'
test_expect_success 'cleanup active jobs' '
        cleanup_active_jobs
'
test_expect_success 'removing resource and qmanager modules' '
	remove_qmanager &&
	remove_resource
'

test_done


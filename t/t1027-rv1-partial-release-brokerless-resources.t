#!/bin/sh
#
test_description='Ensure fluxion cancels resources that are not associated with a broker rank'

# See, e.g. issue #1284

. `dirname $0`/sharness.sh

if test_have_prereq ASAN; then
    skip_all='skipping issues tests under AddressSanitizer'
    test_done
fi
SIZE=1
test_under_flux ${SIZE}


test_expect_success 'an ssd jobspec can be allocated' '
	flux module remove sched-simple &&
	flux module remove resource &&
	flux config load <<EOF &&
[resource]
noverify = true
norestrict = true
path="${SHARNESS_TEST_SRCDIR}/data/resource/jgfs/issue1284.json"
EOF
	flux module load resource monitor-force-up &&
	flux module load sched-fluxion-resource &&
	flux module load sched-fluxion-qmanager &&
	flux module unload job-list &&
	flux module list &&
	flux queue start --all --quiet &&
	flux resource list &&
	flux resource status &&
	jobid=$(flux job submit --flags=waitable \
			${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/issues/issue1284.json) &&
	flux job wait-event -vt5 ${jobid} alloc &&
	flux job wait-event -vt5 ${jobid} clean
'

test_expect_success 'single-node non-ssd jobspecs can be allocated' '
	jobid=$(flux submit -n1 -N1 true) &&
	flux job wait-event -vt5 ${jobid} alloc &&
	flux job wait-event -vt5 ${jobid} clean
'

test_expect_success 'a second ssd jobspec can be allocated' '
	jobid=$(flux job submit --flags=waitable \
			${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/issues/issue1284.json) &&
	flux job wait-event -vt15 ${jobid} alloc &&
	flux job wait-event -vt5 ${jobid} clean
'

test_expect_success 'single-node non-ssd jobspecs can still be allocated' '
	(flux cancel ${jobid} || true) &&
	jobid=$(flux submit -n1 -N1 true) &&
	flux job wait-event -vt5 ${jobid} alloc &&
	flux job wait-event -vt5 ${jobid} clean
'

test_expect_success 'reload fluxion and resource module' '
	flux cancel --all &&
	flux module remove sched-fluxion-qmanager &&
	flux module remove sched-fluxion-resource &&
	flux module reload resource &&
	flux module load sched-fluxion-resource &&
	flux module load sched-fluxion-qmanager &&
	flux module list &&
	flux queue start --all --quiet
'

test_expect_success 'an ssd jobspec with no slot can be allocated' '
	flux resource list &&
	flux resource status &&
	jobid=$(flux job submit --flags=waitable \
			${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/issues/issue1284-noslot.json) &&
	flux job wait-event -vt5 ${jobid} alloc &&
	flux job wait-event -vt5 ${jobid} clean
'

test_expect_success 'single-node non-ssd jobspecs can be allocated' '
	jobid=$(flux submit -n1 -N1 true) &&
	flux job wait-event -vt5 ${jobid} alloc &&
	flux job wait-event -vt5 ${jobid} clean
'

test_expect_success 'a second ssd jobspec with no slot can be allocated' '
	jobid=$(flux job submit --flags=waitable \
			${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/issues/issue1284-noslot.json) &&
	flux job wait-event -vt15 ${jobid} alloc &&
	flux job wait-event -vt5 ${jobid} clean
'

test_expect_success 'single-node non-ssd jobspecs can still be allocated' '
	(flux cancel ${jobid} || true) &&
	jobid=$(flux submit -n1 -N1 true) &&
	flux job wait-event -vt5 ${jobid} alloc &&
	flux job wait-event -vt5 ${jobid} clean
'

test_expect_success 'reload fluxion and resource module' '
	flux module remove sched-fluxion-qmanager &&
	flux module remove sched-fluxion-resource
'

test_expect_success 're-run tests with ssd pruning filters' '
	flux module remove resource &&
	flux config load <<EOF &&
[resource]
noverify = true
norestrict = true
path="${SHARNESS_TEST_SRCDIR}/data/resource/jgfs/issue1284.json"
[sched-fluxion-resource]
prune-filters = "ALL:core,ALL:ssd"
EOF
	flux module load resource monitor-force-up &&
	flux module load sched-fluxion-resource &&
	flux module load sched-fluxion-qmanager &&
	flux module list &&
	flux queue start --all --quiet &&
	flux resource list &&
	flux resource status &&
	jobid=$(flux job submit --flags=waitable \
			${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/issues/issue1284.json) &&
	flux job wait-event -vt5 ${jobid} alloc &&
	flux job wait-event -vt5 ${jobid} clean
'

test_expect_success 'single-node non-ssd jobspecs can be allocated with ssd pruning filters' '
	jobid=$(flux submit -n1 -N1 true) &&
	flux job wait-event -vt5 ${jobid} alloc &&
	flux job wait-event -vt5 ${jobid} clean
'

test_expect_success 'a second ssd jobspec can be allocated with ssd pruning filters' '
	jobid=$(flux job submit --flags=waitable \
			${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/issues/issue1284.json) &&
	flux job wait-event -vt15 ${jobid} alloc &&
	flux job wait-event -vt5 ${jobid} clean
'

test_expect_success 'single-node non-ssd jobspecs can still be allocated with ssd pruning filters' '
	(flux cancel ${jobid} || true) &&
	jobid=$(flux submit -n1 -N1 true) &&
	flux job wait-event -vt5 ${jobid} alloc &&
	flux job wait-event -vt5 ${jobid} clean
'

test_expect_success 'reload fluxion and resource module with ssd pruning filters' '
	flux cancel --all &&
	flux module remove sched-fluxion-qmanager &&
	flux module remove sched-fluxion-resource &&
	flux module reload resource &&
	flux module load sched-fluxion-resource &&
	flux module load sched-fluxion-qmanager &&
	flux module list &&
	flux queue start --all --quiet
'

test_expect_success 'an ssd jobspec with no slot can be allocated with ssd pruning filters' '
	flux resource list &&
	flux resource status &&
	jobid=$(flux job submit --flags=waitable \
			${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/issues/issue1284-noslot.json) &&
	flux job wait-event -vt5 ${jobid} alloc &&
	flux job wait-event -vt5 ${jobid} clean
'

test_expect_success 'single-node non-ssd jobspecs can be allocated with ssd pruning filters' '
	jobid=$(flux submit -n1 -N1 true) &&
	flux job wait-event -vt5 ${jobid} alloc &&
	flux job wait-event -vt5 ${jobid} clean
'

test_expect_success 'a second ssd jobspec with no slot can be allocated with ssd pruning filters' '
	jobid=$(flux job submit --flags=waitable \
			${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/issues/issue1284-noslot.json) &&
	flux job wait-event -vt15 ${jobid} alloc &&
	flux job wait-event -vt5 ${jobid} clean
'

test_expect_success 'single-node non-ssd jobspecs can still be allocated with ssd pruning filters' '
	(flux cancel ${jobid} || true) &&
	jobid=$(flux submit -n1 -N1 true) &&
	flux job wait-event -vt5 ${jobid} alloc &&
	flux job wait-event -vt5 ${jobid} clean
'

test_expect_success 'reload fluxion and resource module with ssd pruning filters' '
	flux module remove sched-fluxion-qmanager &&
	flux module remove sched-fluxion-resource
'

test_done

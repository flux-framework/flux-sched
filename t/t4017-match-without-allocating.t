#!/bin/sh
#set -x

# Adapted from t4001-match-allocate.t

test_description='Test the basic functionality of resource-match-without-allocating'

. `dirname $0`/sharness.sh

grug="${SHARNESS_TEST_SRCDIR}/data/resource/grugs/tiny.graphml"
jobspec="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/basics/test001.yaml"
malform="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/basics/bad.yaml"
duration_too_large="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/duration/test001.yaml"
duration_negative="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/duration/test002.yaml"
commands="${SHARNESS_TEST_SRCDIR}/data/resource/commands/match_extend/cmds01.in"
query="../../resource/utilities/resource-query"

test_under_flux 1

test_debug '
    echo ${grug} &&
    echo ${jobspec} &&
    echo ${malform}
'

test_expect_success 'loading resource module with a tiny machine config works' '
    load_resource \
load-file=${grug} prune-filters=ALL:core \
load-format=grug subsystems=containment policy=high
'

# Ignore starttime and expiration because they can change during the test
# Match-without-allocating must choose resources deterministically
test_expect_success 'match-without-allocating works with a 1-node, 1-socket jobspec' '
    flux ion-resource match without_allocating ${jobspec} | grep -o "{.*starttime" > match1.out &&
    flux ion-resource match without_allocating ${jobspec} | grep -o "{.*starttime" > match2.out &&
    flux ion-resource match without_allocating ${jobspec} | grep -o "{.*starttime" > match3.out &&
    flux ion-resource match without_allocating ${jobspec} | grep -o "{.*starttime" > match4.out &&
    flux ion-resource match without_allocating ${jobspec} | grep -o "{.*starttime" > match5.out &&
    diff match1.out match2.out &&
    diff match2.out match3.out &&
    diff match3.out match4.out &&
    diff match4.out match5.out
'

test_expect_success 'match-allocate works (all resources)' '
    flux ion-resource match allocate ${jobspec} &&
    flux ion-resource match allocate ${jobspec} &&
    flux ion-resource match allocate ${jobspec} &&
    flux ion-resource match allocate ${jobspec}
'

test_expect_success 'match-allocate fails when all resources are allocated' '
    test_expect_code 16 flux ion-resource match allocate ${jobspec}
'

# match-without-allocating will search ahead in time
test_expect_success 'match-without-allocating succeeds when all resources are allocated' '
    flux ion-resource match without_allocating ${jobspec}
'

# match-without-allocating-extend will search ahead in time
test_expect_success 'match-without-allocating-extend succeeds when all resources are allocated' '
    flux ion-resource match without_allocating_extend ${jobspec}
'

test_expect_success 'JOBID of match_without_allocating request get tracked as MATCHED' '
    flux ion-resource info 0 | grep MATCHED
'

test_expect_success 'JOBID of match_without_allocating_extend request get tracked as MATCHED' '
    flux ion-resource info 10 | grep MATCHED
'

test_expect_success 'detecting of a non-existent jobspec file works' '
    test_expect_code 3 flux ion-resource match without_allocating foo
'

test_expect_success 'handling of a malformed jobspec works' '
    test_expect_code 2 flux ion-resource match without_allocating ${malform}
'

test_expect_success 'handling of an invalid resource type works' '
    test_expect_code 1 flux ion-resource match without_allocating \
        "${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/basics/bad_res_type.yaml"
'

test_expect_success 'invalid duration is caught for match_without_allocating' '
    test_must_fail flux ion-resource match without_allocating ${duration_too_large} &&
    test_must_fail flux ion-resource match without_allocating ${duration_negative}
'

test_expect_success 'invalid duration is caught for match_without_allocating_extend' '
    test_must_fail flux ion-resource match without_allocating_extend ${duration_too_large} &&
    test_must_fail flux ion-resource match without_allocating_extend ${duration_negative}
'

test_expect_success 'removing resource works' '
    remove_resource
'

test_expect_success 'loading resource module with policy=low works' '
    load_resource \
load-file=${grug} prune-filters=ALL:core \
load-format=grug subsystems=containment policy=low
'

test_expect_success 'match-without-allocating matches different resources with policy=low' '
    flux ion-resource match without_allocating ${jobspec} | grep -o "{.*starttime" > matchlow.out &&
    test_expect_code 1 diff match1.out matchlow.out
'

# There are no future jobs, so the matched resources should be valid until graph end
test_expect_success 'match-wo-alloc extends returned resources past equivalent match-allocate' '
    flux ion-resource match without_allocating_extend ${jobspec} |\
grep -o "{.*" | jq ".execution.expiration" > matchwoallocex.out &&
    sleep 1 && flux ion-resource match allocate ${jobspec} |\
grep -o "{.*" | jq ".execution.expiration" > matchallocate1.out &&
    sleep 1 && flux ion-resource match allocate ${jobspec} |\
grep -o "{.*" | jq ".execution.expiration" > matchallocate2.out &&
    python3 -c "exit(not($(cat matchwoallocex.out) > $(cat matchallocate1.out)))" && # Succeed if >
    python3 -c "exit(not($(cat matchallocate1.out) < $(cat matchallocate2.out)))"  # Succeed if <
'

test_expect_success 'removing resource works' '
    remove_resource
'

test_expect_success 'resource-query can perform match-without-allocating' '
	${query} -L ${grug} -S CA -t rq.out <<-'EOF' &&
	match without_allocating ${jobspec}
	match allocate ${jobspec}
	match allocate ${jobspec}
	match allocate ${jobspec}
	match allocate ${jobspec}
	match without_allocating ${jobspec}
    quit
	EOF
    test $(grep MATCHED <rq.out | wc -l) -eq 2 &&
    test $(grep ALLOCATED <rq.out | wc -l) -eq 4 &&
    grep "SCHEDULED AT" rq.out | sed -ne "6p" | grep 3600
'

# Verify that the match_without_allocating got extended until the second reservation.
# i.e. the expiration of the second reservation minus the expiration of the match_wo_alloc
# equals the total duration of the second reservation.
test_expect_success 'match-wo-alloc extends returned resource until the next allocation' '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${commands} > cmds001 &&
    ${query} -L${grug} -f grug -F rv1 -S CA -P low -t rq.out < cmds001 &&
    cat rq.out | grep "{.*" | jq ".execution.expiration" |\
python3 ${SHARNESS_TEST_SRCDIR}/python/t4016-match-got-extended.py
'

test_done

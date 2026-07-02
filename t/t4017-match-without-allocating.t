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

test_expect_success 'match_without_allocating requests do not get tracked (1st would have id 0)' '
    test_expect_code 3 flux ion-resource info 0
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

test_expect_success 'match-without-allocating fails when all resources are allocated' '
    test_expect_code 16 flux ion-resource match without_allocating ${jobspec}
'

test_expect_success 'match-without-allocating-future succeeds when all resources are allocated' '
    flux ion-resource match without_allocating_future ${jobspec} | grep MATCHED
'

# Check for invalid input handling in both W.A. and W.A.F.
for match_op in "without_allocating" "without_allocating_future"; do
    test_expect_success 'detecting of a non-existent jobspec file works' '
        test_expect_code 3 flux ion-resource match '${match_op}' foo
    '
    
    test_expect_success 'handling of a malformed jobspec works' '
        test_expect_code 2 flux ion-resource match '${match_op}' ${malform}
    '
    
    test_expect_success 'handling of an invalid resource type works' '
        test_expect_code 1 flux ion-resource match '${match_op}' \
            "${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/basics/bad_res_type.yaml"
    '
    
    test_expect_success 'invalid duration is caught' '
        test_must_fail flux ion-resource match '${match_op}' ${duration_too_large} &&
        test_must_fail flux ion-resource match '${match_op}' ${duration_negative}
    '
done

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

test_expect_success 'removing resource works' '
    remove_resource
'

test_expect_success 'resource-query can perform match-without-allocating[_future]' '
	${query} -L ${grug} -S CA -t rq.out <<-'EOF' &&
	match without_allocating ${jobspec}
	match allocate ${jobspec}
	match allocate ${jobspec}
	match allocate ${jobspec}
	match allocate ${jobspec}
	match without_allocating ${jobspec}
	match without_allocating_future ${jobspec}
    quit
	EOF
    test $(grep MATCHED <rq.out | wc -l) -eq 2 &&
    test $(grep ALLOCATED <rq.out | wc -l) -eq 4 &&
    test $(grep "No matching resources found" <rq.out | wc -l) -eq 1 &&
    grep "SCHEDULED AT" rq.out | sed -ne "6p" | grep 3600
'

test_done

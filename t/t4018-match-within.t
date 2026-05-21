#!/bin/sh

test_description='Test the functionality of match --within FSD'

. `dirname $0`/sharness.sh

grug="${SHARNESS_TEST_SRCDIR}/data/resource/grugs/tiny.graphml"
jobspec="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/basics/test001.yaml"
within_3579_js="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/user_attributes/within_duration.yaml"
within_infinity_js="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/user_attributes/within_infinity.yaml"
query="../../resource/utilities/resource-query"

test_under_flux 1


test_expect_success 'loading resource module with a tiny machine config works' '
    load_resource \
load-file=${grug} prune-filters=ALL:core \
load-format=grug subsystems=containment policy=high
'

test_expect_success 'ion-resource successfully matches within 0 units' '
    flux ion-resource match allocate --within 0 ${jobspec} &&
    flux ion-resource match allocate_with_satisfiability --within 0d ${jobspec} &&
    flux ion-resource match allocate_orelse_reserve ${jobspec} --within 0 &&
    flux ion-resource match without_allocating_future ${jobspec} --within 0d
'

# A negative value should be interpreted as infinity
test_expect_success 'ion-resource successfully matches within negative units' '
    flux ion-resource match without_allocating_future ${jobspec} --within -1 &&
    flux ion-resource match without_allocating_future ${jobspec} --within \"-1d\"
'

test_expect_success 'ion-resource fails to match within 3579 units (first avail at 3600)' '
    # Allocate the last open slot
    flux ion-resource match allocate ${jobspec} &&
    # This can accidentally succeed if the execution is delayed, hence the 20 second buffer.
    # resource-query covers this edge case more easily because it does not use real time;
    # see reapi_cli_t::match_allocate to confirm.
    test_expect_code 16 flux ion-resource match allocate --within 3579 ${jobspec} &&
    test_expect_code 16 flux ion-resource match allocate_with_satisfiability -w 3579 ${jobspec} &&
    test_expect_code 16 flux ion-resource match allocate_orelse_reserve ${jobspec} --within 3579 &&
    test_expect_code 16 flux ion-resource match without_allocating_future ${jobspec} -w 3579
'

test_expect_success 'ion-resource fails to match when the match-within user attribute = 3579' '
    # All slots are already allocated
    test_expect_code 16 flux ion-resource match allocate ${within_3579_js} &&
    test_expect_code 16 flux ion-resource match allocate_with_satisfiability ${within_3579_js} &&
    test_expect_code 16 flux ion-resource match allocate_orelse_reserve ${within_3579_js} &&
    test_expect_code 16 flux ion-resource match without_allocating_future ${within_3579_js}
'

test_expect_success 'ion-resource fails to match within 3579 units (overridden jobspec usrattr)' '
    # All slots are already allocated
    test_expect_code 16 \
        flux ion-resource match allocate ${within_infinity_js} --within=3579 &&
    test_expect_code 16 \
        flux ion-resource match allocate_with_satisfiability ${within_infinity_js} -w=3579 &&
    test_expect_code 16 \
        flux ion-resource match allocate_orelse_reserve ${within_infinity_js} --within 3579 &&
    test_expect_code 16 \
        flux ion-resource match without_allocating_future ${within_infinity_js} -w 3579
'

test_expect_success 'ion-resource successfully matches within 3600 units (first avail at 3600)' '
    flux ion-resource match allocate_orelse_reserve ${jobspec} --within=3600 | grep RESERVED &&
    flux ion-resource match without_allocating_future ${jobspec} -w=3600 | grep MATCHED
'

test_expect_success 'ion-resource matches within 3600 units (overriding the jobspec user attr)' '
    flux ion-resource match allocate_orelse_reserve ${within_3579_js} -w=3600 | grep RESERVED &&
    flux ion-resource match without_allocating_future ${within_3579_js} -w=3600 | grep MATCHED
'

test_expect_success 'ion-resource matches eventually (w/wo jobspec user attr match_within=-1)' '
    flux ion-resource match allocate_orelse_reserve ${jobspec} &&
    flux ion-resource match without_allocating_future ${jobspec} &&
    flux ion-resource match allocate_orelse_reserve ${within_infinity_js} &&
    flux ion-resource match without_allocating_future ${within_infinity_js}
'

test_expect_success 'removing resource works' '
    remove_resource
'

test_expect_success 'resource-query successfully matches within 0 units' '
	${query} -L ${grug} -S CA -t rq.out <<-'EOF' &&
	match allocate ${jobspec} 0
	match allocate_with_satisfiability ${jobspec} 0
	match allocate_orelse_reserve ${jobspec} 0
	match without_allocating_future ${jobspec} 0
    quit
	EOF
    cat rq.out &&
    test $(grep MATCHED <rq.out | wc -l) -eq 1 &&
    test $(grep ALLOCATED <rq.out | wc -l) -eq 3
'

test_expect_success 'resource-query fails to match within 3599 units (first avail at 3600)' '
	${query} -L ${grug} -S CA -t rq.out <<-'EOF' &&
	match allocate ${jobspec}
	match allocate ${jobspec}
	match allocate ${jobspec}
	match allocate ${jobspec}
	match allocate_orelse_reserve ${jobspec} 3599
	match without_allocating_future ${jobspec} 3599
    quit
	EOF
    cat rq.out &&
    test $(grep MATCHED <rq.out | wc -l) -eq 0 &&
    test $(grep RESERVED <rq.out | wc -l) -eq 0 &&
    test $(grep ALLOCATED <rq.out | wc -l) -eq 4
'

test_expect_success 'resource-query successfully matches within 3600 units (first avail at 3600)' '
	${query} -L ${grug} -S CA -t rq.out <<-'EOF' &&
	match allocate ${jobspec}
	match allocate ${jobspec}
	match allocate ${jobspec}
	match allocate ${jobspec}
	match allocate_orelse_reserve ${jobspec} 3600
	match without_allocating_future ${jobspec} 3600
    quit
	EOF
    cat rq.out &&
    test $(grep MATCHED <rq.out | wc -l) -eq 1 &&
    test $(grep RESERVED <rq.out | wc -l) -eq 1 &&
    test $(grep ALLOCATED <rq.out | wc -l) -eq 4
'

test_done

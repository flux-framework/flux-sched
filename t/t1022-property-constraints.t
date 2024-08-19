#!/bin/sh

test_description='Test constraints complementing t3034-resource-pconstraints.t'

. `dirname $0`/sharness.sh

if flux python -m flux.constraint.parser >/dev/null 2>&1; then
   test_set_prereq RFC35_SYNTAX
fi

export FLUX_SCHED_MODULE=none

test_under_flux 4

test_expect_success 'reload resource with properties set' '
	flux kvs put resource.R="$(flux kvs get resource.R | \
		flux R set-property foo:0-1 bar:1-2 baz:3)" &&
	flux module reload resource
'

test_expect_success 'load fluxion' '
	load_resource policy=hinodex &&
	load_qmanager
'
test_expect_success 'reload ingest without validator' '
	flux module reload -f job-ingest disable-validator
'
test_expect_success 'simple property requirement works' '
	JOBID1=$(flux submit --requires="baz" hostname) &&
	flux job wait-event -t 10 ${JOBID1} clean &&
	flux job info ${JOBID1} R > JOBID1.R &&
	RANK1=$(cat JOBID1.R | jq -r ".execution.R_lite[0].rank") &&
	LENGTH1=$(cat JOBID1.R | jq -r ".execution.properties" | jq length) &&
	PROP_RANK1=$(cat JOBID1.R | jq -r ".execution.properties.baz") &&
	test "${RANK1}" = "3" -a "${LENGTH1}" = "1" -a "${PROP_RANK1}" = "3"
'

test_expect_success 'multiple properties requirement works' '
	JOBID2=$(flux submit --requires="foo" --requires="bar" hostname) &&
	flux job wait-event -t 10 ${JOBID2} clean &&
	flux job info ${JOBID2} R > JOBID2.R &&
	RANK2=$(cat JOBID2.R | jq -r ".execution.R_lite[0].rank") &&
	LENGTH2=$(cat JOBID2.R | jq -r ".execution.properties" | jq length) &&
	PROP_RANK2=$(cat JOBID2.R | jq -r ".execution.properties.foo") &&
	PROP_RANK2B=$(cat JOBID2.R | jq -r ".execution.properties.bar") &&
	test "${RANK2}" = "1" -a "${LENGTH2}" = "2" &&
	test "${PROP_RANK2}" = "1" -a "${PROP_RANK2B}" = "1"
'

test_expect_success '^property requirement works' '
	JOBID3=$(flux submit --requires="^foo" --requires="^bar" hostname) &&
	flux job wait-event -t 10 ${JOBID3} clean &&
	flux job info ${JOBID3} R > JOBID3.R &&
	RANK3=$(cat JOBID3.R | jq -r ".execution.R_lite[0].rank") &&
	LENGTH3=$(cat JOBID3.R | jq -r ".execution.properties" | jq length) &&
	PROP_RANK3=$(cat JOBID3.R | jq -r ".execution.properties.baz") &&
	test "${RANK3}" = "3" -a "${LENGTH3}" = "1" -a "${PROP_RANK3}" = "3"
'
test_expect_success 'nonexistient property works' '
	JOBID4=$(flux submit --requires="yyy" hostname) &&
	flux job wait-event -t 10 ${JOBID4} clean &&
	flux job eventlog ${JOBID4} > evlog4 2>&1 &&
	grep -i "unsatisfiable" evlog4
'
test_expect_success 'invalid property works' '
	JOBID5=$(flux submit --requires="f^oo" hostname) &&
	flux job wait-event -t 10 ${JOBID5} clean &&
	flux job eventlog ${JOBID5} > evlog5 &&
	grep "f^oo is invalid" evlog5
'
test_expect_success 'reload ingest with feasibility' '
	flux module reload -f job-ingest validator-plugins=feasibility
'
test_expect_success RFC35_SYNTAX 'complex constraints work' '
	flux run --requires="foo&bar" flux getattr rank > c1.rank &&
	test_debug "cat c1.rank" &&
	test $(cat c1.rank) -eq 1 &&
	flux run -N3 --requires="foo|baz" flux getattr rank \
		| sort > c2.ranks &&
	cat <<-EOF >c2.expected &&
	0
	1
	3
	EOF
	test_cmp c2.expected c2.ranks &&
	flux run --requires="foo and not bar" flux getattr rank \
		> c3.rank &&
	test_debug "cat c3.rank" &&
	test $(cat c3.rank) -eq 0
'

test_expect_success RFC35_SYNTAX 'invalid complex constraint fails' '
	test_must_fail flux run --requires="foo|bar badop:meep" hostname \
		> badop.out 2>&1 &&
	grep -i "unknown constraint operator: badop" badop.out
'

test_expect_success RFC35_SYNTAX 'ranks constraint works' '
	flux run --requires=rank:2 flux getattr rank > rank.out &&
	test_debug "cat rank.out" &&
	test $(cat rank.out) -eq 2 &&
	flux run -N2 --requires=rank:2-3 flux getattr rank \
		| sort > rank2-3.out &&
	test_debug "cat rank2-3.out" &&
	cat <<-EOF >rank2-3.expected &&
	2
	3
	EOF
	test_cmp rank2-3.expected rank2-3.out
'

test_expect_success RFC35_SYNTAX 'invalid rank constraint fails' '
	test_must_fail flux run --requires=ranks:5-4 hostname
'

test_expect_success RFC35_SYNTAX 'unknown rank returns unsatisfiable' '
	test_must_fail flux run --requires=ranks:999 hostname \
	 >unknown-rank.out 2>&1 &&
	grep -i unsatisfiable unknown-rank.out
'

test_expect_success RFC35_SYNTAX 'hostlist constraint works' '
	flux run --requires=host:$(hostname) hostname
'

test_expect_success RFC35_SYNTAX 'invalid hostlist constraint fails' '
	test_must_fail flux run --requires=host:foo\[  hostname
'

test_expect_success RFC35_SYNTAX 'unknown host returns unsatisfiable' '
	if test "$(hostname)" != "host:xyz123"; then
		test_must_fail flux run --requires=host:xyz123 hostname \
		 >host-unknown.out 2>&1 &&
		grep -i unsatisfiable host-unknown.out
	fi
'
test_expect_success 'removing resource and qmanager modules' '
	remove_qmanager &&
	remove_resource
'

test_done


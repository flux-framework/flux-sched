#!/bin/sh

test_description='Test pconstraints complementing t3034-resource-pconstraints.t'

. `dirname $0`/sharness.sh

skip_all_unless_have jq

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

test_expect_success 'simple property reqirement works' '
	JOBID1=$(flux mini submit --requires="baz" hostname) &&
	flux job wait-event -t 10 ${JOBID1} clean &&
	flux job info ${JOBID1} R > JOBID1.R &&
	RANK1=$(cat JOBID1.R | jq -r ".execution.R_lite[0].rank") &&
	LENGTH1=$(cat JOBID1.R | jq -r ".execution.properties" | jq length) &&
	PROP_RANK1=$(cat JOBID1.R | jq -r ".execution.properties.baz") &&
	test "${RANK1}" = "3" -a "${LENGTH1}" = "1" -a "${PROP_RANK1}" = "3"
'

test_expect_success 'multiple properties reqirement works' '
	JOBID2=$(flux mini submit --requires="foo" --requires="bar" hostname) &&
	flux job wait-event -t 10 ${JOBID2} clean &&
	flux job info ${JOBID2} R > JOBID2.R &&
	RANK2=$(cat JOBID2.R | jq -r ".execution.R_lite[0].rank") &&
	LENGTH2=$(cat JOBID2.R | jq -r ".execution.properties" | jq length) &&
	PROP_RANK2=$(cat JOBID2.R | jq -r ".execution.properties.foo") &&
	PROP_RANK2B=$(cat JOBID2.R | jq -r ".execution.properties.bar") &&
	test "${RANK2}" = "1" -a "${LENGTH2}" = "2" &&
	test "${PROP_RANK2}" = "1" -a "${PROP_RANK2B}" = "1"
'

test_expect_success '^property reqirement works' '
	JOBID3=$(flux mini submit --requires="^foo" --requires="^bar" hostname) &&
	flux job wait-event -t 10 ${JOBID3} clean &&
	flux job info ${JOBID3} R > JOBID3.R &&
	RANK3=$(cat JOBID3.R | jq -r ".execution.R_lite[0].rank") &&
	LENGTH3=$(cat JOBID3.R | jq -r ".execution.properties" | jq length) &&
	PROP_RANK3=$(cat JOBID3.R | jq -r ".execution.properties.baz") &&
	test "${RANK3}" = "3" -a "${LENGTH3}" = "1" -a "${PROP_RANK3}" = "3"
'

test_expect_success 'nonexistient property works' '
	JOBID4=$(flux mini submit --requires="yyy" hostname) &&
	flux job wait-event -t 10 ${JOBID4} clean &&
	flux job eventlog ${JOBID4} > evlog4 &&
	grep "unsatisfiable" evlog4
'

test_expect_success 'invalid property works' '
	JOBID5=$(flux mini submit --requires="f^oo" hostname) &&
	flux job wait-event -t 10 ${JOBID5} clean &&
	flux job eventlog ${JOBID5} > evlog5 &&
	grep "f^oo is invalid" evlog5
'

test_expect_success 'removing resource and qmanager modules' '
	remove_qmanager &&
	remove_resource
'

test_done


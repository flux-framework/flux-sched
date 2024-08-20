#!/bin/sh

test_description='Test configuration file support for qmanager'

. `dirname $0`/sharness.sh

conf_base=${SHARNESS_TEST_SRCDIR}/conf.d
unset FLUXION_RESOURCE_RC_NOOP
unset FLUXION_QMANAGER_RC_NOOP
export FLUX_SCHED_MODULE=none

if test -z "${FLUX_SCHED_TEST_INSTALLED}" || test -z "${FLUX_SCHED_CO_INST}"
 then
     export FLUX_RC_EXTRA="${SHARNESS_TEST_SRCDIR}/../etc"
fi

# Run broker with specified config file and qmanager options.
# Usage: start_qmanager config-path [module options] >outfile
start_qmanager () {
    local config=$1; shift
    local outfile=$1; shift
    QMANAGER_OPTIONS=$*
    echo $QMANAGER_OPTIONS > options
    flux broker --config-path=${config} bash -c \
"flux module reload -f sched-fluxion-resource policy=high && "\
"flux module reload -f sched-fluxion-qmanager ${QMANAGER_OPTIONS} && "\
"flux module stats sched-fluxion-qmanager && "\
"flux qmanager-params >${outfile}"
}

start_qmanager_noconfig () {
    local outfile=$1; shift
    QMANAGER_OPTIONS=$*
    flux broker bash -c \
"flux module reload -f sched-fluxion-resource policy=high && "\
"flux module reload -f sched-fluxion-qmanager ${QMANAGER_OPTIONS} && "\
"flux module stats sched-fluxion-qmanager && "\
"flux qmanager-params >${outfile}"
}

# Usage: check_enforced_policy outfile expected
check_enforced_policy() {
    test $(jq '.params."queue-policy"' ${1}) = ${2}
}

# Usage: check_queue_params outfile expected
check_queue_params() {
    test $(jq '.params."queue-params"' ${1}) = ${2}
}

# Usage: check_policy_params outfile expected [nopkey]
check_policy_params() {
    test $(jq '.params."policy-params"' ${1}) = ${2}
}

# Usage: check_enforced_policy2 outfile expected
check_enforced_policy2() {
    test $(jq '."queue-policy"' ${1}) = ${2}
}

# Usage: check_queue_params2 outfile expected
check_queue_params2() {
    test $(jq '."queue-params"' ${1}) = ${2}
}

# Usage: check_policy_params outfile expected [nopkey]
check_policy_params2() {
    test $(jq '."policy-params"' ${1}) = ${2}
}

test_expect_success 'qmanager: load qmanager works with no config' '
    outfile=noconfig.out &&
    start_qmanager_noconfig ${outfile} &&
    check_enforced_policy ${outfile} "\"fcfs\"" &&
    check_queue_params ${outfile} "\"\"" &&
    check_policy_params ${outfile} "\"\""
'

test_expect_success 'qmanager: load qmanager works with valid qmanager.toml' '
    conf_name="01-default" &&
    outfile=${conf_name}.out &&
    start_qmanager ${conf_base}/${conf_name} ${outfile} &&
    check_enforced_policy ${outfile} "\"fcfs\"" &&
    check_queue_params ${outfile} \
	    "\"max-queue-depth=1000000,queue-depth=8192\"" &&
    check_policy_params ${outfile} \
	    "\"max-reservation-depth=100000,reservation-depth=64\""
'

test_expect_success 'qmanager: load qmanager works with no keys' '
    conf_name="02-no-keys" &&
    outfile=${conf_name}.out &&
    start_qmanager ${conf_base}/${conf_name} ${outfile} &&
    check_queue_params ${outfile} "\"\"" &&
    check_policy_params ${outfile} "\"\""
'

test_expect_success 'qmanager: load qmanager works with extra keys' '
    conf_name="03-extra-keys" &&
    outfile=${conf_name}.out &&
    start_qmanager ${conf_base}/${conf_name} ${outfile} &&
    check_queue_params ${outfile} \
	"\"max-queue-depth=1000000,queue-depth=8192,foo=1234\"" &&
    check_policy_params ${outfile} \
	"\"max-reservation-depth=100000,reservation-depth=64\""
'

test_expect_success 'qmanager: load works with no reservation-depth key' '
    conf_name="04-conservative" &&
    outfile=${conf_name}.out &&
    start_qmanager ${conf_base}/${conf_name} ${outfile} &&
    check_enforced_policy ${outfile} "\"conservative\"" &&
    check_queue_params ${outfile} \
	"\"max-queue-depth=1000000,queue-depth=8192\"" &&
    check_policy_params ${outfile} \
	"\"max-reservation-depth=64\""
'

test_expect_success 'qmanager: load works with small queue-depth' '
    conf_name="05-small-queue-depth" &&
    outfile=${conf_name}.out &&
    start_qmanager ${conf_base}/${conf_name} ${outfile} &&
    check_queue_params ${outfile} \
	"\"max-queue-depth=1000,queue-depth=10\"" &&
    check_policy_params ${outfile} \
	"\"max-reservation-depth=100000,reservation-depth=64\""
'

test_expect_success 'qmanager: load works with hybrid params 1' '
    conf_name="06-hybrid" &&
    outfile=${conf_name}.out &&
    start_qmanager ${conf_base}/${conf_name} ${outfile} &&
    check_queue_params ${outfile} \
	"\"max-queue-depth=1000000,queue-depth=8192\"" &&
    check_policy_params ${outfile} \
	"\"max-reservation-depth=128,reservation-depth=64\""
'

test_expect_success 'qmanager: load works with hybrid params 2' '
    conf_name="07-hybrid2" &&
    outfile=${conf_name}.out &&
    start_qmanager ${conf_base}/${conf_name} ${outfile} &&
    check_queue_params ${outfile} \
	"\"max-queue-depth=1000000,queue-depth=8192\"" &&
    check_policy_params ${outfile} \
	"\"max-reservation-depth=64,reservation-depth=128\""
'

test_expect_success 'qmanager: load works with no queue-depth key' '
    conf_name="08-no-queue-depth" &&
    outfile=${conf_name}.out &&
    start_qmanager ${conf_base}/${conf_name} ${outfile} &&
    check_queue_params ${outfile} \
	"\"max-queue-depth=1000\"" &&
    check_policy_params ${outfile} \
	"\"max-reservation-depth=100000,reservation-depth=64\""
'

test_expect_success 'qmanager: load must tolerate an invalid policy' '
    conf_name="09-invalid-policy" &&
    outfile=${conf_name}.out &&
    start_qmanager ${conf_base}/${conf_name} ${outfile} &&
    check_enforced_policy ${outfile} "\"fcfs\""
'

test_expect_success 'qmanager: load must fail on negative value' '
    conf_name="10-neg-value" &&
    outfile=${conf_name}.out &&
    test_must_fail start_qmanager ${conf_base}/${conf_name} ${outfile}
'

test_expect_success 'qmanager: load works with no params categories' '
    conf_name="11-no-params" &&
    outfile=${conf_name}.out &&
    start_qmanager ${conf_base}/${conf_name} ${outfile} &&
    check_queue_params ${outfile} "\"\"" &&
    check_policy_params ${outfile} "\"\""
'

test_expect_success 'qmanager: load works with no policy-params category' '
    conf_name="12-no-policy-params" &&
    outfile=${conf_name}.out &&
    start_qmanager ${conf_base}/${conf_name} ${outfile} &&
    check_policy_params ${outfile} "\"\""
'

test_expect_success 'qmanager: load must fail on a bad value' '
    conf_name="13-bad-value" &&
    outfile=${conf_name}.out &&
    test_must_fail start_qmanager ${conf_base}/${conf_name} ${outfile}
'

test_expect_success 'qmanager: load works with no queue-params category' '
    conf_name="14-no-queue-params" &&
    outfile=${conf_name}.out &&
    start_qmanager ${conf_base}/${conf_name} ${outfile} &&
    check_queue_params ${outfile} "\"\""
'

test_expect_success 'qmanager: load succeeds on no qmanager category' '
    conf_name="15-no-qmanager" &&
    outfile=${conf_name}.out &&
    start_qmanager ${conf_base}/${conf_name} ${outfile} &&
    check_queue_params ${outfile} \
	"\"max-queue-depth=1000000,queue-depth=8192\"" &&
    check_policy_params ${outfile} \
	"\"max-reservation-depth=100000,reservation-depth=64\""
'

test_expect_success 'qmanager: homogeneous multiqueue configuration works' '
    conf_name="16-multiqueue" &&
    outfile=${conf_name}.out &&
    start_qmanager ${conf_base}/${conf_name} ${outfile} &&
    jq ".params.debug" ${outfile} > ${outfile}.qdebug &&
    jq ".params.batch" ${outfile} > ${outfile}.qbatch &&
    check_enforced_policy2 ${outfile}.qdebug "\"fcfs\"" &&
    check_queue_params2 ${outfile}.qdebug \
        "\"max-queue-depth=1000000,queue-depth=8192\"" &&
    check_policy_params2 ${outfile}.qdebug \
        "\"max-reservation-depth=100000,reservation-depth=64\"" &&
    check_enforced_policy2 ${outfile}.qbatch "\"fcfs\"" &&
    check_queue_params2 ${outfile}.qbatch \
        "\"max-queue-depth=1000000,queue-depth=8192\"" &&
    check_policy_params2 ${outfile}.qbatch \
        "\"max-reservation-depth=100000,reservation-depth=64\""
'

test_expect_success 'qmanager: heterogeneous multiqueue configuration works' '
    conf_name="17-mq-hetero" &&
    outfile=${conf_name}.out &&
    start_qmanager ${conf_base}/${conf_name} ${outfile} &&
    jq ".params.debug" ${outfile} > ${outfile}.qdebug &&
    jq ".params.batch" ${outfile} > ${outfile}.qbatch &&
    check_enforced_policy2 ${outfile}.qdebug "\"fcfs\"" &&
    check_queue_params2 ${outfile}.qdebug "\"queue-depth=16\"" &&
    check_policy_params2 ${outfile}.qdebug "\"\"" &&
    check_enforced_policy2 ${outfile}.qbatch "\"hybrid\"" &&
    check_queue_params2 ${outfile}.qbatch "\"\"" &&
    check_policy_params2 ${outfile}.qbatch \
        "\"max-reservation-depth=100000,reservation-depth=64\""
'

test_expect_success 'qmanager: per-queue parameter overriding works' '
    conf_name="18-mq-override" &&
    outfile=${conf_name}.out &&
    start_qmanager ${conf_base}/${conf_name} ${outfile} &&
    jq ".params.debug" ${outfile} > ${outfile}.qdebug &&
    jq ".params.batch" ${outfile} > ${outfile}.qbatch &&
    check_enforced_policy2 ${outfile}.qdebug "\"fcfs\"" &&
    check_queue_params2 ${outfile}.qdebug "\"queue-depth=16\"" &&
    check_policy_params2 ${outfile}.qdebug "\"\"" &&
    check_enforced_policy2 ${outfile}.qbatch "\"easy\"" &&
    check_queue_params2 ${outfile}.qbatch \
        "\"max-queue-depth=1000000,queue-depth=8192\"" &&
    check_policy_params2 ${outfile}.qbatch "\"\""
'

test_expect_success 'qmanager: load must fail when pre-rfc33 queues are provided' '
    conf_name="19-pre-rfc33-queues" &&
    outfile=${conf_name}.out &&
    test_must_fail start_qmanager ${conf_base}/${conf_name} ${outfile}
'

test_done

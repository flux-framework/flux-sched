#!/bin/sh

test_description='Test configuration file support for qmanager'

ORIG_HOME=${HOME}

. `dirname $0`/sharness.sh

#
# sharness modifies $HOME environment variable, but this interferes
# with python's package search path, in particular its user site package.
#
HOME=${ORIG_HOME}

conf_base=${SHARNESS_TEST_SRCDIR}/conf.d
unset FLUXION_RESOURCE_RC_NOOP
unset FLUXION_QMANAGER_RC_NOOP
export FLUX_SCHED_MODULE=none

if test -z "${FLUX_SCHED_TEST_INSTALLED}" || test -z "${FLUX_SCHED_CO_INST}"
 then
     export FLUX_RC_EXTRA="${SHARNESS_TEST_SRCDIR}/rc"
fi

# Run broker with specified config file and qmanager options.
# Usage: start_qmanager config-path [module options] >outfile
start_qmanager () {
    local config=$1; shift
    QMANAGER_OPTIONS=$*
    echo $QMANAGER_OPTIONS > options
    flux broker --config-path=${config} bash -c \
"flux dmesg -C && "\
"flux module reload -f sched-fluxion-resource load-whitelist=node,core,gpu "\
"policy=high && "\
"flux module reload -f sched-fluxion-qmanager ${QMANAGER_OPTIONS} && "\
"flux dmesg"
}
start_qmanager_noconfig () {
    QMANAGER_OPTIONS=$*
    flux broker bash -c \
"flux dmesg -C && "\
"flux module reload -f sched-fluxion-resource load-whitelist=node,core,gpu "\
"policy=high && "\
"flux module reload -f sched-fluxion-qmanager ${QMANAGER_OPTIONS} && "\
"flux dmesg"
}
# Usage: check_enforced_policy outfile expected
check_enforced_policy() {
    test "$(grep "enforced policy" <$1 | awk "{ print \$6}" )" = $2
}

check_params() {
    res="$(grep "$3" <$1 | awk "{ print \$7}")"
    for tok in $(echo ${res} | sed -e 's@,@ @g')
    do
        echo ${2} | grep ${tok} || return 1
    done
}

# Usage: check_queue_params outfile expected
check_queue_params() {
    check_params $1 $2 "queue params"
}
# Usage: check_policy_params outfile expected
check_policy_params() {
    check_params $1 $2 "policy params"
}

test_expect_success 'qmanager: load qmanager works with no config' '
    outfile=noconfig.out &&
    start_qmanager_noconfig >${outfile} &&
    check_enforced_policy ${outfile} fcfs &&
    check_queue_params ${outfile} default &&
    check_policy_params ${outfile} default
'

test_expect_success 'qmanager: load qmanager works with valid qmanager.toml' '
    conf_name="01-default" &&
    outfile=${conf_name}.out &&
    start_qmanager ${conf_base}/${conf_name} >${outfile} &&
    check_enforced_policy ${outfile} "fcfs" &&
    check_queue_params ${outfile} \
	    "queue-depth=8192,max-queue-depth=1000000" &&
    check_policy_params ${outfile} \
	    "reservation-depth=64,max-reservation-depth=100000"
'

test_expect_success 'qmanager: module load options takes precedence' '
    conf_name="01-default" &&
    outfile=${conf_name}2.out &&
    start_qmanager ${conf_base}/${conf_name} \
	queue-policy=easy \
	queue-params=max-queue-depth=20000 \
	policy-params=max-reservation-depth=64 >${outfile} &&
    check_enforced_policy ${outfile} "easy" &&
    check_queue_params ${outfile} \
	    "max-queue-depth=20000" &&
    check_policy_params ${outfile} \
	    "max-reservation-depth=64"
'

test_expect_success 'qmanager: load qmanager works with no keys' '
    conf_name="02-no-keys" &&
    outfile=${conf_name}.out &&
    start_qmanager ${conf_base}/${conf_name} >${outfile} &&
    check_queue_params ${outfile} default &&
    check_policy_params ${outfile} default
'

test_expect_success 'qmanager: load qmanager works with extra keys' '
    conf_name="03-extra-keys" &&
    outfile=${conf_name}.out &&
    start_qmanager ${conf_base}/${conf_name} >${outfile} &&
    check_queue_params ${outfile} \
	"foo=1234,max-queue-depth=1000000,queue-depth=8192" &&
    check_policy_params ${outfile} \
	"reservation-depth=64,max-reservation-depth=100000"
'

test_expect_success 'qmanager: load works with no reservation-depth key' '
    conf_name="04-conservative" &&
    outfile=${conf_name}.out &&
    start_qmanager ${conf_base}/${conf_name} >${outfile} &&
    check_enforced_policy ${outfile} "conservative" &&
    check_queue_params ${outfile} \
	"queue-depth=8192,max-queue-depth=1000000" &&
    check_policy_params ${outfile} \
	"max-reservation-depth=64"
'

test_expect_success 'qmanager: load works with small queue-depth' '
    conf_name="05-small-queue-depth" &&
    outfile=${conf_name}.out &&
    start_qmanager ${conf_base}/${conf_name} >${outfile} &&
    check_queue_params ${outfile} \
	"queue-depth=10,max-queue-depth=1000" &&
    check_policy_params ${outfile} \
	"reservation-depth=64,max-reservation-depth=100000"
'

test_expect_success 'qmanager: load works with hybrid params 1' '
    conf_name="06-hybrid" &&
    outfile=${conf_name}.out &&
    start_qmanager ${conf_base}/${conf_name} >${outfile} &&
    check_queue_params ${outfile} \
	"queue-depth=8192,max-queue-depth=1000000" &&
    check_policy_params ${outfile} \
	"reservation-depth=64,max-reservation-depth=128"
'

test_expect_success 'qmanager: load works with hybrid params 2' '
    conf_name="07-hybrid2" &&
    outfile=${conf_name}.out &&
    start_qmanager ${conf_base}/${conf_name} >${outfile} &&
    check_queue_params ${outfile} \
	"queue-depth=8192,max-queue-depth=1000000" &&
    check_policy_params ${outfile} \
	"reservation-depth=128,max-reservation-depth=64"
'

test_expect_success 'qmanager: load works with no queue-depth key' '
    conf_name="08-no-queue-depth" &&
    outfile=${conf_name}.out &&
    start_qmanager ${conf_base}/${conf_name} >${outfile} &&
    check_queue_params ${outfile} \
	"max-queue-depth=1000" &&
    check_policy_params ${outfile} \
	"reservation-depth=64,max-reservation-depth=100000"
'

test_expect_success 'qmanager: load must tolerate an invalid policy' '
    conf_name="09-invalid-policy" &&
    outfile=${conf_name}.out &&
    start_qmanager ${conf_base}/${conf_name} >${outfile} &&
    check_enforced_policy ${outfile} fcfs
'

test_expect_success 'qmanager: load must fail on negative value' '
    conf_name="10-neg-value" &&
    outfile=${conf_name}.out &&
    test_must_fail start_qmanager ${conf_base}/${conf_name} >${outfile}
'

test_expect_success 'qmanager: load works with no params categories' '
    conf_name="11-no-params" &&
    outfile=${conf_name}.out &&
    start_qmanager ${conf_base}/${conf_name} >${outfile} &&
    check_queue_params ${outfile} default &&
    check_policy_params ${outfile} default
'

test_expect_success 'qmanager: load works with no policy-params category' '
    conf_name="12-no-policy-params" &&
    outfile=${conf_name}.out &&
    start_qmanager ${conf_base}/${conf_name} >${outfile} &&
    check_policy_params ${outfile} default
'

test_expect_success 'qmanager: load must fail on a bad value' '
    conf_name="13-bad-value" &&
    outfile=${conf_name}.out &&
    test_must_fail start_qmanager ${conf_base}/${conf_name} >${outfile}
'

test_expect_success 'qmanager: load works with no queue-params category' '
    conf_name="14-no-queue-params" &&
    outfile=${conf_name}.out &&
    start_qmanager ${conf_base}/${conf_name} >${outfile} &&
    check_queue_params ${outfile} default
'

test_expect_success 'qmanager: load succeeds on no qmanager category' '
    conf_name="15-no-qmanager" &&
    outfile=${conf_name}.out &&
    start_qmanager ${conf_base}/${conf_name} >${outfile} &&
    check_queue_params ${outfile} default &&
    check_policy_params ${outfile} default
'

test_expect_success 'qmanager: homogeneous multiqueue configuration works' '
    conf_name="16-multiqueue" &&
    outfile=${conf_name}.out &&
    start_qmanager ${conf_base}/${conf_name} > ${outfile} &&
    grep "queue=debug" ${outfile} > ${outfile}.qdebug &&
    grep "queue=batch" ${outfile} > ${outfile}.qbatch &&
    check_enforced_policy ${outfile}.qdebug "fcfs" &&
    check_queue_params ${outfile}.qdebug \
        "queue-depth=8192,max-queue-depth=1000000" &&
    check_policy_params ${outfile}.qdebug \
        "reservation-depth=64,max-reservation-depth=100000" &&
    check_enforced_policy ${outfile}.qbatch "fcfs" &&
    check_queue_params ${outfile}.qbatch \
        "queue-depth=8192,max-queue-depth=1000000" &&
    check_policy_params ${outfile}.qbatch \
        "reservation-depth=64,max-reservation-depth=100000"
'

test_expect_success 'qmanager: heterogeneous multiqueue configuration works' '
    conf_name="17-mq-hetero" &&
    outfile=${conf_name}.out &&
    start_qmanager ${conf_base}/${conf_name} > ${outfile} &&
    grep "queue=debug" ${outfile} > ${outfile}.qdebug &&
    grep "queue=batch" ${outfile} > ${outfile}.qbatch &&
    check_enforced_policy ${outfile}.qdebug "fcfs" &&
    check_queue_params ${outfile}.qdebug "queue-depth=16" &&
    check_policy_params ${outfile}.qdebug "default" &&
    check_enforced_policy ${outfile}.qbatch "hybrid" &&
    check_queue_params ${outfile}.qbatch "default" &&
    check_policy_params ${outfile}.qbatch \
        "reservation-depth=64,max-reservation-depth=100000"
'

test_expect_success 'qmanager: per-queue parameter overriding works' '
    conf_name="18-mq-override" &&
    outfile=${conf_name}.out &&
    start_qmanager ${conf_base}/${conf_name} > ${outfile} &&
    grep "queue=debug" ${outfile} > ${outfile}.qdebug &&
    grep "queue=batch" ${outfile} > ${outfile}.qbatch &&
    check_enforced_policy ${outfile}.qdebug "fcfs" &&
    check_queue_params ${outfile}.qdebug "queue-depth=16" &&
    check_policy_params ${outfile}.qdebug "default" &&
    check_enforced_policy ${outfile}.qbatch "easy" &&
    check_queue_params ${outfile}.qbatch \
        "queue-depth=8192,max-queue-depth=1000000" &&
    check_policy_params ${outfile}.qbatch "default"
'

test_done


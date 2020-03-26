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

# Run broker with specified config file and qmanager options.
# Usage: start_qmanager config-path [module options] >outfile
start_qmanager () {
    local config=$1; shift
    QMANAGER_OPTIONS=$* flux broker \
	-Sbroker.rc1_path=./rc1 -Sbroker.rc3_path=./rc3 \
	--config-path=${config} \
	flux dmesg
}
start_qmanager_noconfig () {
    QMANAGER_OPTIONS=$* flux broker \
	-Sbroker.rc1_path=./rc1 -Sbroker.rc3_path=./rc3 \
	flux dmesg
}
# Usage: check_enforced_policy outfile expected
check_enforced_policy() {
    test "$(grep "enforced policy" <$1 | awk "{ print \$5}" )" = $2
}
# Usage: check_queue_params outfile expected
check_queue_params() {
    test "$(grep "queue params" <$1 | awk "{ print \$6}" )" = $2
}
# Usage: check_policy_params outfile expected
check_policy_params() {
    test "$(grep "policy params" <$1 | awk "{ print \$6}" )" = $2
}

test_expect_success 'qmanager: create rc1/rc3 for qmanager test' '
	cat <<-EOT >rc1 &&
	flux module load kvs
	flux module load job-manager
	flux module load qmanager \$QMANAGER_OPTIONS
	EOT
	cat <<-EOT >rc3 &&
	flux module remove -f qmanager
	flux module remove -f job-manager
	flux module remove -f kvs
	EOT
	chmod +x rc1 rc3
'

test_expect_success 'qmanager: load qmanager works with no config' '
    outfile=noconfig.out &&
    start_qmanager_noconfig >${outfile} &&
    check_queue_params ${outfile} default &&
    check_policy_params ${outfile} default
'

test_expect_success 'qmanager: load qmanager works with valid qmanager.toml' '
    conf_name="01-default" &&
    outfile=${conf_name}2.out &&
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
	    "max-queue-depth=20000,queue-depth=8192" &&
    check_policy_params ${outfile} \
	    "max-reservation-depth=64,reservation-depth=64"
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
	"queue-depth=8192,max-queue-depth=1000000" &&
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

test_expect_success 'qmanager: load must fail on invalid policy' '
    conf_name="09-invalid-policy" &&
    outfile=${conf_name}.out &&
    test_must_fail start_qmanager ${conf_base}/${conf_name} >${outfile}
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

# because required sub categories exist, this should succeed
test_expect_success 'qmanager: load succeeds on no qmanager category' '
    conf_name="15-no-qmanager" &&
    outfile=${conf_name}.out &&
    start_qmanager ${conf_base}/${conf_name} >${outfile} &&
    check_queue_params ${outfile} \
	"queue-depth=8192,max-queue-depth=1000000" &&
    check_policy_params ${outfile} \
	"reservation-depth=64,max-reservation-depth=100000"
'

test_done


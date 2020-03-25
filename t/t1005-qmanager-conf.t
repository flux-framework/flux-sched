#!/bin/sh

test_description='Test configuration file support for qmanager'

ORIG_HOME=${HOME}

. `dirname $0`/sharness.sh

#
# sharness modifies $HOME environment variable, but this interferes
# with python's package search path, in particular its user site package.
#
HOME=${ORIG_HOME}

unset FLUX_RESOURCE_RC_NOOP
unset FLUX_QMANAGER_RC_NOOP
if test -z "${FLUX_SCHED_TEST_INSTALLED}" || test -z "${FLUX_SCHED_CO_INST}"
then
    export FLUX_RC_EXTRA="${SHARNESS_TEST_SRCDIR}/rc"
fi
conf_base=${SHARNESS_TEST_SRCDIR}/conf.d

test_expect_success 'qmanager: load qmanager works with valid qmanager.toml' '
    conf_name="01-default" &&
    export FLUX_CONF_DIR=${conf_base}/${conf_name} &&
    flux broker bash -c \
"flux dmesg -C && flux module reload -f qmanager && "\
"flux dmesg" > ${conf_name}.out &&
    p=$(cat ${conf_name}.out | grep "enforced policy" | awk "{ print \$5}") &&
    qp=$(cat ${conf_name}.out | grep "queue params" | awk "{ print \$6}") &&
    pp=$(cat ${conf_name}.out | grep "policy params" | awk "{ print \$6}") &&
    test "${p}" = "fcfs" &&
    test "${qp}" = "queue-depth=8192,max-queue-depth=1000000" &&
    test "${pp}" = "reservation-depth=64,max-reservation-depth=100000" &&
    unset FLUX_CONF_DIR
'

test_expect_success 'qmanager: module load options takes precedence' '
    conf_name="01-default" &&
    export FLUX_CONF_DIR=${conf_base}/${conf_name} &&
    flux broker bash -c \
"flux dmesg -C && flux module reload -f qmanager "\
"queue-policy=easy queue-params=max-queue-depth=20000 "\
"policy-params=max-reservation-depth=64 && flux dmesg" > ${conf_name}2.out &&
    p=$(cat ${conf_name}2.out | grep "enforced policy" | awk "{ print \$5}") &&
    qp=$(cat ${conf_name}2.out | grep "queue params" | awk "{ print \$6}") &&
    pp=$(cat ${conf_name}2.out | grep "policy params" | awk "{ print \$6}") &&
    test "${p}" = "easy" &&
    test "${qp}" = "max-queue-depth=20000,queue-depth=8192" &&
    test "${pp}" = "max-reservation-depth=64,reservation-depth=64" &&
    unset FLUX_CONF_DIR
'

test_expect_success 'qmanager: load qmanager works with no keys' '
    conf_name="02-no-keys" &&
    export FLUX_CONF_DIR=${conf_base}/${conf_name} &&
    flux broker bash -c \
"flux dmesg -C && flux module reload -f qmanager && flux dmesg" \
> ${conf_name}.out &&
    qp=$(cat ${conf_name}.out | grep "queue params" | awk "{ print \$6}") &&
    pp=$(cat ${conf_name}.out | grep "policy params" | awk "{ print \$6}") &&
    test "${qp}" = "default" &&
    test "${pp}" = "default" &&
    unset FLUX_CONF_DIR
'

test_expect_success 'qmanager: load qmanager works with extra keys' '
    conf_name="03-extra-keys" &&
    export FLUX_CONF_DIR=${conf_base}/${conf_name} &&
    flux broker bash -c \
"flux dmesg -C && flux module reload -f qmanager && flux dmesg" \
> ${conf_name}.out &&
    qp=$(cat ${conf_name}.out | grep "queue params" | awk "{ print \$6}") &&
    pp=$(cat ${conf_name}.out | grep "policy params" | awk "{ print \$6}") &&
    test "${qp}" = "queue-depth=8192,max-queue-depth=1000000" &&
    test "${pp}" = "reservation-depth=64,max-reservation-depth=100000" &&
    unset FLUX_CONF_DIR
'

test_expect_success 'qmanager: load works with no reservation-depth key' '
    conf_name="04-conservative" &&
    export FLUX_CONF_DIR=${conf_base}/${conf_name} &&
    flux broker bash -c \
"flux dmesg -C && flux module reload -f qmanager && flux dmesg" \
> ${conf_name}.out &&
    p=$(cat ${conf_name}.out | grep "enforced policy" | awk "{ print \$5}") &&
    qp=$(cat ${conf_name}.out | grep "queue params" | awk "{ print \$6}") &&
    pp=$(cat ${conf_name}.out | grep "policy params" | awk "{ print \$6}") &&
    test "${p}" = "conservative" &&
    test "${qp}" = "queue-depth=8192,max-queue-depth=1000000" &&
    test "${pp}" = "max-reservation-depth=64" &&
    unset FLUX_CONF_DIR
'

test_expect_success 'qmanager: load works with small queue-depth' '
    conf_name="05-small-queue-depth" &&
    export FLUX_CONF_DIR=${conf_base}/${conf_name} &&
    flux broker bash -c \
"flux dmesg -C &&  flux module reload -f qmanager && flux dmesg" \
> ${conf_name}.out &&
    qp=$(cat ${conf_name}.out | grep "queue params" | awk "{ print \$6}") &&
    pp=$(cat ${conf_name}.out | grep "policy params" | awk "{ print \$6}") &&
    test "${qp}" = "queue-depth=10,max-queue-depth=1000" &&
    test "${pp}" = "reservation-depth=64,max-reservation-depth=100000" &&
    unset FLUX_CONF_DIR
'

test_expect_success 'qmanager: load works with hybrid params 1' '
    conf_name="06-hybrid" &&
    export FLUX_CONF_DIR=${conf_base}/${conf_name} &&
    flux broker bash -c \
"flux dmesg -C && flux module reload -f qmanager && flux dmesg" \
> ${conf_name}.out &&
    qp=$(cat ${conf_name}.out | grep "queue params" | awk "{ print \$6}") &&
    pp=$(cat ${conf_name}.out | grep "policy params" | awk "{ print \$6}") &&
    test "${qp}" = "queue-depth=8192,max-queue-depth=1000000" &&
    test "${pp}" = "reservation-depth=64,max-reservation-depth=128" &&
    unset FLUX_CONF_DIR
'

test_expect_success 'qmanager: load works with hybrid params 2' '
    conf_name="07-hybrid2" &&
    export FLUX_CONF_DIR=${conf_base}/${conf_name} &&
    flux broker bash -c \
"flux dmesg -C && flux module reload -f qmanager && flux dmesg" \
> ${conf_name}.out &&
    qp=$(cat ${conf_name}.out | grep "queue params" | awk "{ print \$6}") &&
    pp=$(cat ${conf_name}.out | grep "policy params" | awk "{ print \$6}") &&
    test "${qp}" = "queue-depth=8192,max-queue-depth=1000000" &&
    test "${pp}" = "reservation-depth=128,max-reservation-depth=64" &&
    unset FLUX_CONF_DIR
'

test_expect_success 'qmanager: load works with no queue-depth key' '
    conf_name="08-no-queue-depth" &&
    export FLUX_CONF_DIR=${conf_base}/${conf_name} &&
    flux broker bash -c \
"flux dmesg -C && flux module reload -f qmanager && flux dmesg" \
> ${conf_name}.out &&
    qp=$(cat ${conf_name}.out | grep "queue params" | awk "{ print \$6}") &&
    pp=$(cat ${conf_name}.out | grep "policy params" | awk "{ print \$6}") &&
    test "${qp}" = "max-queue-depth=1000" &&
    test "${pp}" = "reservation-depth=64,max-reservation-depth=100000" &&
    unset FLUX_CONF_DIR
'

test_expect_success 'qmanager: load must fail on invalid policy' '
    export FLUX_CONF_DIR=${conf_base}/09-invalid-policy &&
    test_must_fail flux broker /bin/true &&
    unset FLUX_CONF_DIR
'

test_expect_success 'qmanager: load must fail on negative value' '
    export FLUX_CONF_DIR=${conf_base}/10-neg-value &&
    test_must_fail flux broker /bin/true &&
    unset FLUX_CONF_DIR
'

test_expect_success 'qmanager: load must fail on no params categories' '
    export FLUX_CONF_DIR=${conf_base}/11-no-params &&
    test_must_fail flux broker /bin/true &&
    unset FLUX_CONF_DIR
'

test_expect_success 'qmanager: load must fail on no policy-params category' '
    export FLUX_CONF_DIR=${conf_base}/12-no-policy-params &&
    test_must_fail flux broker /bin/true &&
    unset FLUX_CONF_DIR
'

test_expect_success 'qmanager: load must fail on a bad value' '
    export FLUX_CONF_DIR=${conf_base}/13-bad-value &&
    test_must_fail flux broker /bin/true &&
    unset FLUX_CONF_DIR
'

test_expect_success 'qmanager: load must fail on no queue-params category' '
    export FLUX_CONF_DIR=${conf_base}/14-no-queue-params &&
    test_must_fail flux broker /bin/true &&
    unset FLUX_CONF_DIR
'

# because required sub categories exist, this should succeed
test_expect_success 'qmanager: load succeeds on no qmanager category' '
    conf_name="15-no-qmanager" &&
    export FLUX_CONF_DIR=${conf_base}/${conf_name} &&
    flux broker bash -c \
"flux dmesg -C &&  flux module reload -f qmanager && flux dmesg" \
> ${conf_name}.out &&
    qp=$(cat ${conf_name}.out | grep "queue params" | awk "{ print \$6}") &&
    pp=$(cat ${conf_name}.out | grep "policy params" | awk "{ print \$6}") &&
    test "${qp}" = "queue-depth=8192,max-queue-depth=1000000" &&
    test "${pp}" = "reservation-depth=64,max-reservation-depth=100000" &&
    unset FLUX_CONF_DIR
'

test_done


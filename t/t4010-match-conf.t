#!/bin/sh

test_description='Test configuration file support for sched-fluxion-resource'

. `dirname $0`/sharness.sh

conf_base=${SHARNESS_TEST_SRCDIR}/conf.d
unset FLUXION_RESOURCE_RC_NOOP
unset FLUXION_QMANAGER_RC_NOOP
export FLUX_SCHED_MODULE=none

if test -z "${FLUX_SCHED_TEST_INSTALLED}" || test -z "${FLUX_SCHED_CO_INST}"
 then
     export FLUX_RC_EXTRA="${SHARNESS_TEST_SRCDIR}/../etc"
fi

# Run broker with specified config file and sched-fluxion-resource options.
# Usage: start_resource config-path outfile [module options]
start_resource () {
    local config=$1; shift
    local outfile=$1; shift
    RESOURCE_OPTIONS=$*
    echo $RESOURCE_OPTIONS > options
    flux broker --config-path=${config} bash -c \
"flux module reload -f sched-fluxion-resource ${RESOURCE_OPTIONS} && "\
"flux module reload -f sched-fluxion-qmanager && "\
"flux module stats sched-fluxion-qmanager && "\
"flux ion-resource params >${outfile}"
}
start_resource_noconfig () {
    local outfile=$1; shift
    RESOURCE_OPTIONS=$*
    flux broker bash -c \
"flux module reload -f sched-fluxion-resource ${RESOURCE_OPTIONS} && "\
"flux module reload -f sched-fluxion-qmanager && "\
"flux module stats sched-fluxion-qmanager && "\
"flux ion-resource params >${outfile}"
}
check_load_file(){
    test $(jq '.params."load-file"' ${1}) = ${2}
}
check_load_format(){
    test $(jq '.params."load-format"' ${1}) = ${2}
}
check_load_allowlist(){
    test $(jq '.params."load-allowlist"' ${1}) = ${2}
}
check_match_policy(){
    test $(jq '.params."policy"' ${1}) = ${2}
}
check_match_format(){
    test $(jq '.params."match-format"' ${1}) = ${2}
}
check_match_subsystems(){
    test $(jq '.params."subsystems"' ${1}) = ${2}
}
check_reserve_vtx_vec(){
    test $(jq '.params."reserve-vtx-vec"' ${1}) -eq ${2}
}
check_prune_filters(){
    test $(jq '.params."prune-filters"' ${1}) = ${2}
}

test_expect_success 'resource: sched-fluxion-resource loads with no config' '
    outfile=noconfig.out &&
    start_resource_noconfig ${outfile} &&
    check_load_file ${outfile} null &&
    check_load_format ${outfile} "\"rv1exec\"" &&
    check_load_allowlist ${outfile} null &&
    check_match_policy ${outfile} "\"first\"" &&
    check_match_format ${outfile} "\"rv1_nosched\"" &&
    check_match_subsystems ${outfile} "\"containment\"" &&
    check_reserve_vtx_vec ${outfile} 0 &&
    check_prune_filters ${outfile} "\"ALL:core,ALL:node\""
'

test_expect_success 'resource: sched-fluxion-resource loads with valid toml' '
    conf_name="01-default" &&
    outfile=${conf_name}.out &&
    start_resource ${conf_base}/${conf_name} ${outfile} &&
    check_load_file ${outfile} null &&
    check_load_format ${outfile} "\"rv1exec\"" &&
    check_load_allowlist ${outfile} "\"node,core,gpu\"" &&
    check_match_policy ${outfile} "\"lonodex\"" &&
    check_match_format ${outfile} "\"rv1_nosched\"" &&
    check_match_subsystems ${outfile} "\"containment\"" &&
    check_reserve_vtx_vec ${outfile} 200000 &&
    check_prune_filters ${outfile} "\"ALL:core,ALL:gpu\""
'

test_expect_success 'resource: module load options take precedence' '
    conf_name="01-default" &&
    outfile=${conf_name}.out &&
    start_resource ${conf_base}/${conf_name} ${outfile} \
	policy=high match-format=rv1 &&
    check_load_file ${outfile} null &&
    check_load_format ${outfile} "\"rv1exec\"" &&
    check_load_allowlist ${outfile} "\"node,core,gpu\"" &&
    check_match_policy ${outfile} "\"high\"" &&
    check_match_format ${outfile} "\"rv1\"" &&
    check_match_subsystems ${outfile} "\"containment\"" &&
    check_reserve_vtx_vec ${outfile} 200000 &&
    check_prune_filters ${outfile} "\"ALL:core,ALL:gpu\""
'

test_expect_success 'resource: sched-fluxion-resource loads with no keys' '
    conf_name="02-no-keys" &&
    outfile=${conf_name}.out &&
    start_resource ${conf_base}/${conf_name} ${outfile} &&
    check_load_file ${outfile} null &&
    check_load_format ${outfile} "\"rv1exec\"" &&
    check_load_allowlist ${outfile} null &&
    check_match_policy ${outfile} "\"first\"" &&
    check_match_format ${outfile} "\"rv1_nosched\"" &&
    check_match_subsystems ${outfile} "\"containment\"" &&
    check_reserve_vtx_vec ${outfile} 0 &&
    check_prune_filters ${outfile} "\"ALL:core,ALL:node\""
'

test_expect_success 'resource: sched-fluxion-resource loads with extra keys' '
    conf_name="03-extra-keys" &&
    outfile=${conf_name}.out &&
    start_resource ${conf_base}/${conf_name} ${outfile} &&
    check_load_file ${outfile} null &&
    check_load_format ${outfile} "\"rv1exec\"" &&
    check_load_allowlist ${outfile} "\"node,core,gpu,foo\"" &&
    check_match_policy ${outfile} "\"lonodex\"" &&
    check_match_format ${outfile} "\"rv1_nosched\"" &&
    check_match_subsystems ${outfile} "\"containment\"" &&
    check_reserve_vtx_vec ${outfile} 200000 &&
    check_prune_filters ${outfile} "\"ALL:core,ALL:gpu\""
'

test_expect_failure 'resource: load must error out on an invalid policy' '
    conf_name="09-invalid-policy" &&
    outfile=${conf_name}.out &&
    start_resource ${conf_base}/${conf_name} ${outfile}
'

test_expect_success 'resource: load must fail on a bad value' '
    conf_name="13-bad-value" &&
    outfile=${conf_name}.out &&
    test_must_fail start_resource ${conf_base}/${conf_name} ${outfile}
'

test_done


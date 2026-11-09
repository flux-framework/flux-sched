#!/bin/sh

test_description='Test nslots calculation with multiple RV1 writers'

. "$(dirname "$0")/sharness.sh"

cmd_dir="${SHARNESS_TEST_SRCDIR}/data/resource/commands/nslots"
exp_dir="${SHARNESS_TEST_SRCDIR}/data/resource/expected/nslots"
jgf="${SHARNESS_TEST_SRCDIR}/data/resource/jgfs/tiny.json"
query="../../resource/utilities/resource-query"

run_nslots_test () {
    test_number=$1
    description=$2
    writer=$3
    expected_number=$4
    policy=$5
    traverser=$6

    cmd_file="${cmd_dir}/cmds$(printf "%02d" "${test_number}").in"
    generated_cmds="cmds$(printf "%03d" "${test_number}")"
    output_file="$(printf "%03d" "${test_number}").R.out"
    expected_file="${exp_dir}/$(printf "%03d" "${expected_number}").R.out"

    test_expect_success "${writer}: ${description}" "
        sed 's~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g' ${cmd_file} > ${generated_cmds} &&
        ${query} -L ${jgf} -f jgf -F ${writer} -s -S CA -P ${policy} ${traverser:+-T "${traverser}"} -t ${output_file} < ${generated_cmds} &&
        test_cmp ${output_file} ${expected_file}
    "
}

run_writer_tests () {
    writer=$1
    expected_offset=$2

    run_nslots_test 1 \
        "nslots of a full hierarchical jobspec" \
        "${writer}" "$((expected_offset + 1))" high ""

    run_nslots_test 2 \
        "slot count multiplied by higher-order resources" \
        "${writer}" "$((expected_offset + 2))" high ""

    run_nslots_test 3 \
        "nslots for slot ranges" \
        "${writer}" "$((expected_offset + 3))" high ""

    run_nslots_test 4 \
        "slot type as the top resource" \
        "${writer}" "$((expected_offset + 4))" high ""

    run_nslots_test 5 \
        "multiple moldable requests" \
        "${writer}" "$((expected_offset + 5))" high ""

    run_nslots_test 6 \
        "complex allocation sequence" \
        "${writer}" "$((expected_offset + 6))" first ""

    run_nslots_test 7 \
        "nslots calculation with or_slots" \
        "${writer}" "$((expected_offset + 7))" high flexible

    run_nslots_test 8 \
        "nslots calculation with xor_slots" \
        "${writer}" "$((expected_offset + 8))" high flexible
}

run_writer_tests rv1 0
run_writer_tests rv1_nosched 8
run_writer_tests rv1_shorthand 16

test_done
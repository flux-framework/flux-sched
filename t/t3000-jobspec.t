#!/bin/sh

test_description='Test the jobspec parsing library'

. `dirname $0`/sharness.sh

validate="${SHARNESS_BUILD_DIRECTORY}/resource/libjobspec/flux-jobspec-validate"
data_dir="data/resource/jobspecs/validation"

# Check that the valid jobspecs all pass
for jobspec in ${SHARNESS_TEST_SRCDIR}/${data_dir}/valid/*.yaml; do
    testname=`basename $jobspec`
    test_expect_success $testname "$validate $jobspec"
done

# Check that the invalid jobspec all fail
for jobspec in ${SHARNESS_TEST_SRCDIR}/${data_dir}/invalid/*.yaml; do
    testname=`basename $jobspec`
    test_expect_success $testname "test_must_fail $validate $jobspec"
done

test_done

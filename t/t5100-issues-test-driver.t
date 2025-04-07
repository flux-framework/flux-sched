#!/bin/sh
#
test_description='Verify that fixed issues remain fixed'

. `dirname $0`/sharness.sh

if test_have_prereq ASAN; then
    skip_all='skipping issues tests under AddressSanitizer'
    test_done
fi
SIZE=2
test_under_flux ${SIZE}
echo "# $0: flux session size will be ${SIZE}"

if test -z "$T5100_ISSUES_GLOB"; then
    T5100_ISSUES_GLOB="*"
fi

flux bulksubmit -n1 -o pty --job-name={./%} -t 10m \
	--flags=waitable \
	--quiet --watch  \
	flux start {} \
	::: ${SHARNESS_TEST_SRCDIR}/issues/${T5100_ISSUES_GLOB}

for id in $(flux jobs -ano {id}); do
    test_expect_success $(flux jobs -no {name} $id) "flux job attach $id"
done

test_done

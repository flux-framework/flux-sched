#!/bin/sh
#

test_description='Test the very basics

Ensure the very basics of flux common scheduler services work.
'

. `dirname $0`/sharness.sh

test_expect_success 'TEST_NAME is set' '
	test -n "$TEST_NAME"
'

test_done

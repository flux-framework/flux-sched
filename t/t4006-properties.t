#!/bin/sh
#set -x

test_description='Test the basic functionality of properties (get/set) within resource
'

ORIG_HOME=${HOME}

. `dirname $0`/sharness.sh

#
# sharness modifies $HOME environment variable, but this interferes
# with python's package search path, in particular its user site package.
#
HOME=${ORIG_HOME}

grug="${SHARNESS_TEST_SRCDIR}/data/resource/grugs/tiny.graphml"
jobspec="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/basics/test001.yaml"

#
# test_under_flux is under sharness.d/
#
test_under_flux 1

#
# print only with --debug
#

test_debug '
	echo ${grug} &&
	echo ${jobspec}
'

test_expect_success 'loading resource module with a tiny machine config works' '
	flux module load resource grug-conf=${grug} prune-filters=ALL:core \
	subsystems=containment policy=high
'

test_expect_success 'set/get property basic test works' '
	flux resource set-property /tiny0/rack0/node0 class=one &&
	flux resource get-property /tiny0/rack0/node0 class > sp.0 &&
	echo "class = one" > expected &&
	test_cmp expected sp.0
'

test_expect_success 'set/get property multiple resources works' '
	flux resource set-property /tiny0/rack0/node0 nodeprop=1 &&
	flux resource set-property /tiny0/rack0/node0/socket1 sockprop=abc &&
	flux resource set-property /tiny0/rack0/node1/socket0/core17 coreprop=z &&
	flux resource get-property /tiny0/rack0/node0 nodeprop > sp.1 &&
	flux resource get-property /tiny0/rack0/node0/socket1 sockprop >> sp.1 &&
	flux resource get-property /tiny0/rack0/node1/socket0/core17 coreprop >> sp.1 &&
	cat <<-EOF >expected &&
	nodeprop = 1
	sockprop = abc
	coreprop = z
	EOF
	test_cmp expected sp.1
'

test_expect_success 'set/get property multiple properties works' '
	flux resource set-property /tiny0/rack0/node0 prop1=a && 
	flux resource set-property /tiny0/rack0/node0 prop2=foo &&
	flux resource set-property /tiny0/rack0/node0 prop3=123 &&
	flux resource set-property /tiny0/rack0/node0 prop4=bar &&
	flux resource set-property /tiny0/rack0/node0 prop5=baz &&
	flux resource get-property /tiny0/rack0/node0 prop1 > sp.2 &&
	flux resource get-property /tiny0/rack0/node0 prop2 >> sp.2 &&
	flux resource get-property /tiny0/rack0/node0 prop3 >> sp.2 &&
	flux resource get-property /tiny0/rack0/node0 prop4 >> sp.2 &&
	flux resource get-property /tiny0/rack0/node0 prop5 >> sp.2 &&
	cat <<-EOF >expected &&
	prop1 = a
	prop2 = foo
	prop3 = 123
	prop4 = bar
	prop5 = baz
	EOF
	test_cmp expected sp.2
'

test_expect_success 'test with no path works' '
	test_expect_code 1 flux resource set-property /dont/exist random=1
'

test_expect_success 'test with no property works' '
	test_expect_code 1 flux resource get-property /tiny0/rack0/node0 dontexist
'

test_expect_success 'test with malformed inputs works' '
	test_expect_code 1 flux resource set-property /tiny0/rack0/node0 badprop &&
	test_expect_code 1 flux resource get-property /tiny0/rack0/node0 badprop &&
	test_expect_code 1 flux resource set-property /tiny0/rack0/node0 badprop= &&
	test_expect_code 1 flux resource get-property /tiny0/rack0/node0 badprop &&
	test_expect_code 1 flux resource set-property /tiny0/rack0/node0 =badprop &&
	test_expect_code 1 flux resource get-property /tiny0/rack0/node0 badprop &&
	test_expect_code 1 flux resource set-property /tiny0/rack0/node0 = && 
	test_expect_code 1 flux resource get-property /tiny0/rack0/node0 badprop
'

test_expect_success 'test with complex inputs works' '
	flux resource set-property /tiny0/rack0/node0 badprop==1 && 
	flux resource get-property /tiny0/rack0/node0 badprop > sp.5 &&
	flux resource set-property /tiny0/rack0/node0 badprop=1=class=random && 
	flux resource get-property /tiny0/rack0/node0 badprop >> sp.5 &&
	flux resource set-property /tiny0/rack0/node0 badprop=1 && 
	flux resource get-property /tiny0/rack0/node0 badprop >> sp.5 &&
	cat <<-EOF >expected &&
	badprop = =1
	badprop = 1=class=random
	badprop = 1
	EOF
	test_cmp expected sp.5
'

test_expect_success 'removing resource works' '
	flux module remove resource
'

test_done

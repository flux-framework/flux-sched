#!/bin/sh
#set -x

test_description='Test the basic functionality of properties (get/set) within resource
'

. `dirname $0`/sharness.sh

grug="${SHARNESS_TEST_SRCDIR}/data/resource/grugs/tiny.graphml"
jobspec="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/basics/test001.yaml"

#
# test_under_flux is under sharness.d/
#
test_under_flux 1

#
# print only with --debug
#

send_rpc() {
	flux python -c "import flux; flux.Flux().rpc(\"sched-fluxion-resource.${1}\").get()"
}

test_debug '
	echo ${grug} &&
	echo ${jobspec}
'

test_expect_success 'loading resource module with a tiny machine config works' '
	load_resource \
load-file=${grug} load-format=grug \
prune-filters=ALL:core subsystems=containment policy=high
'

test_expect_success 'set/get/remove property basic test works' "
	flux ion-resource set-property /tiny0/rack0/node0 class=one &&
	flux ion-resource get-property /tiny0/rack0/node0 class > sp.0 &&
	echo \"class = ['one']\" > expected &&
	test_cmp expected sp.0 &&
	flux ion-resource find property=class --format simple | grep node0 &&
	flux ion-resource find property=class --format simple -q | grep node0 &&
	flux ion-resource find property=class --format jgf -q | jq .graph.nodes | grep node0 &&
	flux ion-resource remove-property /tiny0/rack0/node0 class &&
	test_expect_code 3 flux ion-resource get-property /tiny0/rack0/node0 class &&
	flux ion-resource find property=class --format simple | test_must_fail grep node0 &&
	flux ion-resource find property=class --format jgf | test_must_fail grep node0
"

test_expect_success 'set/get/remove property multiple resources works' "
	flux ion-resource set-property /tiny0/rack0/node0 nodeprop=1 &&
	flux ion-resource set-property /tiny0/rack0/node0/socket1 sockprop=abc &&
	flux ion-resource set-property /tiny0/rack0/node1/socket0/core17 coreprop=z &&
	flux ion-resource get-property /tiny0/rack0/node0 nodeprop > sp.1 &&
	flux ion-resource get-property /tiny0/rack0/node0/socket1 sockprop >> sp.1 &&
	flux ion-resource get-property /tiny0/rack0/node1/socket0/core17 coreprop >> sp.1 &&
	cat <<-EOF >expected &&
	nodeprop = ['1']
	sockprop = ['abc']
	coreprop = ['z']
	EOF
	test_cmp expected sp.1 &&
	flux ion-resource find property=nodeprop --format simple | grep node0 &&
	flux ion-resource find property=sockprop --format jgf -q | jq .graph.nodes | grep socket1 &&
	flux ion-resource find property=coreprop --format simple | grep core17 &&
	flux ion-resource find 'property=coreprop or property=nodeprop or property=sockprop' \
		--format simple | grep core17 &&
	flux ion-resource find 'property=coreprop or property=nodeprop or property=sockprop' \
		--format simple | grep socket1 &&
	flux ion-resource find 'property=coreprop or property=nodeprop or property=sockprop' \
		--format jgf -q | jq .graph.nodes | grep node0 &&
	flux ion-resource remove-property /tiny0/rack0/node0 nodeprop &&
	flux ion-resource remove-property /tiny0/rack0/node0/socket1 sockprop &&
	flux ion-resource remove-property /tiny0/rack0/node1/socket0/core17 coreprop &&
	test_expect_code 3 flux ion-resource get-property /tiny0/rack0/node0 nodeprop &&
	flux ion-resource get-property /tiny0/rack0/node0 nodeprop \
		| grep \"Property 'nodeprop' was not found\" &&
	test_expect_code 3 flux ion-resource get-property /tiny0/rack0/node0/socket1 sockprop &&
	flux ion-resource get-property /tiny0/rack0/node0/socket1 sockprop \
		| grep \"Property 'sockprop' was not found\" &&
	test_expect_code 3 flux ion-resource get-property /tiny0/rack0/node1/socket0/core17 coreprop &&
	flux ion-resource get-property /tiny0/rack0/node1/socket0/core17 coreprop \
		| grep \"Property 'coreprop' was not found\" &&
	flux ion-resource find 'property=coreprop or property=nodeprop or property=sockprop' \
		--format simple | test_must_fail grep node0 &&
	flux ion-resource find property=sockprop --format simple | test_must_fail grep socket1 &&
	flux ion-resource find property=sockprop --format jgf | test_must_fail grep socket1
"

test_expect_success 'set/get/remove property multiple properties works' "
	flux ion-resource set-property /tiny0/rack0/node0 prop1=a &&
	flux ion-resource set-property /tiny0/rack0/node0 prop2=foo &&
	flux ion-resource set-property /tiny0/rack0/node0 prop3=123 &&
	flux ion-resource set-property /tiny0/rack0/node0 prop4=bar &&
	flux ion-resource set-property /tiny0/rack0/node0 prop5=baz &&
	flux ion-resource get-property /tiny0/rack0/node0 prop1 > sp.2 &&
	flux ion-resource get-property /tiny0/rack0/node0 prop2 >> sp.2 &&
	flux ion-resource get-property /tiny0/rack0/node0 prop3 >> sp.2 &&
	flux ion-resource get-property /tiny0/rack0/node0 prop4 >> sp.2 &&
	flux ion-resource get-property /tiny0/rack0/node0 prop5 >> sp.2 &&
	cat <<-EOF >expected &&
	prop1 = ['a']
	prop2 = ['foo']
	prop3 = ['123']
	prop4 = ['bar']
	prop5 = ['baz']
	EOF
	test_cmp expected sp.2 &&
	flux ion-resource find 'property=prop1 and property=prop2 and property=prop3 \
		and property=prop4 and property=prop5' --format simple | grep node0 &&
	flux ion-resource remove-property /tiny0/rack0/node0 prop1 &&
	test_expect_code 3 flux ion-resource get-property /tiny0/rack0/node0 prop1 &&
	flux ion-resource find 'property=prop1 and property=prop2 and property=prop3 \
		and property=prop4 and property=prop5' --format simple | test_must_fail grep node0 &&
	flux ion-resource find 'property=prop2 and property=prop3 and property=prop4 \
		and property=prop5' --format simple | grep node0 &&
	flux ion-resource get-property /tiny0/rack0/node0 prop2 | grep foo &&
	flux ion-resource remove-property /tiny0/rack0/node0 prop2 &&
	test_expect_code 3 flux ion-resource get-property /tiny0/rack0/node0 prop2 &&
	flux ion-resource find 'property=prop2 and property=prop3 and property=prop4 \
		and property=prop5' --format simple | test_must_fail grep node0 &&
	flux ion-resource find 'property=prop3 and property=prop4 and property=prop5' \
		--format simple | grep node0 &&
	flux ion-resource get-property /tiny0/rack0/node0 prop3 | grep 123 &&
	flux ion-resource remove-property /tiny0/rack0/node0 prop3 &&
	test_expect_code 3 flux ion-resource get-property /tiny0/rack0/node0 prop3 &&
	flux ion-resource find 'property=prop3 and property=prop4 and property=prop5' \
		--format simple | test_must_fail grep node0 &&
	flux ion-resource find 'property=prop4 and property=prop5' --format simple | grep node0 &&
	flux ion-resource get-property /tiny0/rack0/node0 prop4 | grep bar &&
	flux ion-resource get-property /tiny0/rack0/node0 prop5 | grep baz &&
	flux ion-resource remove-property /tiny0/rack0/node0 prop4 &&
	test_expect_code 3 flux ion-resource get-property /tiny0/rack0/node0 prop4 &&
	flux ion-resource find 'property=prop4 and property=prop5' \
		--format simple | test_must_fail grep node0 &&
	flux ion-resource find 'property=prop5' --format simple | grep node0 &&
	flux ion-resource remove-property /tiny0/rack0/node0 prop5 &&
	test_expect_code 3 flux ion-resource get-property /tiny0/rack0/node0 prop5 &&
	flux ion-resource find 'property=prop5' --format simple | test_must_fail grep node0
"

test_expect_success 'test with no path works' "
	test_expect_code 3 flux ion-resource set-property /dont/exist random=1 &&
	flux ion-resource set-property /dont/exist random=1 | grep \"Couldn't find '/dont/exist'\" &&
	test_expect_code 3 flux ion-resource get-property /dont/exist random &&
	flux ion-resource get-property /dont/exist random | grep \"Couldn't find '/dont/exist'\" &&
	test_expect_code 3 flux ion-resource remove-property /dont/exist random &&
	flux ion-resource remove-property /dont/exist random | grep \"Couldn't find '/dont/exist'\"
"

test_expect_success 'test with no property works' '
	test_expect_code 3 flux ion-resource get-property /tiny0/rack0/node0 dontexist &&
	flux ion-resource remove-property /tiny0/rack0/node0 dontexist
'

test_expect_success 'test with malformed inputs works' '
	test_expect_code 1 flux ion-resource set-property /tiny0/rack0/node0 badprop &&
	flux ion-resource set-property /tiny0/rack0/node0 badprop | grep "Incorrect format" &&
	test_expect_code 3 flux ion-resource get-property /tiny0/rack0/node0 badprop &&
	test_expect_code 1 flux ion-resource set-property /tiny0/rack0/node0 badprop= &&
	flux ion-resource set-property /tiny0/rack0/node0 badprop= | grep "Incorrect format" &&
	test_expect_code 3 flux ion-resource get-property /tiny0/rack0/node0 badprop &&
	test_expect_code 1 flux ion-resource set-property /tiny0/rack0/node0 =badprop &&
	flux ion-resource set-property /tiny0/rack0/node0 =badprop | grep "Incorrect format" &&
	test_expect_code 3 flux ion-resource get-property /tiny0/rack0/node0 badprop &&
	test_expect_code 1 flux ion-resource set-property /tiny0/rack0/node0 = &&
	flux ion-resource set-property /tiny0/rack0/node0 = | grep "Incorrect format" &&
	test_expect_code 3 flux ion-resource get-property /tiny0/rack0/node0 badprop
'

test_expect_success 'test with complex inputs works' "
	flux ion-resource set-property /tiny0/rack0/node0 badprop==1 &&
	flux ion-resource get-property /tiny0/rack0/node0 badprop > sp.5 &&
	flux ion-resource set-property /tiny0/rack0/node0 badprop=1=class=random &&
	flux ion-resource get-property /tiny0/rack0/node0 badprop >> sp.5 &&
	flux ion-resource set-property /tiny0/rack0/node0 badprop=1 &&
	flux ion-resource get-property /tiny0/rack0/node0 badprop >> sp.5 &&
	cat <<-EOF >expected &&
	badprop = ['=1']
	badprop = ['1=class=random']
	badprop = ['1']
	EOF
	test_cmp expected sp.5
"

test_expect_success 'tests fail when payloads are missing' '
	test_expect_code 1 send_rpc get_property 2>&1 &&
	test_expect_code 1 send_rpc set_property 2>&1 &&
	test_expect_code 1 send_rpc remove_property 2>&1 &&
	send_rpc get_property 2>&1 | grep "could not unpack payload" &&
	send_rpc set_property 2>&1 | grep "could not unpack payload" &&
	send_rpc remove_property 2>&1 | grep "could not unpack payload"
'

test_expect_success 'removing resource works' '
	remove_resource
'

test_done

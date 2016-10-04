#!/bin/sh

test_description='Test flux-rdltool 

Test rdl C interface using flux-rdltool frontend utility
'
. `dirname $0`/sharness.sh

rdltool=$SHARNESS_BUILD_DIRECTORY/rdl/flux-rdltool
conf=$SHARNESS_TEST_SRCDIR/data/rdl/hype.lua

test_expect_success  'flux-rdltool exists' '
	test -x $rdltool
'
test_expect_success  'flux-rdltool resource works' '
	$rdltool -f $conf resource default://
'
test_expect_success  'flux-rdltool tree works' '
	$rdltool -f $conf tree default://hype/hype201 >output.tree &&
	cat <<-EOF >expected.tree &&
/hype201
 /socket0
  /core0
  /memory
  /core1
  /core2
  /core3
  /core4
  /core5
  /core6
  /core7
 /socket1
  /memory
  /core8
  /core9
  /core10
  /core11
  /core12
  /core13
  /core14
  /core15
EOF
	test_cmp expected.tree output.tree
'
test_expect_success  'flux-rdltool aggregate works' '
	$rdltool -f $conf aggregate default:// >output.aggregate &&
	cat >expected.aggregate <<-EOF &&
	default://:
	{ "core": 2464, "socket": 308, "cluster": 1, "node": 154, "memory": 4620000 }
	EOF
	test_cmp expected.aggregate output.aggregate
'
test_expect_success  'flux-rdltool list-hierarchies works' '
	$rdltool -f $conf list-hierarchies | sort > output.hierarchies &&
	cat >expected.hierarchies <<-EOF &&
	bandwidth
	default
	power
	EOF
	test_cmp expected.hierarchies output.hierarchies
'

test_done

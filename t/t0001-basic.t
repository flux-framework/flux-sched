#!/bin/sh
#set -x

test_description='Test basic schedsvr usage in flux session

Ensure the very basics of flux schedsvr work.
'

#
# variables
#
dn=`dirname $0` 
tdir=`readlink -e $dn/../`
schedsrv=`readlink -e $dn/../sched/.libs/schedsrv.so`
rdlconf=`readlink -e $dn/../conf/hype.lua`

#
# source sharness from the directore where this test
# file resides
#
. ${dn}/sharness.sh

#
# print only with --debug
#
test_debug '
	echo ${tdir} &&
	echo ${schedsrv} &&
	echo ${rdlconf}
'

#
# test_under_flux is under sharness.d/
#
test_under_flux 1 $tdir


test_expect_success 'schedsrv: module load works' '
	flux module load ${schedsrv} rdl-conf=${rdlconf}
'

test_expect_success 'schedsrv: module remove works' '
	flux module remove sched
'

test_expect_success 'schedsrv: flux-module load works after a successful unload' '
	flux module load ${schedsrv} rdl-conf=${rdlconf} &&
	flux module remove sched
'

# comment this one out for now
#test_expect_success 'schedsrv: module load should fail' '
#	test_expect_code 1 flux module load ${schedsrv} 
#'

test_expect_success 'schedsrv: module load works after a load failure' '
	flux module load ${schedsrv} rdl-conf=${rdlconf}
'

test_expect_success 'schedsrv: module list works' '
	flux module list
'
test_done

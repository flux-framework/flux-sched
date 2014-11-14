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
schedsrv=`readlink -e $dn/../sched/schedsrv.so`
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
# test_under_flux is under shareness.d/
#
test_under_flux 1 $tdir


test_expect_success 'flux-module load works' '
	flux module load ${schedsrv} rdl-conf=${rdlconf}
'

test_expect_success 'flux-module list' '
	flux module list
'


#
# Seems to hang!
#
#test_expect_success 'flux-module rm' '
#	flux module rm sched
#'

test_done

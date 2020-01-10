#!/bin/sh
#set -x

test_description='Test resource-match using hwloc resource information

Ensure that the match (allocate) handler within the resource module works
'

ORIG_HOME=${HOME}

. `dirname $0`/sharness.sh

#
# sharness modifies $HOME environment variable, but this interferes
# with python's package search path, in particular its user site package.
#
HOME=${ORIG_HOME}

jobspec_basepath=`readlink -e ${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/`
# slot[1]->core[1]
jobspec_1core="${jobspec_basepath}/basics/test008.yaml"
# node[1]->slot[2]->numanode[1]->socket[1]
jobspec_2socket="${jobspec_basepath}/basics/test009.yaml"

# node[1]->slot[1]->socket[1]->core[22]
#                 ->gpu[2]
jobspec_1socket_2gpu="${jobspec_basepath}/basics/test013.yaml"

hwloc_basepath=`readlink -e ${SHARNESS_TEST_SRCDIR}/data/hwloc-data`
# 1 broker: 1 node, 1 socket, 4 cores
hwloc_4core="${hwloc_basepath}/001N/exclusive/04-brokers/0.xml"
# 4 brokers, each (exclusively) have: 1 node, 2 sockets, 16 cores (8 per socket)
excl_4N4B="${hwloc_basepath}/004N/exclusive/04-brokers"
# 4 brokers, each (exclusively) have: 1 node, 2 numanodes, 4 gpus, 44 cores, 176 PUs
excl_4N4B_sierra2="${hwloc_basepath}/004N/exclusive/04-brokers-sierra2"
# 1 broker: 1 node, 2 numanodes, 4 gpus, 44 cores, 176 PUs
hwloc_4gpu="${hwloc_basepath}/004N/exclusive/04-brokers-sierra2/0.xml"
# 1 broker: 1 node, 2 numanodes, 4 AMD gpus, 48 cores, 96 PUs
hwloc_4amdgpu="${hwloc_basepath}/001N/amd_gpu/corona11.xml"
# 1 broker: 1 node, 2 numanodes, 2 gpus (1 NVIDIA and 1 AMD), 32 cores, 64 PUs
hwloc_2mtypes="${hwloc_basepath}/001N/multi_gpu_types/chimera.xml"


query="../../resource/utilities/resource-query"
cmd_dir="${SHARNESS_TEST_SRCDIR}/data/resource/commands/basics"
exp_dir="${SHARNESS_TEST_SRCDIR}/data/resource/expected/basics"

#
# test_under_flux is under sharness.d/
#
test_under_flux 4

#
# print only with --debug
#
test_debug '
    echo ${jobspec_1core} &&
    echo ${jobspec_2socket} &&
    echo ${hwloc_4core} &&
    echo ${excl_4N4B}
'

# Test using the resource query command
test_expect_success 'resource-query works on simple query using xml file' '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmd_dir}/cmds09.in > cmds09 &&
    ${query} -L ${hwloc_4core} -f hwloc -S CA -P high -t 017.R.out < cmds09 &&
    test_cmp 017.R.out ${exp_dir}/017.R.out
'

test_expect_success 'resource-query works on gpu query using xml (NVIDIA)' '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmd_dir}/cmds10.in > cmds10 &&
    ${query} -L ${hwloc_4gpu} -f hwloc -S CA -P high -t 018.R.out < cmds10 &&
    test_cmp 018.R.out ${exp_dir}/018.R.out
'

test_expect_success 'resource-query works on gpu query using xml (AMD GPU)' '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmd_dir}/cmds11.in > cmds11 &&
    ${query} -L ${hwloc_4amdgpu} -f hwloc -S CA -P high -t 019.R.out < cmds11 &&
    test_cmp 019.R.out ${exp_dir}/019.R.out
'

test_expect_success 'resource-query works on gpu type query using xml (MIXED)' '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmd_dir}/cmds11.in > cmds11 &&
    ${query} -L ${hwloc_2mtypes} -f hwloc -S CA -P high -t 020.R.out < cmds11 &&
    test_cmp 020.R.out ${exp_dir}/020.R.out
'

test_expect_success 'resource-query works with whitelist' '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmd_dir}/cmds12.in > cmds12 &&
    ${query} -L ${hwloc_4gpu} -f hwloc -S CA -P high -W node,socket,core,gpu -t 021.R.out < cmds12 &&
    test_cmp 021.R.out ${exp_dir}/021.R.out
'

# Test using the full resource matching service
test_expect_success 'loading resource module with a tiny hwloc xml file works' '
    flux module load resource load-file=${hwloc_4core}
'

test_expect_success 'match-allocate works with four one-core jobspecs' '
    flux resource match allocate ${jobspec_1core} &&
    flux resource match allocate ${jobspec_1core} &&
    flux resource match allocate ${jobspec_1core} &&
    flux resource match allocate ${jobspec_1core}
'

test_expect_success 'match-allocate fails when all resources are allocated' '
    test_expect_code 16 flux resource match allocate ${jobspec_1core} &&
    test_expect_code 16 flux resource match allocate ${jobspec_1core} &&
    test_expect_code 16 flux resource match allocate ${jobspec_1core} &&
    test_expect_code 16 flux resource match allocate ${jobspec_1core}
'

test_expect_success 'removing resource works' '
    flux module remove resource
'

test_expect_success 'reloading session/hwloc information with test data' '
    flux hwloc reload ${excl_4N4B}
'

test_expect_success 'loading resource module with default resource info source' '
    flux module load resource subsystems=containment policy=high
'

test_expect_success 'match-allocate works with four two-socket jobspecs' '
    flux resource match allocate ${jobspec_2socket} &&
    flux resource match allocate ${jobspec_2socket} &&
    flux resource match allocate ${jobspec_2socket} &&
    flux resource match allocate ${jobspec_2socket}
'

test_expect_success 'match-allocate fails when all resources are allocated' '
    test_expect_code 16 flux resource match allocate ${jobspec_2socket} &&
    test_expect_code 16 flux resource match allocate ${jobspec_2socket} &&
    test_expect_code 16 flux resource match allocate ${jobspec_2socket} &&
    test_expect_code 16 flux resource match allocate ${jobspec_2socket}
'

test_expect_success 'reloading sierra xml and match allocate' '
    flux module remove resource &&
    flux hwloc reload ${excl_4N4B_sierra2} &&
    flux module load resource subsystems=containment \
policy=high load-whitelist=node,socket,core,gpu &&
    flux resource match allocate ${jobspec_1socket_2gpu} &&
    flux resource match allocate ${jobspec_1socket_2gpu} &&
    flux resource match allocate ${jobspec_1socket_2gpu} &&
    flux resource match allocate ${jobspec_1socket_2gpu} &&
    flux resource match allocate ${jobspec_1socket_2gpu} &&
    flux resource match allocate ${jobspec_1socket_2gpu} &&
    flux resource match allocate ${jobspec_1socket_2gpu} &&
    flux resource match allocate ${jobspec_1socket_2gpu} &&
    test_must_fail flux resource match allocate ${jobspec_1socket_2gpu}
'

test_expect_success 'removing resource works' '
    flux module remove resource
'

test_done

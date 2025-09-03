#!/bin/sh
#set -x

test_description='Test resource-match using hwloc resource information

Ensure that the match (allocate) handler within the resource module works
'

. `dirname $0`/sharness.sh

jobspec_basepath=`readlink -f ${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/`
# slot[1]->core[1]
jobspec_1core="${jobspec_basepath}/basics/test008.yaml"
# node[1]->slot[2]->socket[1]
jobspec_2socket="${jobspec_basepath}/basics/test009.yaml"

# node[1]->slot[1]->socket[1]->core[22]
#                 ->gpu[2]
jobspec_1socket_2gpu="${jobspec_basepath}/basics/test013.yaml"

hwloc_basepath=`readlink -f ${SHARNESS_TEST_SRCDIR}/data/hwloc-data`
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
# 1 broker: 1 node, 2 numanodes, 8 AMD RSMI gpus
hwloc_4rsmigpu="${hwloc_basepath}/001N/amd_gpu/rsmi_corona240.xml"
# 1 broker: 1 node, 2 numanodes, 2 gpus (1 NVIDIA and 1 AMD), 32 cores, 64 PUs
hwloc_2mtypes="${hwloc_basepath}/001N/multi_gpu_types/chimera.xml"


query="../../resource/utilities/resource-query"
cmd_dir="${SHARNESS_TEST_SRCDIR}/data/resource/commands/basics"
exp_dir="${SHARNESS_TEST_SRCDIR}/data/resource/expected/basics"

#
# test_under_flux is under sharness.d/
#
export FLUX_SCHED_MODULE=none
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
    ${query} -L ${hwloc_4core} -f hwloc -S CA -P high -W node,socket,core -t 017.R.out < cmds09 &&
    test_cmp 017.R.out ${exp_dir}/017.R.out
'

test_expect_success 'resource-query works on gpu query using xml (NVIDIA)' '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmd_dir}/cmds10.in > cmds10 &&
    ${query} -L ${hwloc_4gpu} -f hwloc -S CA -P high -W node,socket,core,gpu -t 018.R.out < cmds10 &&
    test_cmp 018.R.out ${exp_dir}/018.R.out
'

test_expect_success 'resource-query works on gpu query using xml (AMD GPU)' '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmd_dir}/cmds11.in > cmds11 &&
    ${query} -L ${hwloc_4amdgpu} -f hwloc -S CA -P high -W node,socket,core,gpu -t 019.R.out < cmds11 &&
    test_cmp 019.R.out ${exp_dir}/019.R.out
'

test_expect_success 'resource-query works on gpu query using xml (AMD RSMI GPU)' '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmd_dir}/cmds11.in > cmds11 &&
    ${query} -L ${hwloc_4rsmigpu} -f hwloc -S CA -P high -W node,socket,core,gpu -t 019.2.R.out < cmds11 &&
    test_cmp 019.2.R.out ${exp_dir}/019.2.R.out
'
test_expect_success 'resource-query works on gpu type query using xml (MIXED)' '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmd_dir}/cmds11.in > cmds11 &&
    ${query} -L ${hwloc_2mtypes} -f hwloc -S CA -P high -W node,socket,core,gpu -t 020.R.out < cmds11 &&
    test_cmp 020.R.out ${exp_dir}/020.R.out
'

test_expect_success 'resource-query works with allowlist' '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmd_dir}/cmds12.in > cmds12 &&
    ${query} -L ${hwloc_4gpu} -f hwloc -S CA -P high -W node,socket,core,gpu -t 021.R.out < cmds12 &&
    test_cmp 021.R.out ${exp_dir}/021.R.out
'

# Test using the full resource matching service
test_expect_success 'loading resource module with a tiny hwloc xml file works' '
    load_resource load-file=${hwloc_4core} load-format=hwloc
'

test_expect_success 'match-allocate works with four one-core jobspecs' '
    flux ion-resource match allocate ${jobspec_1core} &&
    flux ion-resource match allocate ${jobspec_1core} &&
    flux ion-resource match allocate ${jobspec_1core} &&
    flux ion-resource match allocate ${jobspec_1core}
'

test_expect_success 'match-allocate fails when all resources are allocated' '
    test_expect_code 16 flux ion-resource match allocate ${jobspec_1core} &&
    test_expect_code 16 flux ion-resource match allocate ${jobspec_1core} &&
    test_expect_code 16 flux ion-resource match allocate ${jobspec_1core} &&
    test_expect_code 16 flux ion-resource match allocate ${jobspec_1core}
'

test_expect_success 'removing resource works' '
    remove_resource
'

test_expect_success HAVE_GETXML 'load test resources (4N4B)' '
    load_test_resources ${excl_4N4B}
'

test_expect_success HAVE_GETXML 'loading resource module with default resource info source' '
    load_resource subsystems=containment policy=high \
	load-format=hwloc load-allowlist=node,socket,core
'

test_expect_success HAVE_GETXML 'match-allocate works with four two-socket jobspecs' '
    flux ion-resource match allocate ${jobspec_2socket} &&
    flux ion-resource match allocate ${jobspec_2socket} &&
    flux ion-resource match allocate ${jobspec_2socket} &&
    flux ion-resource match allocate ${jobspec_2socket}
'

test_expect_success HAVE_GETXML 'match-allocate fails when all resources are allocated' '
    test_expect_code 16 flux ion-resource match allocate ${jobspec_2socket} &&
    test_expect_code 16 flux ion-resource match allocate ${jobspec_2socket} &&
    test_expect_code 16 flux ion-resource match allocate ${jobspec_2socket} &&
    test_expect_code 16 flux ion-resource match allocate ${jobspec_2socket}
'

test_expect_success HAVE_GETXML 'unload fluxion resource' '
    remove_resource
'

test_expect_success HAVE_GETXML 'load test resources (4N4B_sierra2)' '
    load_test_resources ${excl_4N4B_sierra2}
'

test_expect_success HAVE_GETXML 'load fluxion resource' '
    load_resource subsystems=containment policy=high \
	load-format=hwloc load-allowlist=node,socket,core,gpu
'

test_expect_success HAVE_GETXML 'match allocate' '
    flux ion-resource match allocate ${jobspec_1socket_2gpu} &&
    flux ion-resource match allocate ${jobspec_1socket_2gpu} &&
    flux ion-resource match allocate ${jobspec_1socket_2gpu} &&
    flux ion-resource match allocate ${jobspec_1socket_2gpu} &&
    flux ion-resource match allocate ${jobspec_1socket_2gpu} &&
    flux ion-resource match allocate ${jobspec_1socket_2gpu} &&
    flux ion-resource match allocate ${jobspec_1socket_2gpu} &&
    flux ion-resource match allocate ${jobspec_1socket_2gpu} &&
    test_expect_code 16 flux ion-resource match allocate ${jobspec_1socket_2gpu}
'

test_expect_success HAVE_GETXML 'removing resource works' '
    remove_resource
'

test_done

#!/bin/sh

test_description='Test Id Namespace Remapping for Nested Instances'

. `dirname $0`/sharness.sh

hwloc_basepath=`readlink -e ${SHARNESS_TEST_SRCDIR}/data/hwloc-data`
# 1 brokers: 1 node, 2 sockets, 44 cores 4 gpus
excl_1N1B="${hwloc_basepath}/001N/exclusive/01-brokers-sierra2"

export FLUX_SCHED_MODULE=none

test_under_flux 1

test_expect_success 'namespace: load test resources' '
    load_test_resources ${excl_1N1B}
'

test_expect_success 'namespace: loading resource and qmanager modules works' '
    load_resource load-allowlist=cluster,node,gpu,core policy=high &&
    load_qmanager
'

test_expect_success 'namespace: gpu id remapping works with hwloc (pol=hi)' '
    cat >nest.sh <<-EOF &&
	#!/bin/sh
	flux module load sched-fluxion-resource load-allowlist=cluster,node,gpu,core policy=high
	flux module load sched-fluxion-qmanager
	flux resource list
	flux ion-resource ns-info 0 gpu 0
	flux ion-resource ns-info 0 gpu 1
	echo \${CUDA_VISIBLE_DEVICES}
EOF
    cat >exp1 <<-EOF &&
	2
	3
	2,3
EOF
    chmod u+x nest.sh &&
    jobid=$(flux mini batch --output=kvs -n1 -N1 -c22 -g2 ./nest.sh) &&
    flux job wait-event -t10 ${jobid} release &&
    flux job attach ${jobid} > out1.a &&
    tail -3 out1.a > out1.a.fin &&
    diff out1.a.fin exp1
'

test_expect_success 'namespace: parent CUDA_VISIBLE_DEVICES has no effect' '
    export CUDA_VISIBLE_DEVICES="0,1,2,3" &&
    jobid=$(flux mini batch --output=kvs -n1 -N1 -c22 -g2 ./nest.sh) &&
    flux job wait-event -t10 ${jobid} release &&
    flux job attach ${jobid} > out1.b &&
    tail -3 out1.b > out1.b.fin &&
    diff out1.b.fin exp1
'

test_expect_success 'namespace: removing resource and qmanager modules' '
    remove_resource
'

test_expect_success 'namespace: loading resource and qmanager modules works' '
    load_resource load-allowlist=cluster,node,gpu,core policy=low &&
    load_qmanager
'

test_expect_success 'namespace: gpu id remapping works with hwloc (pol=low)' '
    cat >exp2 <<-EOF &&
	0
	1
	0,1
EOF
    jobid=$(flux mini batch --output=kvs -n1 -N1 -c22 -g2 ./nest.sh) &&
    flux job wait-event -t10 ${jobid} release &&
    flux job attach ${jobid} > out2.a &&
    tail -3 out2.a > out2.a.fin &&
    diff out2.a.fin exp2
'

test_expect_success 'namespace: parent CUDA_VISIBLE_DEVICES has no effect' '
    export CUDA_VISIBLE_DEVICES=-1 &&
    jobid=$(flux mini batch --output=kvs -n1 -N1 -c22 -g2 ./nest.sh) &&
    flux job wait-event -t10 ${jobid} release &&
    flux job attach ${jobid} > out2.b &&
    tail -3 out2.b > out2.b.fin &&
    diff out2.b.fin exp2
'

test_expect_success 'namespace: removing resource and qmanager modules' '
    remove_resource
'

test_done

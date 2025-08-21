#!/bin/sh

test_description='Test Id Namespace Remapping for Nested Instances'

. `dirname $0`/sharness.sh

hwloc_basepath=`readlink -f ${SHARNESS_TEST_SRCDIR}/data/hwloc-data`
# 1 brokers: 1 node, 2 sockets, 44 cores 4 gpus
excl_1N1B="${hwloc_basepath}/001N/exclusive/01-brokers-sierra2"

export FLUX_SCHED_MODULE=none

test_under_flux 1

test_expect_success 'namespace: load test resources' '
    load_test_resources ${excl_1N1B}
'

test_expect_success HAVE_GETXML 'namespace: loading resource and qmanager modules works' '
    load_resource load-allowlist=cluster,node,gpu,core policy=high &&
    load_qmanager
'

test_expect_success HAVE_GETXML 'namespace: gpu id remapping works with hwloc (pol=hi)' '
    cat >nest.sh <<-EOF &&
	#!/bin/sh
	flux module load sched-fluxion-resource load-format=hwloc load-allowlist=cluster,node,gpu,core policy=high
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
    jobid=$(flux batch --output=kvs -n1 -N1 -c22 -g2 ./nest.sh) &&
    flux job wait-event -t10 ${jobid} release &&
    flux job attach ${jobid} > out1.a &&
    tail -3 out1.a > out1.a.fin &&
    diff out1.a.fin exp1
'

test_expect_success HAVE_GETXML 'namespace: parent CUDA_VISIBLE_DEVICES has no effect' '
    export CUDA_VISIBLE_DEVICES="0,1,2,3" &&
    jobid=$(flux batch --output=kvs -n1 -N1 -c22 -g2 ./nest.sh) &&
    flux job wait-event -t10 ${jobid} release &&
    flux job attach ${jobid} > out1.b &&
    tail -3 out1.b > out1.b.fin &&
    diff out1.b.fin exp1
'

test_expect_success HAVE_GETXML 'namespace: removing resource and qmanager modules' '
    remove_resource
'

test_expect_success HAVE_GETXML 'namespace: loading resource and qmanager modules works' '
    load_resource load-format=hwloc load-allowlist=cluster,node,gpu,core policy=low &&
    load_qmanager
'

test_expect_success HAVE_GETXML 'namespace: gpu id remapping works with hwloc (pol=low)' '
    cat >exp2 <<-EOF &&
	0
	1
	0,1
EOF
    jobid=$(flux batch --output=kvs -n1 -N1 -c22 -g2 ./nest.sh) &&
    flux job wait-event -t10 ${jobid} release &&
    flux job attach ${jobid} > out2.a &&
    tail -3 out2.a > out2.a.fin &&
    diff out2.a.fin exp2
'

test_expect_success HAVE_GETXML 'namespace: parent CUDA_VISIBLE_DEVICES has no effect' '
    export CUDA_VISIBLE_DEVICES=-1 &&
    jobid=$(flux batch --output=kvs -n1 -N1 -c22 -g2 ./nest.sh) &&
    flux job wait-event -t10 ${jobid} release &&
    flux job attach ${jobid} > out2.b &&
    tail -3 out2.b > out2.b.fin &&
    diff out2.b.fin exp2
'

test_expect_success HAVE_GETXML 'namespace: removing resource and qmanager modules' '
    remove_resource
'

test_expect_success 'namespace: loading resource and qmanager modules works' '
    load_resource policy=high &&
    load_qmanager
'

test_expect_success 'namespace: gpu id remapping works with rv1exec (pol=hi)' '
    cat >nest2.sh <<-EOF &&
	#!/bin/sh
	flux module load sched-fluxion-resource policy=high
	flux module load sched-fluxion-qmanager
	flux module stats sched-fluxion-qmanager
	flux resource list
	echo \${CUDA_VISIBLE_DEVICES}
EOF
    cat >exp3 <<-EOF &&
	2,3
EOF
    chmod u+x nest2.sh &&
    jobid=$(flux batch --output=kvs -n1 -N1 -c22 -g2 ./nest2.sh) &&
    flux job wait-event -t10 ${jobid} release &&
    flux job attach ${jobid} > out3.a &&
    tail -1 out3.a > out3.a.fin &&
    diff out3.a.fin exp3
'

test_expect_success 'namespace: parent CUDA_VISIBLE_DEVICES has no effect' '
    export CUDA_VISIBLE_DEVICES="0,1,2,3" &&
    jobid=$(flux batch --output=kvs -n1 -N1 -c22 -g2 ./nest2.sh) &&
    flux job wait-event -t10 ${jobid} release &&
    flux job attach ${jobid} > out3.b &&
    tail -1 out3.b > out3.b.fin &&
    diff out3.b.fin exp3
'

test_expect_success 'namespace: removing resource and qmanager modules' '
    remove_resource
'

test_expect_success 'namespace: loading resource and qmanager modules works' '
    load_resource policy=low &&
    load_qmanager
'

test_expect_success 'namespace: gpu id remapping works with rv1exec (pol=hi)' '
    cat >exp4 <<-EOF &&
	0,1
EOF
    jobid=$(flux batch --output=kvs -n1 -N1 -c22 -g2 ./nest2.sh) &&
    flux job wait-event -t10 ${jobid} release &&
    flux job attach ${jobid} > out4.a &&
    tail -1 out4.a > out4.a.fin &&
    diff out4.a.fin exp4
'

test_expect_success 'namespace: parent CUDA_VISIBLE_DEVICES has no effect' '
    export CUDA_VISIBLE_DEVICES="0,1,2,3" &&
    jobid=$(flux batch --output=kvs -n1 -N1 -c22 -g2 ./nest2.sh) &&
    flux job wait-event -t10 ${jobid} release &&
    flux job attach ${jobid} > out4.b &&
    tail -1 out4.b > out4.b.fin &&
    diff out4.b.fin exp4
'

test_expect_success 'namespace: removing resource and qmanager modules' '
    remove_resource
'

test_done

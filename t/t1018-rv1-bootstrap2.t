#!/bin/sh

test_description='Test Bootstrapping from Configured RV1 Object'

. `dirname $0`/sharness.sh

export WAITFILE="${SHARNESS_TEST_SRCDIR}/scripts/waitfile.lua"
export FLUX_SCHED_MODULE=none

test_under_flux 4

# Purposefully create the same configuration as with t1017-rv1-bootstrap.t
# However, node selections are different this time as node Ids are filled
test_expect_success 'rv1-bootstrap2: create test R with unlikely core count' '
    flux R encode --hosts=sierra[3682,3179,3683,3178] --cores=0-43 --gpu=0-3 \
	| flux ion-R encode > sierra.R.test
'

test_expect_success 'rv1-bootstrap2: reload the configured R' '
    flux resource reload $(pwd)/sierra.R.test
'

test_expect_success 'rv1-bootstrap2: loading resource/qmanager modules works' '
    load_resource load-allowlist=cluster,node,gpu,core \
match-format=rv1 policy=high &&
    load_qmanager
'

test_expect_success 'rv1-bootstrap2: creating a nested batch script' '
    cat >nest.sh <<-EOF &&
	#!/bin/sh
	flux module load sched-fluxion-resource \
load-allowlist=cluster,node,gpu,core match-format=rv1 policy=\$1
	flux module load sched-fluxion-qmanager
	nested_jobid=\$(flux submit -n\$2 -N\$3 -c\$4 -g\$5 sleep 0)
	flux job wait-event -t10 \${nested_jobid} start
	flux job info \${nested_jobid} R > \$6
	sleep inf
EOF
    chmod u+x nest.sh
'

# Idempotency check
test_expect_success 'rv1-bootstrap: resource idempotency preserved' '
    JOBID=$(flux batch -n4 -N4 -c44 -g4 \
	./nest.sh high 4 4 44 4 nest.json) &&
    $WAITFILE -t 20 -v -p \"R_lite\" nest.json &&
    jq -S " del(.execution.starttime, .execution.expiration) " \
	nest.json > nest.norm.json &&
    flux job info ${JOBID} R | \
    jq -S " del(.execution.starttime, .execution.expiration) " \
	> job.norm.json &&
    test_cmp job.norm.json nest.norm.json
'

# Cancel jobs
test_expect_success 'rv1-bootstrap2: killing a nested job works' '
    flux cancel ${JOBID} &&
    flux job wait-event ${JOBID} release
'

# 2 full nodes are scheduled for a nest flux instance -- requiring no remap
test_expect_success 'rv1-bootstrap2: 2N nesting works (policy=high)' '
    JOBID1=$(flux batch -n2 -N2 -c44 -g4 \
	./nest.sh high 2 2 44 4 nest1.json) &&
    $WAITFILE -t 20 -v -p \"R_lite\" nest1.json &&
    remap_rv1_resource_type nest1.json core > nest1.csv &&
    remap_rv1_resource_type nest1.json gpu > nest1.gpu.csv &&
    flux job info ${JOBID1} R | jq . > job1.json &&
    remap_rv1_resource_type job1.json core 0 0 0 1 0 > job1.csv &&
    remap_rv1_resource_type job1.json gpu 0 0 0 1 0 > job1.gpu.csv &&
    test_cmp job1.csv nest1.csv &&
    test_cmp job1.gpu.csv nest1.gpu.csv
'

# 2 nodes each with partial resource set using match policy=high
test_expect_success 'rv1-bootstrap2: 2 partial node nesting works (high)' '
    JOBID2=$(flux batch -n2 -N2 -c10 -g2 \
	./nest.sh high 2 2 10 2 nest2.json) &&
    $WAITFILE -t 20 -v -p \"R_lite\" nest2.json &&
    flux job info ${JOBID2} R | jq . > job2.json &&
    remap_rv1_resource_type nest2.json core > nest2.csv &&
    remap_rv1_resource_type nest2.json gpu > nest2.gpu.csv &&
    remap_rv1_resource_type job2.json core 34 0 1 0 2 > job2.csv &&
    remap_rv1_resource_type job2.json gpu 0 0 1 0 2 > job2.gpu.csv &&
    test_cmp job2.csv nest2.csv &&
    test_cmp job2.gpu.csv nest2.gpu.csv
'

# Cancel jobs
test_expect_success 'rv1-bootstrap2: killing nested jobs works' '
    flux cancel ${JOBID1} &&
    flux cancel ${JOBID2} &&
    flux job wait-event -t20 ${JOBID1} release &&
    flux job wait-event -t20 ${JOBID2} release
'

test_expect_success 'rv1-bootstrap2: remove fluxion schedulers' '
    remove_qmanager &&
    remove_resource
'

test_expect_success 'rv1-bootstrap2: loading fluxion modules works (pol=low)' '
    load_resource load-allowlist=cluster,node,gpu,core \
match-format=rv1 policy=low &&
    load_qmanager
'

# 2 full nodes are scheduled for a nest flux instance -- requiring no remap
test_expect_success 'rv1-bootstrap2: 2N nesting works (policy=low)' '
    JOBID3=$(flux batch -n2 -N2 -c44 -g4 \
	./nest.sh low 2 2 44 4 nest3.json) &&
    $WAITFILE -t 20 -v -p \"R_lite\" nest3.json &&
    remap_rv1_resource_type nest3.json core > nest3.csv &&
    remap_rv1_resource_type nest3.json gpu > nest3.gpu.csv &&
    flux job info ${JOBID3} R | jq . > job3.json &&
    remap_rv1_resource_type job3.json core 0 0 1 0 2 > job3.csv &&
    remap_rv1_resource_type job3.json gpu 0 0 1 0 2 > job3.gpu.csv &&
    test_cmp job3.csv nest3.csv &&
    test_cmp job3.gpu.csv nest3.gpu.csv
'

test_expect_success 'rv1-bootstrap2: 2 partial node nesting works (low)' '
    JOBID4=$(flux batch -n2 -N2 -c10 -g2 \
	./nest.sh low 2 2 10 2 nest4.json) &&
    $WAITFILE -t 20 -v -p \"R_lite\" nest4.json &&
    flux job info ${JOBID4} R | jq . > job4.json &&
    remap_rv1_resource_type nest4.json core > nest4.csv &&
    remap_rv1_resource_type nest4.json gpu > nest4.gpu.csv &&
    remap_rv1_resource_type job4.json core 0 0 0 1 0 > job4.csv &&
    remap_rv1_resource_type job4.json gpu 0 0 0 1 0 > job4.gpu.csv &&
    test_cmp job4.csv nest4.csv &&
    test_cmp job4.gpu.csv nest4.gpu.csv
'

# Cancel jobs
test_expect_success 'rv1-bootstrap2: killing nested jobs works' '
    flux cancel ${JOBID3} &&
    flux cancel ${JOBID4} &&
    flux job wait-event -t 20 ${JOBID3} release &&
    flux job wait-event -t 20 ${JOBID4} release
'

test_expect_success 'rv1-bootstrap2: creating doubly nested batch script' '
    cat >dnest.sh <<-EOF &&
#!/bin/sh
	flux module load sched-fluxion-resource \
load-allowlist=cluster,node,gpu,core match-format=rv1 policy=\$1
	flux module load sched-fluxion-qmanager
	hc=\$(expr \$4 / 2)
	hg=\$(expr \$5 / 2)
	job1=\$(flux batch -n\$2 -N\$3 -c\${hc} -g\${hg} \
		./nest.sh \$1 \$2 \$3 \${hc} \${hg} \$8)
	job2=\$(flux batch -n\$2 -N\$3 -c\${hc} -g\${hg} \
		./nest.sh \$1 \$2 \$3 \${hc} \${hg} \$9)
	\$WAITFILE -t 20 -v -p \"R_lite\" \$8
	flux job info \${job1} R > \$6
	\$WAITFILE -t 20 -v -p \"R_lite\" \$9
	flux job info \${job2} R > \$7
	sleep inf
EOF
    chmod u+x dnest.sh
'

test_expect_success 'rv1-bootstrap2: double nesting works' '
    JOBID5=$(flux batch -n2 -N2 -c10 -g2 ./dnest.sh low 2 2 10 2 \
	job5.1.json job5.2.json nest5.1.json nest5.2.json) &&
    $WAITFILE -t 20 -v -p \"R_lite\" job5.1.json &&
    $WAITFILE -t 20 -v -p \"R_lite\" job5.2.json &&
    remap_rv1_resource_type nest5.1.json core > nest5.1.csv &&
    remap_rv1_resource_type nest5.1.json gpu > nest5.1.gpu.csv &&
    remap_rv1_resource_type nest5.2.json core > nest5.2.csv &&
    remap_rv1_resource_type nest5.2.json gpu > nest5.2.gpu.csv &&
    remap_rv1_resource_type job5.1.json core > job5.1.csv &&
    remap_rv1_resource_type job5.1.json gpu > job5.1.gpu.csv &&
    remap_rv1_resource_type job5.2.json core 0 5 > job5.2.csv &&
    remap_rv1_resource_type job5.2.json gpu > job5.2.gpu.csv
'

# Cancel jobs
test_expect_success 'rv1-bootstrap2: killing doubly nested jobs works' '
    flux cancel ${JOBID5} &&
    flux job wait-event -t 20 ${JOBID5} release
'

test_expect_success 'rv1-bootstrap2: removing resource/qmanager modules' '
    remove_resource
'

test_done

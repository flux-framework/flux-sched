#!/bin/sh

test_description='Test a rabbit cluster with rv1_shorthand'

. $(dirname $0)/sharness.sh

cluster_jgf="${SHARNESS_TEST_SRCDIR}/data/resource/jgfs/rabbit.json"
rabbit_jobspec="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/advanced/rabbit.yaml"
rabbit_nonexcl_ssd_jobspec="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/advanced/rabbit-nonexclusive-ssd.yaml"
HOSTLIST="hetchy[1,201-202,1001-1018]"
SIZE="$(flux hostlist -c ${HOSTLIST})"

test_under_flux ${SIZE}

test_expect_success 'configure Flux' '
    flux config load <<-EOF
    [sched-fluxion-resource]
    match-format = "rv1_shorthand"

    [resource]
    noverify = true
    norestrict = true
    scheduling = "${cluster_jgf}"

    [[resource.config]]
    hosts = "${HOSTLIST}"
    cores = "0-1"
EOF
'

test_expect_success 'load resource' '
    flux module remove -f sched-simple &&
    flux module remove -f sched-fluxion-qmanager &&
    flux module remove -f sched-fluxion-resource &&
    flux module reload resource &&
    flux module load sched-fluxion-resource &&
    flux module load sched-fluxion-qmanager &&
    test_debug flux module list &&
    flux resource list &&
    FLUX_RESOURCE_LIST_RPC=sched.resource-status flux resource list

'

test_expect_success 'run a job' '
    jobid=$(flux submit -N1 hostname) &&
    flux job attach ${jobid} &&
    flux job info ${jobid} R | jq -e ".scheduling.writer == \"fluxion:jgf_shorthand\"" &&
    flux job info ${jobid} R | jq .scheduling.graph | test_must_fail grep core
'

test_expect_success 'run an alloc job' '
    jobid=$(flux alloc -N1 flux resource list -no "{ncores}") &&
    flux job info $(flux job last) R | jq -e ".scheduling.writer == \"fluxion:jgf_shorthand\"" &&
    flux job info $(flux job last) R | jq .scheduling.graph | test_must_fail grep core &&
    test $(flux job attach $(flux job last)) -eq 2
'

test_expect_success 'run a rabbit job' '
    # take $rabbit_jobspec YAML, convert it to JSON, and make it node-exclusive
    python3 -c "import sys, yaml, json; json.dump(yaml.safe_load(sys.stdin), sys.stdout, sort_keys=True, indent=4)" \
        < ${rabbit_jobspec} | jq ".resources[0].with[0].exclusive = true" > rabbit_jobspec.json &&
    jobid=$(flux job submit rabbit_jobspec.json) &&
    flux job attach ${jobid} &&
    flux job info ${jobid} R | jq -e ".scheduling.writer == \"fluxion:jgf_shorthand\"" &&
    # there should only be one core vertex in the JGF, under the rabbit (either hetchy201 or 202)
    # because the rabbit is not allocated exclusively
    flux job info ${jobid} R | jq -e ".scheduling.graph.nodes |
        [.[] | select (.metadata.type == \"core\")] | length" > corecount &&
    test "$(cat corecount)" -eq 1 &&
    flux job info ${jobid} R | jq -e ".scheduling.graph.nodes[].metadata |
        select(.type == \"core\") | .paths.containment" | grep -E "hetchy201|hetchy202"
'

test_expect_success 'run a rabbit job with non-exclusive SSD' '
    # convert YAML to JSON and submit
    python3 -c "import sys, yaml, json; json.dump(yaml.safe_load(sys.stdin), sys.stdout, sort_keys=True, indent=4)" \
        < ${rabbit_nonexcl_ssd_jobspec} | jq ".resources[0].with[0].exclusive = false" > rabbit_nonexcl_ssd.json &&
    jobid=$(flux job submit rabbit_nonexcl_ssd.json) &&
    flux job attach ${jobid} &&
    flux job info ${jobid} R | jq -e ".scheduling.writer == \"fluxion:jgf_shorthand\""
'

test_expect_success 'allocate two jobs with non-exclusive SSD sharing' '
    # submit two jobs that share SSDs on the same chassis
    jobid1=$(flux job submit rabbit_nonexcl_ssd.json) &&
    jobid2=$(flux job submit rabbit_nonexcl_ssd.json) &&
    flux job attach ${jobid1} &&
    flux job attach ${jobid2} &&
    # both jobs should have used the same chassis (because SSDs are pooled and non-exclusive)
    flux job info ${jobid1} R | jq -r ".scheduling.graph.nodes[] | select(.metadata.type == \"chassis\") | .metadata.paths.containment" > job1_chassis &&
    flux job info ${jobid2} R | jq -r ".scheduling.graph.nodes[] | select(.metadata.type == \"chassis\") | .metadata.paths.containment" > job2_chassis &&
    test_cmp job1_chassis job2_chassis
'

test_expect_success 'multiple non-exclusive SSD jobs: verify no over-allocation' '
    # Rabbit cluster has 4 SSDs total (2 per chassis), each 793 GiB = 3172 GiB total
    # Submit 5 jobs requesting 700 GiB each (5*700 = 3500 GiB > 3172 GiB available)
    # First 4 jobs should succeed, 5th should be pending/reserved
    jq ".resources[0].with[1].count = 700" rabbit_nonexcl_ssd.json > rabbit_large_ssd.json &&
    jq ".tasks[0].command = [\"sleep\", \"infinity\"]" rabbit_large_ssd.json > rabbit_large_ssd_sleep.json &&
    # Submit 5 jobs
    jobid1=$(flux job submit rabbit_large_ssd_sleep.json) &&
    jobid2=$(flux job submit rabbit_large_ssd_sleep.json) &&
    jobid3=$(flux job submit rabbit_large_ssd_sleep.json) &&
    jobid4=$(flux job submit rabbit_large_ssd_sleep.json) &&
    jobid5=$(flux job submit rabbit_large_ssd_sleep.json) &&
    # First 4 should allocate and start
    flux job wait-event -t 10 ${jobid1} start &&
    flux job wait-event -t 10 ${jobid2} start &&
    flux job wait-event -t 10 ${jobid3} start &&
    flux job wait-event -t 10 ${jobid4} start &&
    # 5th job should NOT allocate immediately (only 372 GiB remaining)
    test_must_fail flux job wait-event -t 2 ${jobid5} alloc 2>job5_wait.err &&
    # Verify job5 is pending, not allocated
    flux jobs -no {state} ${jobid5} > job5_state &&
    grep -q "PENDING\|SCHED" job5_state &&
    # Cancel all jobs
    flux cancel ${jobid1} ${jobid2} ${jobid3} ${jobid4} ${jobid5} &&
    flux job wait-event ${jobid1} clean &&
    flux job wait-event ${jobid2} clean &&
    flux job wait-event ${jobid3} clean &&
    flux job wait-event ${jobid4} clean &&
    flux job wait-event ${jobid5} clean
'

test_expect_success 'cleanup: cancel jobs from over-allocation test' '
    # Cancel any lingering jobs from test 8 to avoid contaminating later tests
    flux cancel --all 2>/dev/null
'

test_expect_success 'start two non-exclusive SSD jobs and leave them running' '
    # create jobspecs with sleep infinity so jobs run until explicitly canceled
    jq ".tasks[0].command = [\"sleep\", \"infinity\"]" rabbit_nonexcl_ssd.json > rabbit_nonexcl_ssd_sleep.json &&
    # submit two jobs that share non-exclusive SSDs
    jobid_nonexcl1=$(flux job submit rabbit_nonexcl_ssd_sleep.json) &&
    flux job wait-event ${jobid_nonexcl1} alloc &&
    flux job wait-event ${jobid_nonexcl1} start &&
    jobid_nonexcl2=$(flux job submit rabbit_nonexcl_ssd_sleep.json) &&
    flux job wait-event ${jobid_nonexcl2} alloc &&
    flux job wait-event ${jobid_nonexcl2} start
'

test_expect_success 'verify both non-exclusive SSD jobs are still running' '
    flux jobs -no {state} ${jobid_nonexcl1} > job1_state &&
    flux jobs -no {state} ${jobid_nonexcl2} > job2_state &&
    grep -q "RUN" job1_state &&
    grep -q "RUN" job2_state
'

test_expect_success 'agfilters should track non-exclusive SSD usage at cluster level' '
    flux ion-resource find -q --format=jgf agfilter=true \
        | jq . > agfilter_nonexcl_active.json &&
    # With two jobs each allocating count:1 SSD capacity (non-exclusive),
    # agfilter should show used:2 (capacity-based, not full size)
    jq -e ".graph.nodes[].metadata | select(.type == \"cluster\") | .agfilter.ssd
        | startswith(\"used:2\")" agfilter_nonexcl_active.json
'

test_expect_success 'reload scheduler with non-exclusive SSD jobs running' '
    flux module remove -f sched-fluxion-qmanager &&
    flux module remove -f sched-fluxion-resource &&
    flux module load sched-fluxion-resource &&
    flux module load sched-fluxion-qmanager
'

test_expect_success 'jobs with non-exclusive SSDs should survive scheduler reload' '
    # Verify that jobs with non-exclusive SSDs are successfully reconstructed
    # after scheduler reload and remain in RUN state
    flux jobs -no {state} ${jobid_nonexcl1} > job1_state_after_reload &&
    flux jobs -no {state} ${jobid_nonexcl2} > job2_state_after_reload &&
    grep -q "RUN" job1_state_after_reload &&
    grep -q "RUN" job2_state_after_reload
'

test_expect_success 'after reload, agfilters should track non-exclusive SSD usage' '
    flux ion-resource find -q --format=jgf agfilter=true \
        | jq . > agfilter_nonexcl_after_reload.json &&
    # After reload, cluster should still show used:2 for SSDs (capacity-based)
    jq -e ".graph.nodes[].metadata | select(.type == \"cluster\") | .agfilter.ssd
        | startswith(\"used:2\")" agfilter_nonexcl_after_reload.json
'

test_expect_success 'cancel non-exclusive SSD jobs' '
    # Cancel the jobs and wait for cleanup
    flux cancel ${jobid_nonexcl1} ${jobid_nonexcl2} &&
    flux job wait-event ${jobid_nonexcl1} clean &&
    flux job wait-event ${jobid_nonexcl2} clean
'

test_expect_success 'agfilters are correct: all have used:0' '
    flux ion-resource find -q --format=jgf agfilter=true \
        | jq . > agfilter_output.json &&
    jq -e ".graph.nodes[].metadata.agfilter.core | startswith(\"used:0\")" agfilter_output.json &&
    jq -e ".graph.nodes[].metadata | select(.type == \"cluster\") | .agfilter.ssd
        | startswith(\"used:0\")" agfilter_output.json &&
    jq -e ".graph.nodes[].metadata | select(.type == \"chassis\" or .type == \"cluster\")
        | .agfilter.node | startswith(\"used:0\")" agfilter_output.json
'

test_expect_success 'start a rabbit job and leave it running' '
    # take node-exclusive rabbit JSON jobspec and change it to execute `sleep 30`
    # so the job is still running when we reload the scheduler
    jq ".tasks[0].command = [\"sleep\", \"30\"]" rabbit_jobspec.json > rabbit_jobspec_sleep.json &&
    jobid=$(flux job submit rabbit_jobspec_sleep.json) &&
    flux job wait-event ${jobid} alloc
'

test_expect_success 'agfilters are correct for storage_node' '
    flux ion-resource find -q --format=jgf agfilter=true \
        | jq . > agfilter_active_jobs.json &&
    # one rabbit should show that one core is allocated
    rabbit_cores_used=$(jq ".graph.nodes[].metadata | select(.type == \"storage_node\") | .agfilter.core
        | startswith(\"used:1\")" agfilter_active_jobs.json | grep true | wc -l) &&
    test "${rabbit_cores_used}" -eq 1
'

test_expect_success 'agfilters are correct for cluster' '
    # one ssd should be allocated, size 793
    jq -e ".graph.nodes[].metadata | select(.type == \"cluster\") | .agfilter.ssd
        | startswith(\"used:793\")" agfilter_active_jobs.json &&
    jq -e ".graph.nodes[].metadata | select(.type == \"cluster\")
        | .agfilter.node | startswith(\"used:1\")" agfilter_active_jobs.json &&
    jq -e ".graph.nodes[].metadata | select(.type == \"cluster\")
        | .agfilter.core | startswith(\"used:3\")" agfilter_active_jobs.json
'

test_expect_success 'agfilters are correct for chassis' '
    # one of the two chassis should show one node and three cores used
    # the "storage_node" does not count as a node, otherwise it would be two nodes used
    chassis_nodes_used=$(jq ".graph.nodes[].metadata | select(.type == \"chassis\") | .agfilter.node
        | startswith(\"used:1\")" agfilter_active_jobs.json | grep true | wc -l) &&
    test "${chassis_nodes_used}" -eq 1 &&
    chassis_with_three_cores_used=$(jq ".graph.nodes[].metadata | select(.type == \"chassis\") | .agfilter.core
        | startswith(\"used:3\")" agfilter_active_jobs.json | grep true | wc -l) &&
    test "${chassis_with_three_cores_used}" -eq 1
'

test_expect_success 'agfilters are correct for node' '
    # one node should show that both of its two cores are allocated
    cores_used=$(jq ".graph.nodes[].metadata | select(.type == \"node\") | .agfilter.core
        | startswith(\"used:2\")" agfilter_active_jobs.json | grep true | wc -l) &&
    test "${cores_used}" -eq 1
'

test_expect_success 'reload the scheduler' '
    flux module remove -f sched-fluxion-qmanager &&
    flux module remove -f sched-fluxion-resource &&
    flux module load sched-fluxion-resource &&
    flux module load sched-fluxion-qmanager
'

test_expect_success 'after reload, agfilters are correct for storage_node' '
    flux ion-resource find -q --format=jgf agfilter=true \
        | jq . > agfilter_after_reload.json &&
    # one rabbit should show that one core is allocated
    rabbit_cores_used=$(jq ".graph.nodes[].metadata | select(.type == \"storage_node\") | .agfilter.core
        | startswith(\"used:1\")" agfilter_after_reload.json | grep true | wc -l) &&
    test "${rabbit_cores_used}" -eq 1
'

test_expect_success 'after reload, agfilters are correct for cluster' '
    # one ssd should be allocated, size 793
    jq -e ".graph.nodes[].metadata | select(.type == \"cluster\") | .agfilter.ssd
        | startswith(\"used:793\")" agfilter_after_reload.json &&
    jq -e ".graph.nodes[].metadata | select(.type == \"cluster\")
        | .agfilter.node | startswith(\"used:1\")" agfilter_after_reload.json &&
    jq -e ".graph.nodes[].metadata | select(.type == \"cluster\")
        | .agfilter.core | startswith(\"used:3\")" agfilter_after_reload.json
'

test_expect_success 'after reload, agfilters are correct for chassis' '
    # one of the two chassis should show one node and three cores used
    # the "storage_node" does not count as a node, otherwise it would be two nodes used
    chassis_nodes_used=$(jq ".graph.nodes[].metadata | select(.type == \"chassis\") | .agfilter.node
        | startswith(\"used:1\")" agfilter_after_reload.json | grep true | wc -l) &&
    test "${chassis_nodes_used}" -eq 1 &&
    chassis_with_three_cores_used=$(jq ".graph.nodes[].metadata | select(.type == \"chassis\") | .agfilter.core
        | startswith(\"used:3\")" agfilter_after_reload.json | grep true | wc -l) &&
    test "${chassis_with_three_cores_used}" -eq 1
'

test_expect_success 'after reload, agfilters are correct for node' '
    # one node should show that both of its two cores are allocated
    cores_used=$(jq ".graph.nodes[].metadata | select(.type == \"node\") | .agfilter.core
        | startswith(\"used:2\")" agfilter_after_reload.json | grep true | wc -l) &&
    test "${cores_used}" -eq 1
'

test_expect_success 'sched-now=allocated is correct' '
    flux ion-resource find -q --format=jgf sched-now=allocated \
        | jq . > sched_now_alloc.json &&
    # three cores should be allocated: two from a node, one from a storage_node
    jq -e ".graph.nodes | [.[] | select(.metadata.type == \"core\")] | length == 3" \
        sched_now_alloc.json &&
    jq -e ".graph.nodes | [.[] | select(.metadata.type == \"node\")] | length == 1" \
        sched_now_alloc.json &&
    jq -e ".graph.nodes | [.[] | select(.metadata.type == \"storage_node\")] | length == 1" \
        sched_now_alloc.json
'

test_expect_success 'cancel job' '
    flux cancel ${jobid} && flux job wait-event ${jobid} clean
'

test_expect_success 'agfilters are correct: all have used:0' '
    flux ion-resource find -q --format=jgf agfilter=true \
        | jq . > agfilter_jobs_complete.json &&
    jq -e ".graph.nodes[].metadata.agfilter.core | startswith(\"used:0\")" agfilter_jobs_complete.json &&
    jq -e ".graph.nodes[].metadata | select(.type == \"cluster\") | .agfilter.ssd
        | startswith(\"used:0\")" agfilter_jobs_complete.json &&
    jq -e ".graph.nodes[].metadata | select(.type == \"chassis\" or .type == \"cluster\")
        | .agfilter.node | startswith(\"used:0\")" agfilter_jobs_complete.json
'

test_expect_success 'sched-now=allocated is null' '
    flux ion-resource find -q --format=jgf sched-now=allocated &&
    flux ion-resource find -q --format=jgf sched-now=allocated | jq -e ". == null"
'

test_expect_success 'remove manually loaded modules' '
    remove_qmanager && remove_resource
'

test_done

#!/bin/sh

test_description='Test a rabbit cluster with rv1_shorthand'

. $(dirname $0)/sharness.sh

cluster_jgf="${SHARNESS_TEST_SRCDIR}/data/resource/jgfs/rabbit.json"
rabbit_jobspec="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/advanced/rabbit.yaml"
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
    echo "${rabbit_cores_used}" == 1 &&
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
    echo "${chassis_nodes_used}" == 1 &&
    test "${chassis_nodes_used}" -eq 1 &&
    chassis_with_three_cores_used=$(jq ".graph.nodes[].metadata | select(.type == \"chassis\") | .agfilter.core
        | startswith(\"used:3\")" agfilter_active_jobs.json | grep true | wc -l) &&
    echo "${chassis_with_three_cores_used}" == 1 &&
    test "${chassis_with_three_cores_used}" -eq 1
'

test_expect_success 'agfilters are correct for node' '
    # one node should show that both of its two cores are allocated
    cores_used=$(jq ".graph.nodes[].metadata | select(.type == \"node\") | .agfilter.core
        | startswith(\"used:2\")" agfilter_active_jobs.json | grep true | wc -l) &&
    echo "${cores_used}" == 1 &&
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
    echo "${rabbit_cores_used}" == 1 &&
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
    echo "${chassis_nodes_used}" == 1 &&
    test "${chassis_nodes_used}" -eq 1 &&
    chassis_with_three_cores_used=$(jq ".graph.nodes[].metadata | select(.type == \"chassis\") | .agfilter.core
        | startswith(\"used:3\")" agfilter_after_reload.json | grep true | wc -l) &&
    echo "${chassis_with_three_cores_used}" == 1 &&
    test "${chassis_with_three_cores_used}" -eq 1
'

test_expect_success 'after reload, agfilters are correct for node' '
    # one node should show that both of its two cores are allocated
    cores_used=$(jq ".graph.nodes[].metadata | select(.type == \"node\") | .agfilter.core
        | startswith(\"used:2\")" agfilter_after_reload.json | grep true | wc -l) &&
    echo "${cores_used}" == 1 &&
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

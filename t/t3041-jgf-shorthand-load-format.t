#!/bin/sh

test_description='Test the jgf_shorthand load format'

. $(dirname $0)/sharness.sh

query="../../resource/utilities/resource-query"
jobspec="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/load_format/t3041.yaml"
jobspec1="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/load_format/t3041-1node.yaml"

test_expect_success 'Generate rv1 with .scheduling key' '
    flux R encode --hosts=compute[1-20] --cores=0-15 --gpu=0-3 | jq . > R.json &&
    cat R.json | flux ion-R encode > R_JGF.json &&
    jq -S "del(.execution.starttime, .execution.expiration)" R.json > R.norm.json
'

test_expect_success 'resource_query can be loaded with jgf_shorthand reader' '
    cat > match_jobspec_cmd <<-EOF &&
    match allocate ${jobspec}
    quit
EOF
    jq -S .scheduling R_JGF.json > JGF.json &&
    ${query} -L JGF.json -f jgf_shorthand -F rv1_nosched -t R1.out -P high < match_jobspec_cmd &&
    head -n1 R1.out | jq -S "del(.execution.starttime, .execution.expiration)" > r_match.json &&
    test_cmp r_match.json R.norm.json
'

test_expect_success 'generate jgf_shorthand to use to update regular JGF' '
    ${query} -L JGF.json -f jgf_shorthand -F jgf_shorthand -t jgf1.out -P lonodex < match_jobspec_cmd &&
    head -n1 jgf1.out | jq -S . > shorthand.json &&
    test_must_fail grep core shorthand.json > /dev/null &&
    test_must_fail grep gpu shorthand.json > /dev/null &&
    grep node shorthand.json > /dev/null &&
    grep compute1 shorthand.json > /dev/null
'

test_expect_success 'update graph to allocate a job using jgf_shorthand' '
    cat > update_allocate_cmd <<-EOF &&
    update allocate jgf_shorthand shorthand.json 0 0 5
    f sched-now=allocated
    quit
EOF
    jq -S .scheduling R_JGF.json > JGF.json &&
    ${query} -L JGF.json -f jgf_shorthand -F jgf -t jgf2.out -P high < update_allocate_cmd &&
    tail -n4 jgf2.out | head -n1 > allocated.json
'

test_expect_success 'JGF output shows all cores and gpus allocated' '
    grep core allocated.json > /dev/null &&
    grep gpu allocated.json > /dev/null &&
    grep node allocated.json > /dev/null &&
    grep compute1 allocated.json > /dev/null &&
    jq -e ".graph.nodes[] | select(.metadata.type == \"core\") | .id" allocated.json > cores.json &&
    test 320 -eq $(cat cores.json | wc -l) &&
    jq -e ".graph.nodes[] | select(.metadata.type == \"gpu\") | .id" allocated.json > gpus.json &&
    test 80 -eq $(cat gpus.json | wc -l)
'

#
# Reload of a partially freed allocation. The free_ranks key marks ranks
# 0-4 as released by a previous partial free, so reconstructing the job
# must restore only the 15 still-held nodes. See issue #1558.
#
test_expect_success 'update graph with free_ranks using jgf_shorthand' '
    jq ". + {free_ranks: \"0-4\"}" shorthand.json > shorthand_free.json &&
    cat > update_free_cmd <<-EOF &&
    update allocate jgf_shorthand shorthand_free.json 0 0 5
    find jobid-alloc=0
    quit
EOF
    jq -S .scheduling R_JGF.json > JGF.json &&
    ${query} -L JGF.json -f jgf_shorthand -F jgf -t jgf3.out -P high < update_free_cmd &&
    head -n1 jgf3.out > sh_allocated.json &&
    sed -n "7p" jgf3.out > sh_found.json
'

test_expect_success 'jgf_shorthand free_ranks reload holds only 15 nodes' '
    jq -e ".graph.nodes[] | select(.metadata.type == \"core\") | .id" sh_found.json > sh_cores.json &&
    test 240 -eq $(cat sh_cores.json | wc -l) &&
    jq -e ".graph.nodes[] | select(.metadata.type == \"gpu\") | .id" sh_found.json > sh_gpus.json &&
    test 60 -eq $(cat sh_gpus.json | wc -l)
'

test_expect_success 'freed ranks are available to a new job' '
    cat > match_1node_cmd <<-EOF &&
    update allocate jgf_shorthand shorthand_free.json 0 0 5
    match allocate ${jobspec1}
    quit
EOF
    jq -S .scheduling R_JGF.json > JGF.json &&
    ${query} -L JGF.json -f jgf_shorthand -F rv1_nosched -t jgf4.out -P high < match_1node_cmd &&
    grep "JOBID=1" jgf4.out > /dev/null &&
    grep -v INFO jgf4.out | tail -n1 > new_job.json &&
    jq -e ".execution.R_lite[].rank | tonumber < 5" new_job.json > /dev/null
'

#
# The skip of released ranks lives in the base JGF reader, so cover the
# complete-JGF path as well.
#
test_expect_success 'update graph with free_ranks using jgf' '
    ${query} -L JGF.json -f jgf_shorthand -F jgf -t full1.out -P lonodex < match_jobspec_cmd &&
    head -n1 full1.out | jq -S . > full.json &&
    jq ". + {free_ranks: \"0-4\"}" full.json > full_free.json &&
    cat > update_full_cmd <<-EOF &&
    update allocate jgf full_free.json 0 0 5
    find jobid-alloc=0
    quit
EOF
    jq -S .scheduling R_JGF.json > JGF.json &&
    ${query} -L JGF.json -f jgf_shorthand -F jgf -t jgf5.out -P high < update_full_cmd &&
    sed -n "7p" jgf5.out > full_found.json &&
    jq -e ".graph.nodes[] | select(.metadata.type == \"core\") | .id" full_found.json > full_cores.json &&
    test 240 -eq $(cat full_cores.json | wc -l) &&
    jq -e ".graph.nodes[] | select(.metadata.type == \"gpu\") | .id" full_found.json > full_gpus.json &&
    test 60 -eq $(cat full_gpus.json | wc -l)
'

test_done

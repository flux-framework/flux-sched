#!/bin/sh

test_description='Test the jgf_shorthand load format'

. $(dirname $0)/sharness.sh

query="../../resource/utilities/resource-query"

echo \
"version: 9999
resources:
  - type: node
    count: 10
    with:
      - type: slot
        count: 1
        label: default
        with:
          - type: core
            count: 8
          - type: gpu
            count: 2
attributes:
  system:
    duration: 3600
tasks:
  - command: [ \"app\" ]
    slot: default
    count:
      per_slot: 1
" > jobspec.yaml

test_expect_success 'Generate rv1 with .scheduling key' '
    flux R encode --hosts=compute[1-10] --cores=0-7 --gpu=0-1 | jq . > R.json &&
    cat R.json | flux ion-R encode > R_JGF.json &&
    jq -S "del(.execution.starttime, .execution.expiration)" R.json > R.norm.json
'

test_expect_success 'resource_query can be loaded with jgf_shorthand reader' '
    cat > match_jobspec_cmd <<-EOF &&
    match allocate jobspec.yaml
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
    test 80 -eq $(cat cores.json | wc -l) &&
    jq -e ".graph.nodes[] | select(.metadata.type == \"gpu\") | .id" allocated.json > gpus.json &&
    test 20 -eq $(cat gpus.json | wc -l)
'

test_done

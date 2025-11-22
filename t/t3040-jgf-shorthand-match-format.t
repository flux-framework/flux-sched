#!/bin/sh

test_description='Test the jgf_shorthand match format'

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

echo \
"version: 9999
resources:
  - type: node
    exclusive: true
    count: 1
attributes:
  system:
    duration: 3600
tasks:
  - command: [ \"app\" ]
    slot: default
    count:
      per_slot: 1
" > jobspec_only_node.yaml


test_expect_success 'Generate rv1 with .scheduling key' '
    flux R encode --hosts=compute[1-10] --cores=0-7 --gpu=0-1 | jq . > R.json &&
    cat R.json | flux ion-R encode > R_JGF.json &&
    jq -S "del(.execution.starttime, .execution.expiration)" R.json > R.norm.json
'

test_expect_success 'resource_query can be loaded with jgf' '
    cat > match_jobspec_cmd <<-EOF &&
    match allocate jobspec.yaml
    quit
EOF
    jq -S .scheduling R_JGF.json > JGF.json &&
    ${query} -L JGF.json -f jgf -F rv1_nosched -t R1.out -P high < match_jobspec_cmd &&
    head -n1 R1.out | jq -S "del(.execution.starttime, .execution.expiration)" > r_match.json &&
    test_cmp r_match.json R.norm.json
'

test_expect_success 'jgf_shorthand does not emit cores or gpus with policy=lonodex' '
    ${query} -L JGF.json -f jgf -F jgf_shorthand -t jgf1.out -P lonodex < match_jobspec_cmd &&
    head -n1 jgf1.out | jq -S . > match1.json &&
    test_must_fail grep core match1.json > /dev/null &&
    test_must_fail grep gpu match1.json > /dev/null &&
    grep node match1.json > /dev/null &&
    grep compute1 match1.json > /dev/null &&
    grep cluster match1.json > /dev/null &&
    jq -e ".graph.nodes[] | select(.id == \"23\") | .metadata.name == \"compute3\"" match1.json
'

test_expect_success 'emitted jgf_shorthand resource graph can be loaded but has no cores' '
    ${query} -L match1.json -f jgf -F jgf -t shorthand1.out -P low < match_jobspec_cmd &&
    grep "No matching resources found" shorthand1.out &&
    cat > match_only_nodes_cmd <<-EOF &&
    match allocate jobspec_only_node.yaml
    quit
EOF
    ${query} -L match1.json -f jgf -F jgf -t shorthand1-2.out -P low < match_only_nodes_cmd &&
    test_must_fail grep "No matching resources found" shorthand1-2.out &&
    grep cluster shorthand1-2.out && grep node shorthand1-2.out
'

test_expect_success 'jgf_shorthand emits cores and gpus with policy=low' '
    ${query} -L JGF.json -f jgf -F jgf_shorthand -t jgf2.out -P low < match_jobspec_cmd &&
    head -n1 jgf2.out | jq -S . > match2.json &&
    grep core match2.json > /dev/null &&
    grep gpu match2.json > /dev/null &&
    grep node match2.json > /dev/null &&
    grep compute1 match2.json > /dev/null &&
    grep cluster match2.json > /dev/null &&
    jq -e ".graph.nodes[] | select(.id == \"23\") | .metadata.name == \"compute3\"" match2.json
'

test_expect_success 'emitted jgf_shorthand resource graph can be loaded' '
    ${query} -L match2.json -f jgf -F jgf -t shorthand2.out -P first < match_jobspec_cmd &&
    test_must_fail grep "No matching resources found" shorthand2.out &&
    head -n1 shorthand2.out | jq -S . > shorthand2.json &&
    grep core shorthand2.json > /dev/null &&
    grep gpu shorthand2.json > /dev/null &&
    grep node shorthand2.json > /dev/null &&
    grep compute1 shorthand2.json > /dev/null &&
    grep cluster shorthand2.json > /dev/null &&
    jq -e ".graph.nodes[] | select(.id == \"23\") | .metadata.name == \"compute3\"" shorthand2.json
'

test_expect_success 'jgf_shorthand does not emit cores or gpus with exclusive nodes' '
    sed "4i\    exclusive: true" jobspec.yaml > jobspec_exclusive.yaml &&
    cat > match_jobspec_exclusive_cmd <<-EOF &&
    match allocate jobspec_exclusive.yaml
    quit
EOF
    ${query} -L JGF.json -f jgf -F jgf_shorthand -t jgf3.out -P high < match_jobspec_exclusive_cmd &&
    head -n1 jgf3.out | jq -S . > match3.json &&
    test_must_fail grep core match3.json > /dev/null &&
    test_must_fail grep gpu match3.json > /dev/null &&
    grep node match3.json > /dev/null &&
    grep compute1 match3.json > /dev/null &&
    grep cluster match3.json > /dev/null &&
    jq -e ".graph.nodes[] | select(.id == \"23\") | .metadata.name == \"compute3\"" match3.json
'

test_expect_success 'emitted jgf_shorthand resource graph can be loaded but has no cores: #2' '
    ${query} -L match3.json -f jgf -F jgf -t shorthand3.out -P low < match_jobspec_cmd &&
    grep "No matching resources found" shorthand3.out &&
    ${query} -L match3.json -f jgf -F jgf -t shorthand3-2.out -P low < match_only_nodes_cmd &&
    test_must_fail grep "No matching resources found" shorthand3-2.out &&
    grep cluster shorthand3-2.out && grep node shorthand3-2.out
'

test_done

#!/bin/sh

test_description='Test the rv1exec_force load format'

. $(dirname $0)/sharness.sh

query="../../resource/utilities/resource-query"
jobspec="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/load_format/t3039.yaml"


test_expect_success 'Generate rv1 with .scheduling key' '
    flux R encode --hosts=compute[1-10] --cores=0-7 --gpu=0-1 | jq . > R.json &&
    cat R.json | flux ion-R encode > R_JGF.json &&
    jq -S "del(.execution.starttime, .execution.expiration)" R.json > R.norm.json
'

test_expect_success 'resource_query can be loaded with rv1exec_force' '
    cat > match_jobspec_cmd <<-EOF &&
    match allocate ${jobspec}
    quit
EOF
    ${query} -L R_JGF.json -f rv1exec_force -F rv1_nosched -t R1.out -P high < match_jobspec_cmd &&
    head -n1 R1.out | jq -S "del(.execution.starttime, .execution.expiration)" > match1.json &&
    jq -S "del(.execution.starttime, .execution.expiration)" R.json > R.norm.json &&
    test_cmp match1.json R.norm.json
'

test_expect_success 'resource_query can be loaded with rv1exec' '
    ${query} -L R_JGF.json -f rv1exec -F rv1_nosched -t R2.out -P high < match_jobspec_cmd &&
    head -n1 R2.out | jq -S "del(.execution.starttime, .execution.expiration)" > match2.json &&
    test_cmp match2.json R.norm.json
'

test_expect_success 'resource_query can be loaded with jgf' '
    jq .scheduling R_JGF.json > JGF.json &&
    ${query} -L JGF.json -f jgf -F rv1_nosched -t R3.out -P high < match_jobspec_cmd &&
    head -n1 R3.out | jq -S "del(.execution.starttime, .execution.expiration)" > match3.json &&
    test_cmp match3.json R.norm.json
'

test_done

#!/bin/sh

test_description='Test the rv1exec_force load format'

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
# a comment
attributes:
  system:
    duration: 3600
tasks:
  - command: [ \"app\" ]
    slot: default
    count:
      per_slot: 1
" > jobspec.json

echo \
'#!/bin/sh

flux module remove -f sched-simple &&
flux module remove -f sched-fluxion-qmanager &&
flux module remove -f sched-fluxion-resource &&
flux kvs get resource.R | jq ".scheduling = \"nonsense\"" > local_R &&
flux kvs put resource.R="$(cat local_R)" &&
flux module reload resource &&
flux module load sched-fluxion-resource load-format="${1}" &&
flux module load sched-fluxion-qmanager
' > load_nonsense_scheduling_key.sh

chmod +x load_nonsense_scheduling_key.sh


test_under_flux 2


test_expect_success 'Generate rv1 with .scheduling key' '
    flux R encode --hosts=compute[1-10] --cores=0-7 --gpu=0-1 | jq . > R.json &&
    cat R.json | flux ion-R encode > R_JGF.json &&
    jq -S "del(.execution.starttime, .execution.expiration)" R.json > R.norm.json
'

test_expect_success 'resource_query can be loaded with rv1exec_force' '
    cat > match_jobspec_cmd <<-EOF &&
    match allocate jobspec.json
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

test_expect_success 'load resource with load-format-rv1exec_force' '
    flux module remove -f sched-simple &&
    reload_resource load-format=rv1exec_force &&
    reload_qmanager
'

test_expect_success 'init Fluxion with nonsense in R .scheduling key' '
    ./load_nonsense_scheduling_key.sh rv1exec_force
'

test_expect_success 'job can run' '
    jobid=$(flux submit -n1 true) &&
    flux job wait-event -vt 5 ${jobid} clean &&
    flux module list | grep sched-fluxion-resource
'

test_expect_success 'without rv1exec_force, job fails' '
    test_must_fail flux alloc -t5s -N1 -n1 ./load_nonsense_scheduling_key.sh rv1exec &&
    flux job attach $(flux job last) 2>&1 | grep -e grow_resource_db_jgf -e "Invalid argument" &&
    flux job attach $(flux job last) 2>&1 | grep "module exiting abnormally"
'

test_expect_success 'remove manually loaded modules' '
    remove_qmanager && remove_resource
'

test_done

#!/bin/sh

test_description='Test the rv1exec_force load format'

. $(dirname $0)/sharness.sh

echo \
'#!/bin/sh

set -e

flux module remove -f sched-simple &&
flux module remove -f sched-fluxion-qmanager &&
flux module remove -f sched-fluxion-resource &&
flux kvs get resource.R | jq ".scheduling = \"nonsense\"" > local_R &&
flux kvs put resource.R="$(cat local_R)" &&
flux module reload resource &&
flux module load sched-fluxion-resource load-format="${1}" &&
flux module load sched-fluxion-qmanager
' > load_nonsense_scheduling_key.sh

echo \
'#!/bin/sh

set -e

flux module remove -f sched-simple &&
flux module remove -f sched-fluxion-qmanager &&
flux module remove -f sched-fluxion-resource &&
flux module reload resource &&
flux module load sched-fluxion-resource &&
flux module load sched-fluxion-qmanager &&
flux module list
$@
' > load_fluxion.sh

chmod +x load_nonsense_scheduling_key.sh load_fluxion.sh

test_under_flux 2

test_expect_success 'load resource with load-format=rv1exec_force' '
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

test_expect_success 'load resource with match-format=rv1_shorthand' '
    reload_resource load-format=rv1exec_force match-format=rv1_shorthand &&
    reload_qmanager
'

test_expect_success 'jobs run with shorthand jgf' '
    jobid=$(flux submit -n1 true) &&
    flux job wait-event -vt 5 ${jobid} alloc &&
    flux job info ${jobid} R | jq -e ".scheduling.writer == null" &&
    flux job attach ${jobid} &&
    jobid=$(flux submit -N1 true) &&
    flux job wait-event -vt 5 ${jobid} alloc &&
    flux job info ${jobid} R | jq -e ".scheduling.writer == \"fluxion:jgf_shorthand\"" &&
    flux job attach ${jobid}
'

test_expect_success 'flux instances started with shorthand jgf can run jobs' '
    flux alloc -t5s -N1 ./load_fluxion.sh flux run -n1 -c1 echo hello &&
    flux job attach $(flux job last) &&
    flux job info $(flux job last) R | jq -e ".scheduling.writer == \"fluxion:jgf_shorthand\"" &&
    flux job info $(flux job last) R | jq .scheduling.graph | test_must_fail grep core
'

test_expect_success 'scheduler can be reloaded with jgf_shorthand jobs' '
    jobid=$(flux submit -t15s -N1 flux start ./load_fluxion.sh sleep 10) &&
    flux job wait-event ${jobid} alloc &&
    reload_resource load-format=rv1exec_force match-format=rv1_shorthand &&
    reload_qmanager &&
    flux cancel ${jobid} &&
    flux job wait-event ${jobid} clean &&
    flux job info ${jobid} R | jq -e ".scheduling.writer == \"fluxion:jgf_shorthand\"" &&
    flux job info ${jobid} R | jq .scheduling.graph | test_must_fail grep core
'

test_expect_success 'agfilters are correct' '
    flux ion-resource find -q --format=jgf agfilter=true \
        | jq . > agfilter_output.json &&
    jq -e ".graph.nodes[].metadata.agfilter.core | startswith(\"used:0\")" agfilter_output.json &&
    jq -e ".graph.nodes[].metadata | select(.type == \"cluster\") | .agfilter.node
        | startswith(\"used:0\")" agfilter_output.json
'

test_expect_success 'jobid span/tag/alloc output shows no vertices' '
    test $(flux ion-resource find -q --format=jgf jobid-span=${jobid}) = null &&
    test $(flux ion-resource find -q --format=jgf jobid-tag=${jobid}) = null &&
    test $(flux ion-resource find -q --format=jgf jobid-alloc=${jobid}) = null &&
    test $(flux ion-resource find -q --format=jgf sched-now=allocated) = null
'

test_expect_success 'remove manually loaded modules' '
    remove_qmanager && remove_resource
'

test_done

#!/bin/sh

test_description='Test the rv1exec_force load format'

. $(dirname $0)/sharness.sh

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

#!/bin/sh
#set -x

# Adapted from t4005

test_description='Test the basic functionality of match satisfiability'

. `dirname $0`/sharness.sh

conf_base=${SHARNESS_TEST_SRCDIR}/conf.d
notify_base=`readlink -e ${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/shrink`
grug="${SHARNESS_TEST_SRCDIR}/data/resource/grugs/tiny.graphml"
jobspec1="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/basics/test001.yaml"
rabbit_jgf="${SHARNESS_TEST_SRCDIR}/data/resource/jgfs/rabbit.json"
rabbit_jobspec="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/advanced/rabbit.yaml"

export FLUX_URI_RESOLVE_LOCAL=t
export FLUX_SCHED_MODULE=sched-simple

test_under_flux 1 full -Sbroker.module-nopanic=1

test_debug '
    echo ${grug} &&
    echo ${jobspec1}
'

test_expect_success 'loading feasibility module over sched-simple fails' '
    load_feasibility 2>&1 | grep -q "File exists"
'

test_expect_success 'removing sched-simple works' '
    flux module remove sched-simple &&
    flux dmesg -c | grep -q "rmmod sched-simple"
'

test_expect_success 'loading feasibility module before resource fails' '
    load_feasibility &&
    flux dmesg -c | grep -q "Function not implemented"
'

test_expect_success 'loading resource module with a tiny machine config works' '
    load_resource load-file=${grug} load-format=grug \
prune-filters=ALL:core subsystems=containment policy=high &&
    test -z "$(flux dmesg -c | grep -q err)"
'

test_expect_success 'loading feasibility module with a tiny machine config works' '
    load_feasibility load-file=${grug} load-format=grug \
subsystems=containment policy=high &&
    test -z "$(flux dmesg -c | grep -q err)"
'

test_expect_success 'satisfiability works with a 1-node, 1-socket jobspec' '
    flux ion-resource match allocate_with_satisfiability ${jobspec1} &&
    flux ion-resource match allocate_with_satisfiability ${jobspec1} &&
    flux ion-resource match allocate_with_satisfiability ${jobspec1} &&
    flux ion-resource match allocate_with_satisfiability ${jobspec1}
'

test_expect_success 'satisfiability returns EBUSY when no available resources' '
    test_expect_code 16 flux ion-resource \
match allocate_with_satisfiability ${jobspec1} &&
    test_expect_code 16 flux ion-resource \
match allocate_with_satisfiability ${jobspec1} &&
    test_expect_code 16 flux ion-resource \
match allocate_with_satisfiability ${jobspec1} &&
    test_expect_code 16 flux ion-resource \
match allocate_with_satisfiability ${jobspec1}
'

test_expect_success 'jobspec is still satisfiable even when no available resources' '
    flux ion-resource match satisfiability ${jobspec1} &&
    flux ion-resource match satisfiability ${jobspec1} &&
    flux ion-resource match satisfiability ${jobspec1} &&
    flux ion-resource match satisfiability ${jobspec1}
'

test_expect_success 'removing load-file feasibility module works' '
    remove_feasibility &&
    test -z "$(flux dmesg -c | grep -q err)"
'

# A resource module that has a load-file will not relay those resources to
# the feasibility module. The feasibility module needs the same load-file.
test_expect_success 'loading feasibility module from load-file resource module fails' '
    load_feasibility &&
    flux dmesg -c | grep -q err &&
    ! flux module list | grep -q sched-fluxion-feasib
'

test_expect_success 'removing resource module works' '
    remove_resource
'

test_expect_success 'loading non-load-file resource module works' '
    load_resource &&
    test -z "$(flux dmesg -c | grep -q err)"
'

test_expect_success 'loading feasibility from non-load-file resource module works' '
    load_feasibility &&
    test -z "$(flux dmesg -c | grep -q err)"
'

test_expect_success 'loading job-validator conf works' '
{ cat >conf.tmp << 'EOF'
[ingest.validator]
plugins = ["jobspec", "feasibility"]
EOF
} &&
    flux config load conf.tmp && rm conf.tmp &&
    flux config get | grep feasibility &&
    flux job-validator --list-plugins | grep feasibility
'

test_expect_success 'job-validator correctly returns unsatisfiable' '
    flux submit -N99999 --dry-run sleep inf |\
    flux job-validator --plugins=feasibility --jobspec-only |\
    grep "{\"errnum\": 19, \"errstr\": \"Unsatisfiable request\"}"
'

test_expect_success 'job-validator correctly returns satisfiable' '
    flux run -n1 --dry-run sleep inf |\
    flux job-validator --plugins=feasibility --jobspec-only |\
    grep "{\"errnum\": 0}"
'

test_expect_success 'removing resource works and removes feasibility' '
    remove_resource &&
    flux dmesg -c | grep -q "exiting due to sched-fluxion-resource.notify failure"
'

test_expect_success 'job-validator returns satisfiable without the feas module' '
    flux submit -N99999 --dry-run sleep inf |\
    flux job-validator --plugins=feasibility --jobspec-only |\
    grep "{\"errnum\": 0}"
'

export FLUX_SCHED_MODULE=none

test_expect_success 'loading feasibility with its own config works' '
    flux broker --config-path=${conf_base}/01-default bash -c \
"flux module reload -f sched-fluxion-resource && "\
"flux module reload -f sched-fluxion-feasibility && "\
"flux module reload -f sched-fluxion-qmanager"
'

# Tests for storage_node feasibility

test_expect_success 'removing resource and feasibility modules' '
    flux module remove -f sched-fluxion-qmanager &&
    flux module remove -f sched-fluxion-feasibility &&
    flux module remove -f sched-fluxion-resource
'

test_expect_success 'loading resource with rabbit JGF (includes storage_node)' '
    load_resource load-file=${rabbit_jgf} load-format=jgf \
prune-filters=ALL:core policy=high &&
    test -z "$(flux dmesg -c | grep -q err)"
'

test_expect_success 'loading feasibility with rabbit JGF' '
    load_feasibility load-file=${rabbit_jgf} load-format=jgf policy=high &&
    test -z "$(flux dmesg -c | grep -q err)"
'

test_expect_success 'feasibility check succeeds for core request on storage_node' '
    # in the graph, hetchy201 is a rabbit / storage_node
    flux run -n1 --requires=hosts:hetchy201 --dry-run hostname | \
        flux job-validator --plugins=feasibility --jobspec-only | \
        grep "{\"errnum\": 0}"
'

test_expect_success 'feasibility check rejects too many cores' '
    flux run -n999 --dry-run hostname | \
        flux job-validator --plugins=feasibility --jobspec-only | \
        grep "{\"errnum\": 19, \"errstr\": \"Unsatisfiable request\"}"
'

test_expect_success 'feasibility accepts storage_node with correct property constraint' '
    # hetchy201 has the rabbit property
    flux run -n1 --requires="hosts:hetchy201 and rabbit" --dry-run hostname | \
        flux job-validator --plugins=feasibility --jobspec-only | \
        grep "{\"errnum\": 0}"
'

test_expect_success 'feasibility rejects storage_node with incorrect property constraint' '
    # hetchy201 does not have the parrypeak property, but other nodes do
    flux run -n1 --requires="hosts:hetchy201 and parrypeak" --dry-run hostname | \
        flux job-validator --plugins=feasibility --jobspec-only | \
        grep "{\"errnum\": 19, \"errstr\": \"Unsatisfiable request\"}"
'

test_expect_success 'satisfiability check accepts storage_node with core request and or-constraint' '
    flux run -n1 --requires="hosts:hetchy201 and (rabbit or parrypeak)" \
        --dry-run hostname > rabbit_or_parrypeak.json &&
    flux ion-resource match satisfiability rabbit_or_parrypeak.json
'

test_expect_success 'satisfiability check accepts explicit storage_node with or-constraint' '
    cat ${rabbit_jobspec} | flux python ${SHARNESS_TEST_SRCDIR}/scripts/y2j.py \
        | jq ".attributes.system.constraints = {\"or\": [{\"properties\":
            [\"rabbit\"]}, {\"properties\": [\"parrypeak\"]}]}" > rabbit_with_constraints.json &&
    flux ion-resource match satisfiability rabbit_with_constraints.json
'

test_expect_success 'feasibility check accepts explicit storage_node with or-constraint' '
    # for some reason the validator requires compact JSON
    jq -c . rabbit_with_constraints.json | flux job-validator --plugins=feasibility --jobspec-only | \
        grep "{\"errnum\": 0}"
'

test_expect_success 'removing rabbit modules' '
    remove_feasibility &&
    remove_resource
'

test_done

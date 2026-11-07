#!/bin/sh

test_description='Test matched slot label emission through the resource module

Ensure that the flexible traverser, running inside sched-fluxion-resource,
tags each vertex it allocates to a matched slot with that slot label and
emits it in R under scheduling.graph.nodes[].metadata.ephemeral.slot_label
(match-format=rv1). This is the module-level companion to t3043 (which drives
resource-query directly) and t3044 (which submits full jobs).
'

. `dirname $0`/sharness.sh

grug="${SHARNESS_TEST_SRCDIR}/data/resource/grugs/tiny.graphml"
jobspec_dir="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/flexible"

#
# test_under_flux is under sharness.d/
#
test_under_flux 1

if command -v jq >/dev/null 2>&1; then
    test_set_prereq HAVE_JQ
fi

# `flux ion-resource match allocate` prints a header followed by the R object;
# the R is the single line beginning with '{'. Extract it for jq.
extract_R() {
    grep '^{'
}

# The sorted, unique, non-empty set of slot labels present in an R object read
# on stdin.
labels_of_R() {
    jq -r '[.scheduling.graph.nodes[].metadata.ephemeral.slot_label // empty]
           | map(select(. != ""))
           | unique
           | .[]'
}

test_expect_success 'load resource module with flexible traverser and rv1' '
    load_resource \
        load-file=${grug} load-format=grug prune-filters=ALL:core \
        subsystems=containment policy=high traverser=flexible \
        match-format=rv1
'

test_expect_success HAVE_JQ 'xor-slot: matched branch label appears in R' '
    flux ion-resource match allocate ${jobspec_dir}/test010.yaml \
        | extract_R > xor.R &&
    labels_of_R < xor.R > xor.labels &&
    test $(wc -l < xor.labels) -eq 1 &&
    grep -qx "small" xor.labels
'

test_expect_success HAVE_JQ 'nested xor-slot: inner and outer labels appear' '
    flux ion-resource match allocate ${jobspec_dir}/test012.yaml \
        | extract_R > nested.R &&
    labels_of_R < nested.R > nested.labels &&
    grep -qx "a1" nested.labels &&
    grep -qx "default" nested.labels
'

test_expect_success HAVE_JQ 'label lands on the gpu leaf of the matched branch' '
    flux ion-resource match allocate ${jobspec_dir}/test010.yaml \
        | extract_R > gpu.R &&
    jq -r ".scheduling.graph.nodes[]
           | select(.metadata.type == \"gpu\")
           | .metadata.ephemeral.slot_label // empty" gpu.R \
        | sort -u > gpu.label &&
    grep -qx "small" gpu.label
'

test_expect_success 'generate an unlabeled flexible jobspec' '
    cat >unlabeled.yaml <<-EOF
	version: 9999
	resources:
	  - type: slot
	    count: 1
	    label: ""
	    with:
	      - type: core
	        count: 4
	attributes:
	  system:
	    duration: 3600
	tasks:
	  - command: [ "app" ]
	    slot: ""
	    count:
	      per_slot: 1
	EOF
'

test_expect_success HAVE_JQ 'unlabeled slot emits no slot_label' '
    flux ion-resource match allocate unlabeled.yaml | extract_R > none.R &&
    labels_of_R < none.R > none.labels &&
    test ! -s none.labels
'

test_expect_success HAVE_JQ 'no stale label leaks onto a later job on reused vertices' '
    out=$(flux ion-resource match allocate ${jobspec_dir}/test010.yaml) &&
    jobid=$(echo "$out" | awk "NR==2{print \$1}") &&
    echo "$out" | extract_R | labels_of_R > stale.job1 &&
    grep -qx "small" stale.job1 &&
    flux ion-resource cancel ${jobid} &&
    flux ion-resource match allocate unlabeled.yaml \
        | extract_R | labels_of_R > stale.job2 &&
    test ! -s stale.job2
'

test_expect_success 'reload resource module with the default rv1_nosched format' '
    remove_resource &&
    load_resource \
        load-file=${grug} load-format=grug prune-filters=ALL:core \
        subsystems=containment policy=high traverser=flexible \
        match-format=rv1_nosched
'

test_expect_success HAVE_JQ 'rv1_nosched: R has no scheduling key (feature inert)' '
    flux ion-resource match allocate ${jobspec_dir}/test010.yaml \
        | extract_R > nosched.R &&
    test $(jq "has (\"scheduling\")" nosched.R) = "false"
'

test_expect_success 'remove resource module' '
    remove_resource
'

test_done

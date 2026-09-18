#!/bin/sh

test_description='Test matched slot label emission into R scheduling data'

. $(dirname $0)/sharness.sh

jgf="${SHARNESS_TEST_SRCDIR}/data/resource/jgfs/tiny.json"
grug_small="${SHARNESS_TEST_SRCDIR}/data/resource/grugs/small.graphml"
jobspec_dir="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/flexible"
query="../../resource/utilities/resource-query"

#
# The flexible traverser tags each vertex consumed by a matched slot with the
# winning slot's label in the vertex's ephemeral data, which the rv1 writer
# emits under R's "scheduling" key as
# scheduling.graph.nodes[].metadata.ephemeral.slot_label.  The shell uses this
# to select the task command bound to the matched slot label.  These tests
# assert the label reaches R and that stale labels do not leak across jobs.
#

if command -v jq >/dev/null 2>&1; then
    test_set_prereq HAVE_JQ
fi

# Collect the set of distinct slot_label values present across all allocated
# jobs in an rv1 test-output file (one JSON object per allocated job).
labels_in() {
    grep '^{' "$1" | jq -r '
        .scheduling.graph.nodes[].metadata.ephemeral.slot_label // empty
    ' | sort -u
}

# labels present in the Nth allocated job (1-indexed)
labels_in_job() {
    grep '^{' "$1" | sed -n "${2}p" | jq -r '
        .scheduling.graph.nodes[].metadata.ephemeral.slot_label // empty
    ' | sort -u
}

# The slot_label carried by vertices of a given resource type in the first
# allocated job (used to check that the label lands on the expected leaves).
label_on_type() {
    grep '^{' "$1" | sed -n '1p' | jq -r --arg t "$2" '
        .scheduling.graph.nodes[]
        | select(.metadata.type == $t)
        | .metadata.ephemeral.slot_label // empty
    ' | sort -u
}

# A two-branch, two-task jobspec: one command bound to each xor_slot label.
# tuolumne needs a gpu; tioga is cpu-only. Which branch matches (and hence
# which label is tagged) depends on whether the target graph has gpus.
write_multitask() {
    cat >"$1" <<-EOF
	version: 9999
	resources:
	  - type: xor_slot
	    count: 1
	    label: tuolumne
	    with:
	      - type: core
	        count: 8
	      - type: gpu
	        count: 1
	  - type: xor_slot
	    count: 1
	    label: tioga
	    with:
	      - type: core
	        count: 16
	attributes:
	  system:
	    duration: 3600
	tasks:
	  - command: [ "tuolumne_app" ]
	    slot: tuolumne
	    count:
	      per_slot: 1
	  - command: [ "tioga_app" ]
	    slot: tioga
	    count:
	      per_slot: 1
	EOF
}

test_expect_success HAVE_JQ 'xor-slot: matched branch label appears in R' '
    cat >cmds_xor <<-EOF &&
	match allocate ${jobspec_dir}/test010.yaml
	quit
	EOF
    ${query} -L ${jgf} -f jgf -S CA -P first -T flexible -F rv1 \
        -t xor.out < cmds_xor &&
    labels_in xor.out > xor.labels &&
    test $(wc -l < xor.labels) -eq 1 &&
    grep -qx "small" xor.labels
'

test_expect_success HAVE_JQ 'or-slot: DP-selected branch label appears in R' '
    cat >or.yaml <<-EOF &&
	version: 9999
	resources:
	  - type: slot
	    count: 1
	    label: alpha
	    with:
	      - type: core
	        count: 4
	  - type: slot
	    count: 1
	    label: beta
	    with:
	      - type: core
	        count: 2
	      - type: gpu
	        count: 1
	attributes:
	  system:
	    duration: 3600
	tasks:
	  - command: [ "app" ]
	    slot: alpha
	    count:
	      per_slot: 1
	EOF
    cat >cmds_or <<-EOF &&
	match allocate or.yaml
	quit
	EOF
    ${query} -L ${jgf} -f jgf -S CA -P first -T flexible -F rv1 \
        -t or.out < cmds_or &&
    labels_in or.out > or.labels &&
    grep -qx "alpha" or.labels
'

test_expect_success HAVE_JQ 'unlabeled slot emits no slot_label' '
    cat >nolabel.yaml <<-EOF &&
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
    cat >cmds_none <<-EOF &&
	match allocate nolabel.yaml
	quit
	EOF
    ${query} -L ${jgf} -f jgf -S CA -P first -T flexible -F rv1 \
        -t none.out < cmds_none &&
    labels_in none.out > none.labels &&
    test ! -s none.labels
'

test_expect_success HAVE_JQ 'no stale label leaks into a later job on reused vertices' '
    cat >cmds_stale <<-EOF &&
	match allocate or.yaml
	cancel 1
	match allocate nolabel.yaml
	quit
	EOF
    ${query} -L ${jgf} -f jgf -S CA -P first -T flexible -F rv1 \
        -t stale.out < cmds_stale &&
    labels_in_job stale.out 1 > stale.job1 &&
    labels_in_job stale.out 2 > stale.job2 &&
    grep -qx "alpha" stale.job1 &&
    test ! -s stale.job2
'

#
# Multiple-task jobspecs: each xor_slot label has its own task command. The
# emitted label is the one the shell would use to pick that task's command.
#

test_expect_success HAVE_JQ 'multi-task: gpu graph tags the gpu branch label' '
    write_multitask multitask.yaml &&
    cat >cmds_mt_gpu <<-EOF &&
	match allocate multitask.yaml
	quit
	EOF
    ${query} -L ${jgf} -f jgf -S CA -P first -T flexible -F rv1 \
        -t mt_gpu.out < cmds_mt_gpu &&
    labels_in mt_gpu.out > mt_gpu.labels &&
    test $(wc -l < mt_gpu.labels) -eq 1 &&
    grep -qx "tuolumne" mt_gpu.labels
'

test_expect_success HAVE_JQ 'multi-task: same jobspec on gpu-free graph tags the cpu branch label' '
    write_multitask multitask.yaml &&
    cat >cmds_mt_cpu <<-EOF &&
	match allocate multitask.yaml
	quit
	EOF
    ${query} -L ${grug_small} -f grug -S CA -P first -T flexible -F rv1 \
        -t mt_cpu.out < cmds_mt_cpu &&
    labels_in mt_cpu.out > mt_cpu.labels &&
    test $(wc -l < mt_cpu.labels) -eq 1 &&
    grep -qx "tioga" mt_cpu.labels
'

test_expect_success HAVE_JQ 'multi-task: high policy tags the same matched branch' '
    write_multitask multitask.yaml &&
    cat >cmds_mt_high <<-EOF &&
	match allocate multitask.yaml
	quit
	EOF
    ${query} -L ${jgf} -f jgf -S CA -P high -T flexible -F rv1 \
        -t mt_high.out < cmds_mt_high &&
    labels_in mt_high.out > mt_high.labels &&
    grep -qx "tuolumne" mt_high.labels
'

#
# Nested xor_slots: the inner matched branch and the outer group both carry a
# label. Both must be present, tagged on the vertices each level consumes.
#

test_expect_success HAVE_JQ 'nested xor: inner and outer labels both appear' '
    cat >cmds_nested <<-EOF &&
	match allocate ${jobspec_dir}/test012.yaml
	quit
	EOF
    ${query} -L ${jgf} -f jgf -S CA -P first -T flexible -F rv1 \
        -t nested.out < cmds_nested &&
    labels_in nested.out > nested.labels &&
    grep -qx "a1" nested.labels &&
    grep -qx "default" nested.labels &&
    label_on_type nested.out gpu > nested.gpu.label &&
    grep -qx "a1" nested.gpu.label
'

#
# A slot spanning several leaf types (including a multi-unit leaf like memory)
# tags every consumed leaf, not just the first.
#

test_expect_success HAVE_JQ 'label tags every leaf type a slot consumes' '
    cat >memslot.yaml <<-EOF &&
	version: 9999
	resources:
	  - type: slot
	    count: 1
	    label: memslot
	    with:
	      - type: core
	        count: 2
	      - type: memory
	        count: 4
	attributes:
	  system:
	    duration: 3600
	tasks:
	  - command: [ "app" ]
	    slot: memslot
	    count:
	      per_slot: 1
	EOF
    cat >cmds_mem <<-EOF &&
	match allocate memslot.yaml
	quit
	EOF
    ${query} -L ${jgf} -f jgf -S CA -P first -T flexible -F rv1 \
        -t mem.out < cmds_mem &&
    label_on_type mem.out core > mem.core.label &&
    label_on_type mem.out memory > mem.memory.label &&
    grep -qx "memslot" mem.core.label &&
    grep -qx "memslot" mem.memory.label
'

#
# Distinct labels across separate jobs: each job carries only its own label.
#

test_expect_success HAVE_JQ 'two labeled jobs each carry only their own label' '
    write_multitask multitask.yaml &&
    cat >single_beta.yaml <<-EOF &&
	version: 9999
	resources:
	  - type: slot
	    count: 1
	    label: beta_only
	    with:
	      - type: core
	        count: 4
	attributes:
	  system:
	    duration: 3600
	tasks:
	  - command: [ "app" ]
	    slot: beta_only
	    count:
	      per_slot: 1
	EOF
    cat >cmds_two <<-EOF &&
	match allocate multitask.yaml
	match allocate single_beta.yaml
	quit
	EOF
    ${query} -L ${jgf} -f jgf -S CA -P first -T flexible -F rv1 \
        -t two.out < cmds_two &&
    labels_in_job two.out 1 > two.job1 &&
    labels_in_job two.out 2 > two.job2 &&
    grep -qx "tuolumne" two.job1 &&
    test $(wc -l < two.job1) -eq 1 &&
    grep -qx "beta_only" two.job2 &&
    test $(wc -l < two.job2) -eq 1
'

test_done

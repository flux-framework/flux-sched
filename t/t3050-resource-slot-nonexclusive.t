#!/bin/sh

test_description='Test that resources with explicit exclusive:false under slots can be shared'

. $(dirname $0)/sharness.sh

cmd_dir="${SHARNESS_TEST_SRCDIR}/data/resource/commands/slot-nonexclusive"
exp_dir="${SHARNESS_TEST_SRCDIR}/data/resource/expected/slot-nonexclusive"
jgf="${SHARNESS_TEST_SRCDIR}/data/resource/jgfs/rabbit.json"
query="../../resource/utilities/resource-query"

#
# Test that SSDs with explicit exclusive:false can be matched
#

cmds001="${cmd_dir}/cmds-ssd-shared-test.in"
test001_desc="match shared SSDs under slot multiple times"
test_expect_success "${test001_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds001} > cmds001 &&
    ${query} -L ${jgf} -f jgf -t 001.R.out < cmds001 &&
    grep "JOBID=1" 001.R.out &&
    grep "JOBID=2" 001.R.out &&
    grep "No matching resources found" 001.R.out
'

#
# Test that exclusive SSDs are exhausted after both chassis are used
#
test002_desc="exclusive SSDs limit allocations to available chassis"
test_expect_success "${test002_desc}" '
    cat > ssd-exclusive-2.yaml <<-EOF &&
	version: 9999
	resources:
	  - type: slot
	    count: 1
	    label: testslot
	    with:
	      - type: chassis
	        count: 1
	        with:
	          - type: ssd
	            count: 2
	            # implicit exclusive: true
	          - type: node
	            count: 1
	            with:
	              - type: core
	                count: 1
	attributes:
	  system:
	    duration: 3600
	tasks:
	  - command: [ "app" ]
	    slot: testslot
	    count:
	      per_slot: 1
	EOF
    cat > cmds002 <<-EOF &&
	match allocate ${PWD}/ssd-exclusive-2.yaml
	match allocate ${PWD}/ssd-exclusive-2.yaml
	match allocate ${PWD}/ssd-exclusive-2.yaml
	quit
	EOF
    ${query} -L ${jgf} -f jgf -t 002.R.out < cmds002 &&
    test $(grep -c "RESOURCES=ALLOCATED" 002.R.out) -eq 2 &&
    grep "No matching resources found" 002.R.out
'

#
# Test that shared SSDs also work (same limit due to chassis exhaustion)
# This test verifies the implementation doesn'\''t break normal allocation patterns
#
test003_desc="shared SSDs allow same allocations when chassis is the constraint"
test_expect_success "${test003_desc}" '
    cat > ssd-shared-2.yaml <<-EOF &&
	version: 9999
	resources:
	  - type: slot
	    count: 1
	    label: testslot
	    with:
	      - type: chassis
	        count: 1
	        with:
	          - type: ssd
	            count: 2
	            exclusive: false
	          - type: node
	            count: 1
	            with:
	              - type: core
	                count: 1
	attributes:
	  system:
	    duration: 3600
	tasks:
	  - command: [ "app" ]
	    slot: testslot
	    count:
	      per_slot: 1
	EOF
    cat > cmds003 <<-EOF &&
	match allocate ${PWD}/ssd-shared-2.yaml
	match allocate ${PWD}/ssd-shared-2.yaml
	match allocate ${PWD}/ssd-shared-2.yaml
	quit
	EOF
    ${query} -L ${jgf} -f jgf -t 003.R.out < cmds003 &&
    test $(grep -c "RESOURCES=ALLOCATED" 003.R.out) -eq 2 &&
    grep "No matching resources found" 003.R.out
'

test_done

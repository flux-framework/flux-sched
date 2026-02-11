#!/bin/sh
#
test_description='Ensure flux ion-resource can add and remove resources from the resource graph'

. `dirname $0`/sharness.sh

if test_have_prereq ASAN; then
    skip_all='skipping issues tests under AddressSanitizer'
    test_done
fi
SIZE=1
test_under_flux ${SIZE}

exp_dir="${SHARNESS_TEST_SRCDIR}/data/resource/expected/elastic"

test_expect_success 'load test configuration' '
	flux module remove sched-simple &&
	flux module remove resource &&
	flux config load <<EOF &&
[resource]
noverify = true
norestrict = true
path="${SHARNESS_TEST_SRCDIR}/data/resource/jgfs/elastic/add-remove-node-test.json"
EOF
	flux module load resource monitor-force-up &&
	flux module load sched-fluxion-resource &&
	flux module load sched-fluxion-qmanager &&
	flux module unload job-list &&
	flux module list &&
	flux queue start --all --quiet &&
	flux resource list &&
	flux resource status
'

test_expect_failure 'correctly fails on null input' '
	flux ion-resource add-subgraph ""
'

test_expect_failure 'correctly fails on subgraph with different root' '
	flux ion-resource add-subgraph ${SHARNESS_TEST_SRCDIR}/data/resource/jgfs/issue1260.json
'

test_expect_success 'successfully add subgraph' '
	flux ion-resource add-subgraph ${SHARNESS_TEST_SRCDIR}/data/resource/jgfs/elastic/node-add-test.json &&
	flux ion-resource find --format=jgf status=up > node-add.out &&
	test_cmp node-add.out ${exp_dir}/node-add.out
'

test_expect_failure 'correctly fails on null input' '
	flux ion-resource remove-subgraph ""
'

test_expect_failure 'correctly fails on malformed input' '
	flux ion-resource remove-subgraph /cluster0
'

test_expect_success 'successfully remove subgraph' '
	flux ion-resource remove-subgraph /medium0/rack0/node0 &&
	flux ion-resource find --format=jgf status=up > node-remove.out &&
	test_cmp node-remove.out ${exp_dir}/node-remove.out
'

test_expect_success 'reload fluxion and resource module' '
	flux module remove sched-fluxion-qmanager &&
	flux module remove sched-fluxion-resource
'

test_done

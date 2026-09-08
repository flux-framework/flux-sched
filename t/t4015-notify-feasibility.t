#!/bin/sh
#set -x

# Adapted from t2317 and t1031

test_description='Test the functionality of match satisfiability after module restart'

. `dirname $0`/sharness.sh

conf_base=${SHARNESS_TEST_SRCDIR}/conf.d
notify_base=`readlink -e ${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/satisfiability`

SIZE=4
export FLUX_URI_RESOLVE_LOCAL=t
export FLUX_SCHED_MODULE=none

test_under_flux $SIZE full --test-exit-mode=leader

force_down () {
	flux python -c "import flux; flux.Flux().rpc(\"resource.monitor-force-down\", {\"ranks\":\"$1\"}).get()"
}

# Usage: wait_for_node_count N
wait_for_node_count() {
    retries=5
    while test $retries -ge 0; do
        test $(flux resource list -s all -no {nnodes}) -eq $1 && return 0
        retries=$(($retries-1))
        sleep 0.1
    done
    return 1
}

# If force-down is not supported then this version of flux-core does not
# support shrink, so skip all tests:
if ! force_down "" 2>/dev/null ; then
       skip_all='resource.monitor-force-down failed, skipping all tests'
	test_done
fi

# Test that the feasibility module gets and remembers shrink updates
test_expect_success 'loading non-load-file resource module works' '
    load_resource &&
    test -z "$(flux dmesg -c | grep -q err)"
'

test_expect_success 'loading feasibility from non-load-file resource module works' '
    load_feasibility &&
    test -z "$(flux dmesg -c | grep -q err)"
'

test_expect_success 'a job on all ranks is satisfiable' '
    flux ion-resource -v match satisfiability ${notify_base}/shrink4.yaml
'

test_expect_success 'disconnect rank 3' '
	flux overlay disconnect 3
'

test_expect_success 'there are now only 3 nodes' '
    wait_for_node_count 3
'

test_expect_success 'a 4 node job is now unsatisfiable' '
    test_must_fail flux ion-resource match satisfiability ${notify_base}/shrink4.yaml
'

test_expect_success 'but a 3 node job is satisfiable' '
    flux ion-resource match satisfiability ${notify_base}/shrink3.yaml
'

test_expect_success 'a 4 node job is unsatisfiable after feasibility restart' '
    reload_feasibility &&
    test_must_fail flux ion-resource match satisfiability ${notify_base}/shrink4.yaml
'

test_expect_success 'removing resource works and removes feasibility' '
    remove_resource &&
    flux dmesg -c | grep -q "exiting due to sched-fluxion-resource.notify failure"
'

# Test that the feasibility module gets and remembers add and remove subgraph updates
test_expect_success 'loading non-load-file resource and feasibility modules works' '
	flux config load <<-'EOF' &&
	[resource]
	noverify=true
	norestrict=true
	path="${SHARNESS_TEST_SRCDIR}/data/resource/jgfs/elastic/tiny-remove-add-test.json"
	EOF
    flux module reload resource monitor-force-up &&
    load_resource &&
    load_feasibility
'

test_expect_success 'a two-node job nodes is satisfiable' '
    flux run -N2 --dry-run sleep inf | tee twonode.json | jq "del(.attributes)"  &&
    flux ion-resource -v match satisfiability twonode.json
'

test_expect_success 'remove one node with remove-subgraph' '
    flux ion-resource remove-subgraph /tiny0/rack0/node1 &&
    flux ion-resource find exists=true
'

test_expect_success 'a one-node job is still satisfiable' '
    flux run -N1 --dry-run sleep inf >onenode.json &&
    flux ion-resource match satisfiability onenode.json
'

test_expect_success 'a two-node job is now unsatisfiable' '
    test_must_fail flux ion-resource match satisfiability twonode.json
'

test_expect_success 'a two-node job is still unsatisfiable after feasibility restart' '
    reload_feasibility &&
    test_must_fail flux ion-resource match satisfiability twonode.json
'

test_expect_success 'a one-node job is still satisfiable after feasibility restart' '
    flux ion-resource match satisfiability onenode.json
'

test_expect_success 'add one node back with add-subgraph' '
    flux ion-resource add-subgraph ${SHARNESS_TEST_SRCDIR}/data/resource/jgfs/tiny.json &&
    flux ion-resource find exists=true
'

test_expect_success 'a two-node job is now satisfiable again' '
    flux ion-resource match satisfiability twonode.json
'

test_expect_success 'a two-node job is still satisfiable after feasibility restart' '
    reload_feasibility &&
    flux ion-resource match satisfiability twonode.json
'

test_expect_success 'removing resource works' '
    remove_resource
'

test_done

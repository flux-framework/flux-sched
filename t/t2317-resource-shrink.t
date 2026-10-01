#!/bin/sh

test_description='Test resource shrink in a subinstance running fluxion'

. `dirname $0`/sharness.sh

SIZE=4
LAST_RANK=$(($SIZE-1))

# Let modprobe load the fluxion modules in this instance and in any
# subinstance a test starts, rather than loading them by hand.
# sched-sharness.sh disables them by default.
unset FLUX_MODPROBE_DISABLE

# Match configurations to sweep, as match-format:match-policy pairs.
#
# The formats differ in the scheduling key fluxion writes into a job's
# R, which is what a subinstance rebuilds its graph from: rv1 emits a
# complete JGF, loaded with the JGF reader, while rv1_shorthand emits a
# JGF with no core vertices, which cannot be rebuilt from and falls back
# to the rv1exec reader. rv1_nosched writes no scheduling key at all and
# falls back the same way, so it is left out as a third configuration
# covering no third path.
#
# Usage: MATCH_CONFIGS="rv1:low ..." make check TESTS=t2317-resource-shrink.t
MATCH_CONFIGS=${MATCH_CONFIGS:-"rv1:lonodex rv1_shorthand:firstnodex"}

mkdir -p conf.d

# Configure resources from an R with a JGF scheduling key instead of
# letting hwloc discover them, so this instance holds a real resource
# graph and the R it hands to a subinstance derives from one.
#
# flux-ion-R(1) is not installed with flux-sched, and this runs before
# the test instance starts, so run the in-tree copy directly rather than
# as a flux(1) subcommand.
ion_R="${SHARNESS_TEST_SRCDIR}/../src/cmd/flux-ion-R.py"
flux R encode --hosts="test[0-$LAST_RANK]" --cores=0-3 \
	| PYTHONPATH="${SHARNESS_TEST_SRCDIR}/../src/python${PYTHONPATH:+:${PYTHONPATH}}" \
	  flux python $ion_R encode >R.jgf ||
	error "failed to generate R"

cat >conf.d/resource.toml <<-EOT
[resource]
noverify = true
path = "$(pwd)/R.jgf"
EOT

test_under_flux $SIZE full --test-exit-mode=leader \
	-o,--config-path=$(pwd)/conf.d
export FLUX_URI_RESOLVE_LOCAL=t

# Nodes this instance can still start a subinstance on. Resources here
# are configured, so a lost node is only marked down and stays in "all".
node_count() {
    run_timeout 30 flux resource list -s up -no {nnodes}
}

# Nodes the subinstance running as job $1 has. Counts "all" on purpose:
# a shrink must drop the lost rank from the resource set entirely, so
# counting "up" would pass even if the shrink never happened.
sub_node_count() {
    run_timeout 30 flux proxy $1 flux resource list -s all -no {nnodes}
}

# Nodes fluxion reports in the subinstance running as job $1, rather
# than the count from the core resource module. Prints nothing if
# fluxion is not answering, so callers must quote it.
#
# N.B. FLUX_RESOURCE_LIST_RPC must be set on flux-resource-list(1)
# itself. On a surrounding test(1) it has no effect, and the command
# substitution inside then silently queries the core resource module.
sub_sched_node_count() {
    FLUX_RESOURCE_LIST_RPC=sched.resource-status \
        run_timeout 30 flux proxy $1 flux resource list -s all -no {nnodes}
}

# Usage: expected_reader jobid
#
# Which reader the subinstance running as job $1 should have rebuilt its
# graph with, derived from the R it was handed so that any match-format
# may be swept: the JGF reader for a complete JGF, rv1exec for shorthand
# JGF or for an R with no scheduling key.
expected_reader() {
    writer=$(flux job info $1 R | jq -r ".scheduling.writer // \"\"")
    if flux job info $1 R | jq -e "has(\"scheduling\")" >/dev/null &&
       test "$writer" != "fluxion:jgf_shorthand"; then
        echo JGF
    else
        echo rv1exec
    fi
}

# Usage: wait_for_node_count N
wait_for_node_count() {
    retries=50
    while test $retries -ge 0; do
        test "$(node_count)" = "$1" && return 0
        retries=$(($retries-1))
        sleep 0.1
    done
    return 1
}

# Usage: wait_for_sub_node_count jobid N
wait_for_sub_node_count() {
    retries=50
    while test $retries -ge 0; do
        test "$(sub_node_count $1)" = "$2" && return 0
        retries=$(($retries-1))
        sleep 0.1
    done
    return 1
}

# Usage: wait_for_sub_sched_node_count jobid N
wait_for_sub_sched_node_count() {
    retries=50
    while test $retries -ge 0; do
        test "$(sub_sched_node_count $1 2>/dev/null)" = "$2" && return 0
        retries=$(($retries-1))
        sleep 0.1
    done
    return 1
}

test_expect_success 'fluxion was loaded by modprobe' '
	flux module list >modules.out &&
	grep sched-fluxion-qmanager modules.out
'
test_expect_success 'configured resources gave this instance a JGF graph' '
	flux dmesg -H >encl.log &&
	grep "datastore loaded with JGF reader" encl.log
'

# A shrink reaches a subinstance when the enclosing instance loses a
# node and reports it lost to the job holding it. Each configuration
# spans every node this instance still has, so the lost node belongs to
# the subinstance whichever ones the match policy chose. Disconnecting
# a rank terminates its broker and the node does not come back, so the
# instance is one node smaller for the configuration that follows.
nnodes=$SIZE
for cfg in $MATCH_CONFIGS; do
    fmt=${cfg%%:*}
    policy=${cfg##*:}
    shrunk=$(($nnodes-1))
    lost_rank=$shrunk

    test_expect_success "$cfg: reload fluxion with this configuration" '
	flux module remove sched-fluxion-feasibility &&
	flux module remove sched-fluxion-qmanager &&
	flux module reload -f sched-fluxion-resource \
		match-format=$fmt policy=$policy &&
	flux module load sched-fluxion-qmanager &&
	flux module stats sched-fluxion-qmanager >/dev/null &&
	flux module load sched-fluxion-feasibility
    '
    test_expect_success "$cfg: this instance has $nnodes nodes" '
	flux resource status &&
	wait_for_node_count $nnodes
    '
    test_expect_success "$cfg: start a subinstance on all $nnodes nodes" '
	subid=$(run_timeout 60 flux alloc \
		--broker-opts=-Sbroker.module-nopanic=1 --bg -xN$nnodes \
		-o exit-timeout=none --conf=tbon.topo=kary:0) &&
	echo $subid >subid
    '
    test_expect_success "$cfg: fluxion was loaded in the subinstance" '
	run_timeout 30 flux proxy $(cat subid) flux module list \
		>sub-modules.out &&
	grep sched-fluxion-qmanager sub-modules.out
    '
    # Check the subinstance used the reader its R calls for, so the
    # sweep cannot silently cover one path twice, e.g. if a format
    # stopped emitting a usable scheduling key.
    test_expect_success "$cfg: subinstance used the expected reader" '
	run_timeout 30 flux proxy $(cat subid) flux dmesg -H \
		>sub-$fmt-$policy.load.log &&
	grep -o "datastore loaded with .* reader" sub-$fmt-$policy.load.log \
		>sub-$fmt-$policy.reader &&
	test_debug "cat sub-$fmt-$policy.reader" &&
	grep -q "with $(expected_reader $(cat subid)) reader" \
		sub-$fmt-$policy.reader &&
	run_timeout 30 flux proxy $(cat subid) flux dmesg -C
    '
    test_expect_success "$cfg: subinstance reports $nnodes nodes" '
	test $(sub_node_count $(cat subid)) -eq $nnodes
    '
    # Query fluxion before the shrink as well as after:
    # sched.resource-status caches its view of the resource set, so
    # without a query here to fill that cache, the one afterwards builds
    # it from scratch and a stale cache goes unnoticed.
    test_expect_success "$cfg: subinstance scheduler reports $nnodes nodes" '
	test $(sub_sched_node_count $(cat subid)) -eq $nnodes
    '
    test_expect_success "$cfg: a $nnodes node job runs in the subinstance" '
	run_timeout 60 flux proxy $(cat subid) flux run -N$nnodes hostname
    '
    test_expect_success "$cfg: disconnect rank $lost_rank" '
	run_timeout 30 flux overlay disconnect $lost_rank
    '
    # The lost rank must be gone from the resource set, not just down:
    # it is never coming back, so a job needing it has to be rejected
    # rather than left pending.
    test_expect_success "$cfg: subinstance shrinks to $shrunk nodes" '
	wait_for_sub_node_count $(cat subid) $shrunk &&
	run_timeout 30 flux proxy $(cat subid) flux resource list -s all &&
	run_timeout 30 flux proxy $(cat subid) flux resource status \
		>sub-$fmt-$policy.status &&
	test_debug "cat sub-$fmt-$policy.status" &&
	test_must_fail grep -E "^ *(down|drain)" sub-$fmt-$policy.status
    '
    test_expect_success "$cfg: subinstance scheduler reports $shrunk nodes" '
	wait_for_sub_sched_node_count $(cat subid) $shrunk &&
	FLUX_RESOURCE_LIST_RPC=sched.resource-status \
		run_timeout 30 flux proxy $(cat subid) flux resource list -s all
    '
    test_expect_success "$cfg: fluxion is still running in the subinstance" '
	run_timeout 30 flux proxy $(cat subid) flux module list \
		>sub-modules2.out &&
	grep sched-fluxion-resource sub-modules2.out &&
	grep sched-fluxion-qmanager sub-modules2.out
    '
    test_expect_success "$cfg: no fluxion errors logged in subinstance" '
	run_timeout 30 flux proxy $(cat subid) flux dmesg -H \
		>sub-$fmt-$policy.log &&
	test_debug "cat sub-$fmt-$policy.log" &&
	test_must_fail grep -E "sched-fluxion.*(err|crit)" sub-$fmt-$policy.log
    '
    test_expect_success "$cfg: a $shrunk node job still runs" '
	run_timeout 60 flux proxy $(cat subid) flux run -N$shrunk hostname
    '
    test_expect_success "$cfg: a $nnodes node job is unsatisfiable" '
	test_must_fail run_timeout 30 flux proxy $(cat subid) \
		flux run -N$nnodes hostname 2>sub-$fmt-$policy.err &&
	test_debug "cat sub-$fmt-$policy.err" &&
	grep -i unsatisfiable sub-$fmt-$policy.err
    '
    test_expect_success "$cfg: shutdown subinstance" '
	run_timeout 60 flux shutdown $(cat subid) &&
	flux job wait-event -t 60 $(cat subid) clean
    '
    nnodes=$shrunk
done

test_done

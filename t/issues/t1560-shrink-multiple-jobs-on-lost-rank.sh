#!/bin/bash -e
#
#  Make sure that Fluxion can shrink a rank that more than one job uses.
#
#  When a rank is lost, Fluxion releases the resources of that rank from
#  each job that has state on the rank.  Before the fix, Fluxion added the
#  counts of all jobs on the rank, and subtracted this sum from each job.
#  With N jobs on the rank, the reduction for each job was the counts of N
#  jobs.  At the cluster vertex, this reduction was more than the span of
#  the job, and the shrink failed with this error:
#
#    partial cancel by ranks ("3") failed: mod_agfilter: \
#        planner_multi_reduce_span returned -1.
#
#  The shrink then stopped before Fluxion removed the rank from the graph.
#  No function marks a lost rank as down.  As a result, the scheduler could
#  give the rank to new jobs.  Make sure that the shrink causes no error,
#  and that a new job cannot use the rank.
#
#  The last check asks for the rank the shrink removed.  An exception on
#  that job is not enough on its own.  A scheduler that wrongly allocated
#  the lost rank and then failed to start the job there also raises an
#  exception.  Require the scheduler's own rejection: an alloc exception
#  whose reason is "unsatisfiable", and no alloc event at all.
#

log() { printf "issue#1560: $@\n" >&2; }
die() { log "$@"; exit 1; }

if test "$ISSUE_1560_ACTIVE" != "t"; then
    export ISSUE_1560_ACTIVE=t
    # The resources must come from discovery, not from configuration,
    # because flux-core does not report configured resources as lost.
    # On a host with less than two cores, set FLUX_HWLOC_XMLFILE to a
    # topology file, and set FLUX_HWLOC_XMLFILE_NOT_THISSYSTEM=1.  Do
    # these steps before you start this test.
    log "Re-launching test script under flux-start"
    exec flux start -Sbroker.module-nopanic=1 --conf=resource.norestrict=true \
	-s 4 $0
fi

force_down() {
    flux python -c \
"import flux; flux.Flux().rpc(\"resource.monitor-force-down\", {\"ranks\":\"$1\"}).get()"
}

# The number of nodes the resource module reports.
rl_nnodes() {
    flux resource list -s all -no {nnodes} 2>/dev/null || echo unknown
}

# Usage: graph_rank_count rank
#
# The resources the scheduler's own graph still holds for a rank.  The
# by_rank map of the resource module statistics is the scheduler's view of
# the graph, and a shrink has to drive the lost rank's entry to zero.
#
# Do not use sched.resource-status here.  Its "all" set comes from the R
# the module recorded at initialization, so it still names the lost rank
# after a successful shrink and cannot time one.
graph_rank_count() {
    flux module stats sched-fluxion-resource 2>/dev/null | flux python -c "
import json, sys
want = int(sys.argv[1])
try:
    by_rank = json.load(sys.stdin)['by_rank']
except Exception:
    print('unknown')
    sys.exit(0)
total = 'gone'
for key, count in by_rank.items():
    for part in key.strip('[]').split(','):
        if '-' in part:
            lo, hi = part.split('-')
            ranks = range(int(lo), int(hi) + 1)
        else:
            ranks = [int(part)]
        if want in ranks:
            total = count
print(total)
" $1
}

# Usage: wait_for_lost_rank rank nnodes
#
# A fixed sleep either wastes time or races the shrink, and neither outcome
# says the rank is gone.  Poll the two observable effects of the removal
# instead, for at most 30 seconds: flux-core stops listing the node, and
# the scheduler's graph holds nothing on that rank.
wait_for_lost_rank() {
    local i=0
    while test $i -lt 300; do
	if test "$(rl_nnodes)" = "$2" && test "$(graph_rank_count $1)" = "0"; then
	    return 0
	fi
	i=$((i+1))
	sleep 0.1
    done
    log "resource list reports $(rl_nnodes) nodes;" \
	"the scheduler's graph holds $(graph_rank_count $1) on rank $1"
    return 1
}

if ! force_down "" 2>/dev/null; then
    log "resource.monitor-force-down unsupported, skipping test"
    exit 0
fi

ncores=$(flux resource list -s all -no {ncores})
nnodes=$(flux resource list -s all -no {nnodes})
test $nnodes -eq 4 || die "expected 4 nodes, got $nnodes"
test $ncores -ge 8 \
	|| die "test needs >=2 cores per rank, instance has $ncores over $nnodes nodes"

log "Loading fluxion..."
flux module remove sched-simple
flux module load sched-fluxion-resource policy=first
flux module load sched-fluxion-qmanager
flux module stats sched-fluxion-qmanager >/dev/null
test "$(graph_rank_count 3)" -gt 0 \
	|| die "the scheduler's graph has nothing on rank 3 before the shrink"

log "Submitting two single-core jobs, both on rank 3..."
for i in 1 2; do
    id=$(flux submit -N1 -n1 -o cpu-affinity=off --requires=rank:3 sleep inf)
    flux job wait-event -t 30 $id start >/dev/null
done
test $(flux jobs --filter=running -no {id} | wc -l) -eq 2 \
	|| die "expected 2 running jobs on rank 3"

log "Forcing rank 3 down to trigger a shrink..."
flux dmesg -C
force_down 3
wait_for_lost_rank 3 3 \
	|| die "rank 3 was still present 30s after the forced down"
log "flux-core lists 3 nodes and the scheduler's graph holds nothing on rank 3"
flux dmesg -H >dmesg.out
grep -q "removed ranks\|shrink" dmesg.out \
	|| die "no shrink was attempted; log was: $(cat dmesg.out)"
grep -E "partial cancel by ranks|planner_multi_reduce_span|cancel_vertex failed|shrink \(lost\)" \
	dmesg.out >shrink-errors.out || true
if test -s shrink-errors.out; then
    log "FAIL: fluxion reported errors during shrink:"
    cat shrink-errors.out >&2
    exit 1
fi

log "Make sure that new jobs cannot use rank 3..."
flux cancel --all --quiet || true
flux queue idle -t 30s
test "$(graph_rank_count 3)" = "0" \
	|| die "the scheduler's graph still holds $(graph_rank_count 3) on rank 3"
test "$(rl_nnodes)" = "3" \
	|| die "flux-core lists $(rl_nnodes) nodes after the shrink, not 3"
id=$(flux submit -n1 -o cpu-affinity=off --requires=rank:3 true)
flux job wait-event -t 10 $id exception >exception.out 2>&1 \
	|| die "the scheduler did not reject a job that requires lost rank 3"
log "exception: $(cat exception.out)"
flux job eventlog $id >eventlog.out

# The exception has to be the scheduler's rejection of a request it cannot
# satisfy, not a start failure on a rank it should never have chosen.
grep -q 'type="alloc"' exception.out \
	|| die "the exception is not an alloc exception: $(cat exception.out)"
grep -q 'unsatisfiable' exception.out \
	|| die "the scheduler did not call the request unsatisfiable: $(cat exception.out)"
if awk '{print $2}' eventlog.out | grep -qx alloc; then
    log "eventlog of $id was:"
    cat eventlog.out >&2
    die "the scheduler allocated resources for a job that requires lost rank 3"
fi
log "rejected with: $(sed -n 's/.*note="\(.*\)" userid.*/\1/p' exception.out)"

log "PASS: rank 3 shrank cleanly with two jobs on it"

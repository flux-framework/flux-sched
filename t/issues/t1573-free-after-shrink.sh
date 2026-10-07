#!/bin/bash -e
#
#  Make sure that a shrink of a rank of a multi-rank job leaves the free
#  protocol intact.
#
#  A job runs on several ranks.  A shrink removes one of those ranks, and
#  Fluxion releases the resources of the job on it.  flux-core knows
#  nothing of that release, so when the job ends it still frees every
#  rank, one .free RPC per rank, and marks only the last one final.  Two
#  defects followed:
#
#  1. The partial cancel of a rank that the shrink removed found the rank
#     in by_rank with no vertices, no subgraph root, and failed with
#     "rank_to_root is empty".
#  2. When the frees of the surviving ranks removed the last resources of
#     the job before the final .free RPC, the qmanager called the full
#     removal a protocol error, logged "removed allocation before final
#     .free RPC", and dropped the job.  The final .free RPC then logged
#     "can't find queue for job".
#
#  The order of the frees decides which defect a run meets, so this test
#  runs three passes, each one in its own instance.  flux-core frees a
#  rank when the housekeeping of that rank ends, so the housekeeping
#  command sets the order: it sleeps for a time per rank.  The passes are:
#
#  1. A three-rank job on ranks 1-3 loses rank 1 and frees it last.  The
#     second non-final free removes the last resources of the job in the
#     graph, which is the early full removal of defect 2.
#  2. The same job loses rank 3 and frees it first.  The partial cancel of
#     a rank with no vertices runs first, which is defect 1.  The final
#     free removes the last resources, so no early full removal happens.
#  3. A four-rank job on ranks 0-3 loses rank 3 and frees it last.  Three
#     non-final frees arrive before the final one, so the early full
#     removal happens at the third non-final free.  The final free then
#     reaches a resource module that no longer knows the job: the full
#     cancel gets ENOENT, the adapter accepts it, and no error appears.
#  4. A four-rank job on ranks 0-3 loses ranks 2 and 3 in one shrink, and
#     frees them last.  The second non-final free removes the last
#     resources of the job in the graph.  The third non-final free, of a
#     lost rank, then reaches a resource module that no longer knows the
#     job: the partial cancel gets ENOENT, the adapter accepts it, and no
#     error appears.  With one lost rank this case cannot occur, because
#     the early full removal always happens at the last non-final free.
#
#  The first two passes do not free the ranks in the order of the rank
#  numbers, so they cannot pass by chance.  The protocol does not give the
#  order, so the trace of the frees must show the order that the
#  housekeeping command set.  No two ranks of a pass sleep for the same
#  time, so the order does not depend on how flux-core frees ranks that
#  end housekeeping together.
#
#  The oracle is the whole sequence, not one message: the frees arrive in
#  the order of the pass and only the last one is final; no Fluxion module
#  logs an error; the job retires once; no allocation is left behind; and
#  the surviving ranks are usable by a new job.
#
#  Each pass is a table row with six fields, separated by "|":
#
#    <lost ranks>|<job ranks>|<surviving ranks>|<sleeps>|<free order>|<early removal>
#
#  <sleeps> is a list of "rank:seconds" pairs, separated by commas, that
#  becomes the housekeeping command of the pass.  <free order> is the list
#  of the ranks in the order in which flux-core must free them; the last
#  rank of the list carries the final flag.  <early removal> says if the
#  qmanager must report an early full removal.
#

PASSES="
1|1-3|2-3|1:4,2:1,3:2|2 3 1|yes
3|1-3|1-2|1:4,2:6,3:1|3 1 2|no
3|0-3|0-2|0:1,1:2,2:4,3:7|0 1 2 3|yes
2-3|0-3|0-1|0:1,1:2,2:4,3:7|0 1 2 3|yes
"

# printf takes its first argument as the format, so join the arguments
# first.  If a message goes into the format, a "%" in it or a message of
# more than one argument breaks the output.
log() { printf "issue#1573: %s\n" "$*" >&2; }
die() { log "FAIL: $*"; exit 1; }

if test "$T1573_ACTIVE" != "t"; then
    export T1573_ACTIVE=t
    # The resources must come from discovery, not from configuration,
    # because flux-core does not report configured resources as lost.
    # Housekeeping with release-after=0 is what makes flux-core free the
    # ranks of the job one at a time, when the housekeeping of each rank
    # ends.  The command sleeps for a time that depends on the rank, so
    # that the order of the frees is known.  The command has to succeed,
    # or the job manager drains the node and the last check would fail
    # for an unrelated reason.
    conf=$(mktemp -d "${TMPDIR:-/tmp}/t1573-conf.XXXXXX")
    cat >$conf/resource.toml <<-EOT
	[resource]
	norestrict = true
	EOT
    cat >$conf/housekeeping.toml <<-EOT
	[job-manager.housekeeping]
	command = [ "$conf/housekeeping.sh" ]
	release-after = "0"
	EOT
    rc=0
    while IFS='|' read -r lost jobranks keep sleeps order early; do
	test -n "$lost" || continue
	pass="$lost|$jobranks|$keep|$sleeps|$order|$early"
	# Write the housekeeping command of this pass from the sleep list.
	{
	    printf "#!/bin/sh\n"
	    printf "case \$(flux getattr rank) in\n"
	    for item in $(printf "%s" "$sleeps" | tr ',' ' '); do
		printf "%s) sleep %s ;;\n" "${item%%:*}" "${item##*:}"
	    done
	    printf "*) sleep 1 ;;\n"
	    printf "esac\n"
	    printf "exit 0\n"
	} >$conf/housekeeping.sh
	chmod +x $conf/housekeeping.sh
	log "Re-launching test script under flux-start:" \
	    "job on ranks $jobranks, losing rank $lost, frees in the order $order"
	# The instance must not read the table, so give it no stdin.
	T1573_PASS="$pass" flux start -Sbroker.module-nopanic=1 \
	    -o,--config-path=$conf -s 4 $0 </dev/null || rc=1
    done <<<"$PASSES"
    rm -rf $conf
    exit $rc
fi

IFS='|' read -r LOST JOBRANKS KEEP SLEEPS ORDER EARLY \
	<<<"${T1573_PASS:-1|1-3|2-3|1:4,2:1,3:1|2 3 1|yes}"

# Usage: idset_count idset
#
# The number of ranks in an idset such as "0-2" or "1,3".
idset_count() {
    printf "%s" "$1" | awk -F, \
	'{n=0; for (i=1; i<=NF; i++) {split ($i, r, "-");
	  n += (r[2] == "" ? 1 : r[2] - r[1] + 1)} print n}'
}

NJOB=$(idset_count "$JOBRANKS")
NKEEP=$(idset_count "$KEEP")

# The frees that the test demands, in the format of the check below.
# flux-core marks only the last free of a job final, so every rank of the
# order except the last one carries "non-final".
expected_free_sequence() {
    local rank i=0 n
    set -- $ORDER
    n=$#
    for rank in "$@"; do
	i=$((i+1))
	if test $i -eq $n; then printf "%s:final" $rank
	else printf "%s:non-final " $rank
	fi
    done
}

force_down() {
    flux python -c \
"import flux; flux.Flux().rpc(\"resource.monitor-force-down\", {\"ranks\":\"$1\"}).get()"
}

# The number of nodes flux-core lists.
rl_nnodes() {
    flux resource list -s all -no {nnodes} 2>/dev/null || echo unknown
}

# Usage: graph_rank_count rank
#
# What the scheduler's own graph still holds for a rank.  The by_rank map
# of the resource module statistics is that view, and a shrink has to drive
# the lost rank's entry to zero.  sched.resource-status is no use here: its
# "all" set comes from the R the module recorded at initialization, so it
# still names the lost rank after a successful shrink.
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

# The number of jobs the qmanager still counts as running.
qm_running() {
    flux module stats sched-fluxion-qmanager 2>/dev/null | flux python -c "
import json, sys
try:
    queues = json.load(sys.stdin)['queues']
except Exception:
    print('unknown')
    sys.exit(0)
print(sum(len(q['scheduled_queues']['running']) for q in queues.values()))
"
}

# The cores the scheduler reports as allocated.
sched_allocated() {
    FLUX_RESOURCE_LIST_RPC=sched.resource-status \
	flux resource list -s allocated -no {ncores} 2>/dev/null || echo unknown
}

# Usage: free_sequence trace-file decimal-jobid
#
# The .free requests that the qmanager received for one job, in the order
# of arrival, as "rank:final" or "rank:non-final", one line per request.
#
# The rank and the final flag have to come from the same message, and the
# message has to belong to this job, or an unrelated message could satisfy
# the check.  So walk the trace instead of grepping it: flux module trace
# prints a header line and then the JSON of the message, which ends at the
# first line that is exactly "}".  Keep the header lines of the .free
# requests that the qmanager received, parse the JSON that follows, and
# drop the messages of any other job.
free_sequence() {
    flux python -c '
import json, sys

path, jobid = sys.argv[1], int(sys.argv[2])
header = "sched-fluxion-qmanager rx > sched.free"
lines = open (path).read ().splitlines ()
i = 0
while i < len (lines):
    line = lines[i]
    i += 1
    if header not in line:
        continue
    if i >= len (lines) or lines[i] != "{":
        continue
    body = []
    while i < len (lines):
        body.append (lines[i])
        if lines[i] == "}":
            i += 1
            break
        i += 1
    try:
        msg = json.loads ("\n".join (body))
    except ValueError:
        continue
    if msg.get ("id") != jobid:
        continue
    ranks = ",".join (e["rank"] for e in msg["R"]["execution"]["R_lite"])
    print ("%s:%s" % (ranks, "final" if msg.get ("final") else "non-final"))
' "$1" "$2"
}

# Usage: idset_ranks idset
#
# The ranks of an idset such as "0-2" or "1,3", one by one.
idset_ranks() {
    flux python -c \
	"import sys; from flux.idset import IDset; print(' '.join(str(i) for i in IDset(sys.argv[1])))" "$1"
}

# Usage: lost_ranks_gone lost-idset
#
# True if the scheduler's graph holds nothing on each lost rank.
lost_ranks_gone() {
    local rank
    for rank in $(idset_ranks "$1"); do
	test "$(graph_rank_count $rank)" = "0" || return 1
    done
    return 0
}

# Usage: wait_for_lost_rank lost-idset nnodes
#
# A fixed sleep either wastes time or races the shrink.  Poll the two
# observable effects of the removal instead, for at most 30 seconds.
wait_for_lost_rank() {
    local i=0
    while test $i -lt 300; do
	if test "$(rl_nnodes)" = "$2" && lost_ranks_gone "$1"; then
	    return 0
	fi
	i=$((i+1))
	sleep 0.1
    done
    log "resource list reports $(rl_nnodes) nodes;" \
	"the scheduler's graph still holds resources on ranks $1"
    return 1
}

# Usage: wait_for_idle
# Wait for housekeeping to drain and for the queue to go idle.
wait_for_idle() {
    local i=0
    flux queue idle -t 60s >/dev/null 2>&1 || true
    while test $i -lt 600; do
	if test "$(flux housekeeping list -no {id} 2>/dev/null | wc -l)" -eq 0 \
	    && test "$(flux jobs --filter=pending,running -no {id} | wc -l)" -eq 0; then
	    return 0
	fi
	i=$((i+1))
	sleep 0.1
    done
    return 1
}

# Usage: dmesg_wait PATTERN
# Wait for PATTERN to reach the ring buffer, so the log is complete.
dmesg_wait() {
    local i=0
    while test $i -lt 100; do
	flux dmesg -H 2>/dev/null | grep -q "$1" && return 0
	i=$((i+1))
	sleep 0.1
    done
    return 1
}

# Usage: count_event eventlog-file name
count_event() {
    awk -v want="$2" '$2 == want {n++} END {print n+0}' "$1"
}

if ! force_down "" 2>/dev/null; then
    log "resource.monitor-force-down unsupported, skipping test"
    exit 0
fi

WORK=$(mktemp -d "${TMPDIR:-/tmp}/t1573-work.XXXXXX")
cd $WORK

nnodes=$(flux resource list -s all -no {nnodes})
ncores=$(flux resource list -s all -no {ncores})
test $nnodes -eq 4 || die "expected 4 nodes, got $nnodes"
test $ncores -ge 8 \
	|| die "test needs >=2 cores per rank, instance has $ncores over $nnodes nodes"
test "$(flux config get --default= job-manager.housekeeping.command)" != "" \
	|| die "housekeeping is not configured; the frees would not be split"

log "Loading fluxion, losing ranks $LOST, keeping ranks $KEEP..."
flux module remove -f sched-simple
flux module load sched-fluxion-resource policy=first
flux module load sched-fluxion-qmanager
flux module stats sched-fluxion-qmanager >/dev/null
if ! flux dmesg | grep -q '+partial-ok'; then
    log "this flux-core does not offer partial release, skipping test"
    exit 0
fi

flux dmesg -C
id=$(flux submit -N$NJOB -n$NJOB -o cpu-affinity=off --requires=rank:$JOBRANKS sleep inf)
flux job wait-event -t 30 $id start >/dev/null \
	|| die "the $NJOB-rank job did not start"
test "$(flux jobs -no {ranks} $id)" = "[$JOBRANKS]" \
	|| die "the job is on $(flux jobs -no {ranks} $id), not ranks $JOBRANKS"
log "job $id runs on ranks $JOBRANKS"

# Trace the free protocol, so the test can show that the frees really
# arrived in the order of this pass, and that only the last one was final.
# Without that the rest of the checks could all pass on a sequence that
# never exercised the fix.
flux module trace -f -H sched-fluxion-qmanager sched-fluxion-resource \
	>$WORK/trace.out 2>&1 &
TRACE_PID=$!
sleep 1

log "Forcing ranks $LOST down to trigger a shrink..."
force_down $LOST
wait_for_lost_rank $LOST $((4 - $(idset_count "$LOST"))) \
	|| die "ranks $LOST were still present 30s after the forced down"
test "$(flux jobs -no {state} $id)" = "RUN" \
	|| log "NOTE: the job is $(flux jobs -no {state} $id) after the shrink"

log "Ending the job, so flux-core frees all $NJOB ranks..."
flux cancel $id >/dev/null 2>&1 || true
flux job wait-event -t 60 $id clean >/dev/null || die "the job never became clean"
wait_for_idle || die "housekeeping did not finish within 60s"
flux logger t1573-sentinel
dmesg_wait t1573-sentinel || die "the log never caught up"
sleep 1
kill $TRACE_PID 2>/dev/null || true
wait $TRACE_PID 2>/dev/null || true

flux dmesg -H >$WORK/dmesg.out
flux job eventlog $id >$WORK/eventlog.out

#  Every legitimate message has to be acknowledged, and none of them may
#  be called a protocol error.
grep -E "sched-fluxion-(resource|qmanager|feasibility)\.(err|crit|alert|emerg)" \
	$WORK/dmesg.out >$WORK/errors.out || true
if test -s $WORK/errors.out; then
    log "fluxion logged errors:"
    cat $WORK/errors.out >&2
    die "a fluxion module logged an error during the free of a shrunk job"
fi
grep -E "rank_to_root is empty|removed allocation before final .free RPC|Protocol error|can't find queue for job" \
	$WORK/dmesg.out >$WORK/known.out || true
if test -s $WORK/known.out; then
    log "the free protocol broke down:"
    cat $WORK/known.out >&2
    die "a known free-protocol error was logged"
fi

#  The housekeeping command set the order of the frees.  Make sure that the
#  qmanager received exactly that sequence, because the checks below depend
#  on it.  The whole sequence is the oracle: it holds the number of the
#  frees, their order, and the one final flag.
jobid_dec=$(flux job id --to=dec $id)
freed=$(free_sequence $WORK/trace.out $jobid_dec | tr '\n' ' ' | sed 's/ *$//')
want=$(expected_free_sequence)
log "the qmanager received these frees: $freed"
test "$freed" = "$want" || die "expected the frees [$want], got [$freed]"

#  If the lost rank is freed last, a non-final free removes the last
#  resources of the job from the graph before the final .free RPC.  The
#  qmanager has to keep the job until the final .free RPC, and say so at
#  the debug level.  If the lost rank is freed first, the partial cancel of
#  a rank with no vertices runs first, and the final free removes the last
#  resources.
if test $EARLY = yes; then
    grep -q "partial cancel removed the last resources of jobid" $WORK/dmesg.out \
	    || die "the early full removal never happened, so the fix was not tested"
    log "the qmanager kept the job after the early full removal"
else
    grep -q "partial cancel removed the last resources of jobid" $WORK/dmesg.out \
	    && die "the early full removal happened although it was not expected"
    log "the final free removed the last resources of the job"
fi

#  The job retires once.
test "$(flux jobs -no {state} $id)" = "INACTIVE" \
	|| die "the job is $(flux jobs -no {state} $id), not INACTIVE"
test "$(count_event $WORK/eventlog.out clean)" = "1" \
	|| die "the job has $(count_event $WORK/eventlog.out clean) clean events, not 1"
test "$(count_event $WORK/eventlog.out free)" = "1" \
	|| die "the job has $(count_event $WORK/eventlog.out free) free events, not 1"

#  Nothing is left allocated.
test "$(qm_running)" = "0" \
	|| die "the qmanager still counts $(qm_running) running jobs"
test "$(sched_allocated)" = "0" \
	|| die "the scheduler still reports $(sched_allocated) cores allocated"
if flux ion-resource find sched-now=allocated >$WORK/alloc.out 2>&1; then
    grep -A1 "MATCHED RESOURCES" $WORK/alloc.out | grep -q "^null$" \
	    || { cat $WORK/alloc.out >&2; die "the graph still has an allocation"; }
else
    log "NOTE: flux ion-resource is unavailable, skipping the graph check"
fi

#  The surviving ranks are usable.
log "A new $NKEEP-node job must run on ranks $KEEP..."
id2=$(flux submit -N$NKEEP -n$NKEEP -o cpu-affinity=off --requires=rank:$KEEP true)
flux job wait-event -t 30 $id2 start >/dev/null \
	|| die "a new $NKEEP-node job did not start on ranks $KEEP"
flux job wait-event -t 30 $id2 clean >/dev/null
test "$(flux jobs -no {returncode} $id2)" = "0" \
	|| die "the new job exited with $(flux jobs -no {returncode} $id2)"
test "$(flux jobs -no {ranks} $id2)" = "[$KEEP]" \
	|| die "the new job ran on $(flux jobs -no {ranks} $id2), not ranks $KEEP"

log "PASS: losing ranks $LOST left the free protocol of a $NJOB-rank job intact"
rm -rf $WORK

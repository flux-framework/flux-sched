#!/bin/bash -e
#
#  Issue #1424: under the easy backfill policy, a reserved job advertises
#  its start-time estimate via the sched.t_estimate annotation, which
#  `flux jobs` displays as an eta.  If the job later loses its reservation
#  (e.g. it is deprioritized and another job takes the single reservation
#  slot), the now-stale estimate must be cleared so `flux jobs` stops
#  displaying an incorrect eta.
#

log() { printf "issue#1424: $@\n" >&2; }

# Run in a fresh instance so we can swap schedulers freely
if test "$ISSUE_1424_ACTIVE" != "t"; then
    export ISSUE_1424_ACTIVE=t
    log "Re-launching test script under flux-start"
    exec flux start -Sbroker.module-nopanic=1 $0
fi

# `flux jobs` renders sched.t_estimate in the INFO column as "eta:<fsd>"
# (or "eta:now" once the estimated time has passed) for jobs in SCHED state.
eta() {
    flux jobs -no "{contextual_info}" $1
}

has_eta() {
    eta $1 | grep -q "eta:"
}

no_eta() {
    ! has_eta $1
}

# Annotation updates are not posted to the KVS eventlog, so poll.
wait_for() {
    local i=0
    while ! "$@"; do
        i=$((i+1))
        test $i -ge 50 && return 1
        sleep 0.2
    done
    return 0
}

log "loading fluxion with easy backfill policy"
flux module remove sched-simple
flux module load sched-fluxion-resource
flux module load sched-fluxion-qmanager queue-policy=easy

ncores=$(flux resource list -no {ncores})
log "instance has ${ncores} cores"

log "submitting job A to occupy all cores"
jobA=$(flux submit -n ${ncores} -t 600s sleep 500)
flux job wait-event -t 30 ${jobA} start

log "submitting jobs B and C, both blocked behind A"
jobB=$(flux submit -n ${ncores} -t 60s sleep 50)
jobC=$(flux submit -n ${ncores} -t 60s sleep 50)

log "waiting for B to be reserved and show an eta"
wait_for has_eta ${jobB}
log "B: '$(eta ${jobB})'"
if has_eta ${jobC}; then
    log "ERROR: C unexpectedly has an eta: '$(eta ${jobC})'"
    exit 1
fi

log "deprioritizing B so C takes the reservation"
flux job urgency ${jobB} 1

log "waiting for B's stale eta to be cleared"
wait_for no_eta ${jobB}
log "waiting for C to gain an eta"
wait_for has_eta ${jobC}
log "B: '$(eta ${jobB})' C: '$(eta ${jobC})'"

flux jobs -ao "{id.f58:>12} {urgency:<3} {status_abbrev:<2} {contextual_info}" >&2

log "cleaning up"
flux cancel ${jobA} ${jobB} ${jobC}
flux job wait-event -t 30 ${jobC} clean

flux module remove sched-fluxion-qmanager
flux module remove sched-fluxion-resource
flux module load sched-simple

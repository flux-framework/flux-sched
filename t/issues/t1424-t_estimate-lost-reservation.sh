#!/bin/bash -e
#
#  Issue #1424: under the easy backfill policy, a reserved job advertises
#  its start-time estimate via the sched.t_estimate annotation, which
#  `flux jobs` displays as an eta.  If the job later loses its reservation
#  because newly submitted higher-urgency jobs take the single reservation
#  slot, the now-stale estimate must be cleared so `flux jobs` stops
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

selection_type_absent() {
    test "$(flux job list-ids "$1" |
        jq -r '(.annotations.sched // {}) |
               has("selection_type") | not')" = "true"
}

# Annotation updates are not posted to the eventlog, so poll.
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

log "submitting squatter job to occupy the node"
squatter=$(flux submit -x -N1 -n1 -t 600s --job-name=squatter sleep inf)
flux job wait-event -t 30 ${squatter} start

log "submitting low-urgency job (demoted), blocked behind squatter"
demoted=$(flux submit --urgency=1 -x -N1 -n1 -t 60s --job-name=demoted sleep inf)

log "waiting for demoted to be reserved and show an eta"
wait_for has_eta ${demoted}
log "demoted: '$(eta ${demoted})'"

# With easy backfill the reservation depth is effectively 1, so once
# highest and higher are submitted only the highest-urgency job should
# hold the reservation and an eta; demoted's eta is now stale and must
# be cleared, and higher is skipped without ever getting an eta.
log "submitting higher-urgency jobs to displace demoted's reservation"
highest=$(flux submit --urgency=6 -x -N1 -n1 -t 60s --job-name=highest sleep inf)
higher=$(flux submit --urgency=4 -x -N1 -n1 -t 60s --job-name=higher sleep inf)

log "waiting for demoted's stale eta to be cleared"
wait_for no_eta ${demoted}
log "verifying that selection_type was cleared when stale eta was cleared"
selection_type_absent ${demoted}
log "waiting for highest to gain an eta"
wait_for has_eta ${highest}
if has_eta ${higher}; then
    log "ERROR: higher unexpectedly has an eta: '$(eta ${higher})'"
    exit 1
fi
log "demoted: '$(eta ${demoted})' highest: '$(eta ${highest})' higher: '$(eta ${higher})'"

flux jobs -ao "{id.f58:>12} {urgency:<3} {status_abbrev:<2} {contextual_info}" >&2

log "cleaning up"
flux cancel ${squatter} ${demoted} ${highest} ${higher}
flux job wait-event -t 30 ${higher} clean

flux module remove sched-fluxion-qmanager
flux module remove sched-fluxion-resource
flux module load sched-simple

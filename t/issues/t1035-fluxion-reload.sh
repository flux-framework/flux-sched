#!/bin/bash
#
#  Ensure fluxion modules can recover running jobs with rv1 match format.
#
log() { printf "issue#1035: $@\n" >&2; }
die() { log "$@"; exit 1; }
run_timeout() {
    "${PYTHON:-python3}" "${SHARNESS_TEST_SRCDIR}/scripts/run_timeout.py" "$@"
}

if test -z "$ISSUE_1035_TEST_ACTIVE"; then
    export ISSUE_1035_TEST_ACTIVE=t
    log "relaunching under test instance of size 4..."
    exec flux start -Sbroker.module-nopanic=1 -s 4 $0 "$@"
fi
test $(flux resource list -no {nnodes}) -eq 4 || die "test requires 4 nodes"

log "Unloading modules..."
flux module remove sched-simple
flux module remove resource

log "Amending instance resource set with properties: batch, debug..."
flux kvs get resource.R \
	| flux R set-property batch:0-1 debug:2-3 \
	| flux kvs put -r resource.R=-
#flux kvs get resource.R | jq

log "Loading config with queues and match-format=\"rv1\"..."
flux config load <<EOF
[queues.debug]
requires = ["debug"]

[queues.batch]
requires = ["batch"]

[sched-fluxion-resource]
match-format = "rv1"
EOF
flux config get | \
	jq -e ".\"sched-fluxion-resource\".\"match-format\" == \"rv1\"" \
	|| die "failed to set sched-fluxion-resource.match-format = rv1"

log "Reloading modules..."
flux module load resource noverify
flux module load sched-fluxion-resource
flux module load sched-fluxion-qmanager
flux dmesg -HL | grep version | tail -2

log "Starting all queues..."
flux queue start --all --quiet
flux queue status
flux resource list -s free

log "Submitting two sleep jobs..."
run_timeout 10 flux submit -N2 --wait-event=start --queue=debug sleep inf
run_timeout 10 flux submit -N2 --wait-event=start --queue=batch sleep inf

log "Reloading fluxion..."
flux module unload sched-fluxion-qmanager
flux module reload sched-fluxion-resource
flux module load sched-fluxion-qmanager

log "Checking that running jobs were recovered..."
flux jobs -ano "{id.f58:>12} {status_abbrev:>2} {name}"
test $(flux jobs -no {id} | wc -l) -eq 2 \
	|| die "Expected 2 jobs still running"
flux cancel --all


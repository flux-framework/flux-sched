#!/bin/bash
#
#  Ensure fluxion calc_factor executes without overflow
#

log() { printf "issue#1129: $@\n" >&2; }

TEST_SIZE=${TEST_SIZE:-900}

log "Unloading modules..."
flux module remove sched-simple
flux module remove resource

flux config load <<EOF
[sched-fluxion-qmanager]
queue-policy = "easy"

[resource]
noverify = true
norestrict = true

[[resource.config]]
hosts = "test[1-${TEST_SIZE}]"
cores = "0-112"
gpus = "0-8"
EOF

flux module load resource monitor-force-up
flux module load sched-fluxion-resource
flux module load sched-fluxion-qmanager
flux queue start --all --quiet
flux resource list
flux resource status

log "Running test job."
flux run -vvv -N${TEST_SIZE} -n${TEST_SIZE} \
	--setattr=exec.test.run_duration=1ms \
	true

log "reloading sched-simple..."
flux module remove sched-fluxion-qmanager
flux module remove sched-fluxion-resource
flux module load sched-simple

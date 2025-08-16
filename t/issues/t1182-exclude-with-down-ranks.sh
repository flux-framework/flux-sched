#!/bin/bash -e
#
#  Ensure Fluxion marks all ranks down even if some ranks are excluded
#

log() { printf "issue#1182: $@\n" >&2; }

# Need a few ranks for this test, so start a new instance of size=4
if test "$ISSUE_1182_ACTIVE" != "t"; then
    export ISSUE_1182_ACTIVE=t
    log "Re-launching test script under flux-start"
    exec flux start -Sbroker.module-nopanic=1 -s 4 $0
fi

cat <<'EOF' >rcheck.py
import sys
import flux
from flux.resource.list import ResourceListRPC

h = flux.Flux()

rpc1 = ResourceListRPC(h, "resource.sched-status", nodeid=0)
rpc2 = ResourceListRPC(h, "sched.resource-status", nodeid=0)

rset = rpc1.get()
fluxion = rpc2.get()

def symmetric_diff(a, b):
    return (a|b) - (a&b)

diff = symmetric_diff(rset.down, fluxion.down)
if diff.ranks:
    print("difference detected between fluxion and core down ranks:")
    print(f"hosts: {diff.nodelist}")
    print(f"ranks: {diff.ranks}")
    sys.exit(1)
sys.exit(0)
EOF

log "Unloading modules..."
flux module remove sched-simple
flux module remove resource

# Exclude rank 0
flux config load <<EOF
[resource]
exclude = "0,2"
EOF

flux module load resource monitor-force-up

# Drain rank 3. Scheduler should only see rank 1 as up
log "draining rank 3"
flux resource drain 3

flux resource status

flux module load sched-fluxion-resource
flux module load sched-fluxion-qmanager

log "comparing fluxion down ranks with flux-core resource module:"
flux resource list
FLUX_RESOURCE_LIST_RPC=sched.resource-status flux resource list
flux python ./rcheck.py

log "reloading sched-simple..."
flux module remove sched-fluxion-qmanager
flux module remove sched-fluxion-resource
flux module load sched-simple

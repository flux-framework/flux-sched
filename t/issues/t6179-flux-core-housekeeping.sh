#!/bin/bash
#
#  Ensure fluxion conforms to updated RFC 27 and behaves as desired if resource state
#  discrepancies exit between flux-core and -sched.
#

log() { printf "flux-coreissue#6179: $@\n" >&2; }

cat <<'EOF' >free.py
import flux
import json
import sys
import subprocess as sp
from flux.job import JobID

jobid = JobID(sys.argv[1])
r_obj=json.loads(sp.check_output(['flux', 'job', 'info', sys.argv[1], 'R']).decode())
obj = {'id': jobid, 'R': r_obj, 'final': True}
flux.Flux().rpc('sched.free', obj)
sys.exit(0)
EOF

cat <<'EOF' >incomplete-free.py
import flux
import json
import sys
import subprocess as sp
from flux.job import JobID

jobid = JobID(sys.argv[1])
R_str = '{"version": 1, "execution": {"R_lite": [{"rank": "1", "children": {"core": "0-15", "gpu": "0-3"}}], "nodelist": ["node1"]}}'
r_obj = json.loads(R_str)
obj = {'id': jobid, 'R': r_obj, 'final': True}
flux.Flux().rpc('sched.free', obj)
sys.exit(0)
EOF

cat <<EOF >flux.config
[sched-fluxion-resource]
match-policy = "lonodex"
match-format = "rv1_nosched"

[resource]
noverify = true
norestrict = true

[[resource.config]]
hosts = "node[0-1]"
cores = "0-15"
gpus = "0-3"
EOF

log "Unloading modules..."
flux module remove sched-simple
flux module remove resource

flux config load flux.config

flux module load resource monitor-force-up
flux module load sched-fluxion-resource
flux module load sched-fluxion-qmanager queue-policy="easy"
flux queue start --all --quiet
flux resource list
flux resource status
flux module list

log "Running test job 1"
jobid1=$(flux submit --wait-event=alloc -N2 -t 1h --setattr=exec.test.run_duration=1m sleep inf)
log "Sending final RPC for job 1"
flux python ./free.py ${jobid1}
state=$( flux ion-resource find "sched-now=allocated" )
if [[ ${state} != *"null"* ]]; then
    # retry since ./free.py isn't blocking
    state=$( flux ion-resource find "sched-now=allocated" )
    if [[ ${state} != *"null"* ]]; then
        # .free didn't release all resources
        exit 1
    fi
fi

# Need to execute cancel to remove from job manager
flux cancel ${jobid1}
flux job wait-event -t 5 ${jobid1} release

log "Running test job 2"
jobid2=$(flux submit --wait-event=alloc -N2 -t 1h --setattr=exec.test.run_duration=1m sleep inf)
log "Sending final RPC for job 2"
flux python ./incomplete-free.py ${jobid2}
state=$( flux ion-resource find "sched-now=allocated" )
if [[ ${state} != *"null"* ]]; then
    # retry since ./free.py isn't blocking
    state=$( flux ion-resource find "sched-now=allocated" )
    if [[ ${state} != *"null"* ]]; then
        # .free didn't release all resources
        exit 1
    fi
fi

# Need to execute cancel to remove from job manager
flux cancel ${jobid2}
flux job wait-event -t 5 ${jobid2} release
flux jobs -a

log "reloading sched-simple..."
flux module remove sched-fluxion-qmanager
flux module remove sched-fluxion-resource
flux module load sched-simple

log "Unloading modules for FCFS test..."
flux module remove sched-simple
flux module remove resource

flux config load flux.config

flux module load resource monitor-force-up
flux module load sched-fluxion-resource
flux module load sched-fluxion-qmanager queue-policy="fcfs"
flux queue start --all --quiet
flux resource list
flux resource status
flux module list

log "Running test job 3"
jobid3=$(flux submit --wait-event=alloc -N2 -t 1h --setattr=exec.test.run_duration=1m sleep inf)
log "Sending final RPC for job 3"
flux python ./free.py ${jobid3}
state=$( flux ion-resource find "sched-now=allocated" )
if [[ ${state} != *"null"* ]]; then
    # retry since ./free.py isn't blocking
    state=$( flux ion-resource find "sched-now=allocated" )
    if [[ ${state} != *"null"* ]]; then
        # .free didn't release all resources
        exit 1
    fi
fi

# Need to execute cancel to remove from job manager
flux cancel ${jobid3}
flux job wait-event -t 5 ${jobid3} release

log "Running test job 4"
jobid4=$(flux submit --wait-event=alloc -N2 -t 1h --setattr=exec.test.run_duration=1m sleep inf)
log "Sending final RPC for job 4"
flux python ./incomplete-free.py ${jobid4}
state=$( flux ion-resource find "sched-now=allocated" )
if [[ ${state} != *"null"* ]]; then
    # retry since ./free.py isn't blocking
    state=$( flux ion-resource find "sched-now=allocated" )
    if [[ ${state} != *"null"* ]]; then
        # .free didn't release all resources
        exit 1
    fi
fi

# Need to execute cancel to remove from job manager
flux cancel ${jobid4}
flux job wait-event -t 5 ${jobid4} release
flux jobs -a

log "reloading sched-simple..."
flux module remove sched-fluxion-qmanager
flux module remove sched-fluxion-resource
flux module load sched-simple


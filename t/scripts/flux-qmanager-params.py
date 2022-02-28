#!/bin/false
#
#  Run script as `flux qmanager-params` with properly configured
#   FLUX_EXEC_PATH or `flux python flux-qmanager-params` if not to
#   avoid python version mismatch
#
import json
import flux

print(json.dumps(flux.Flux().rpc("sched-fluxion-qmanager.params").get()))

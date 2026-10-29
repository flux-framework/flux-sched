#!/bin/sh

test_description='Test a job that takes a core and a device added at runtime

A device the graph did not ship with is added under a rack with the
add-subgraph RPC, as a site would add one to a running instance. A small
jobspec then asks for a node with a core and, beside it, that device. The
request goes through the job path, from submit to qmanager to the resource
module, and the allocation has to contain the device. The device is
requested exclusive, since a shared leaf is matched but left out of R.
'

. `dirname $0`/sharness.sh

if test_have_prereq ASAN; then
	skip_all='skipping coschedule device test under AddressSanitizer'
	test_done
fi

SIZE=1
test_under_flux ${SIZE}

base_jgf="${SHARNESS_TEST_SRCDIR}/data/resource/jgfs/issue1284.json"

test_expect_success 'load the full stack against the base graph' '
	flux module remove sched-simple &&
	flux module remove resource &&
	flux config load <<-EOF2 &&
	[resource]
	noverify = true
	norestrict = true
	path = "${base_jgf}"
	EOF2
	flux module load resource monitor-force-up &&
	flux module load sched-fluxion-resource match-format=rv1 &&
	flux module load sched-fluxion-qmanager &&
	flux module unload job-list &&
	flux queue start --all --quiet
'

# The subgraph carries the rack it hangs from, copied as is, so the reader
# matches the rack on its path and attaches the new vertices under it
test_expect_success 'write a device subgraph under rack0' '
	flux python - ${base_jgf} > qdevice.json <<-EOF2
	import json, sys
	g = json.load(open(sys.argv[1]))["scheduling"]["graph"]
	rack = next(n for n in g["nodes"] if n["metadata"]["paths"]["containment"] == "/compute0/rack0")
	nid = max(int(n["id"]) for n in g["nodes"]) + 1
	def vertex(vid, vtype, path, props):
	    return {"id": str(vid), "metadata": {"type": vtype, "basename": vtype,
	            "name": vtype + "0", "id": 0, "uniq_id": vid, "rank": -1,
	            "exclusive": False, "unit": "", "size": 1, "properties": props,
	            "paths": {"containment": path}}}
	def edge(src, dst):
	    return {"source": str(src), "target": str(dst), "directed": True,
	            "metadata": {"subsystem": "containment"}}
	dev = vertex(nid, "qdevice_ibm", "/compute0/rack0/qdevice_ibm0", {"ibm": ""})
	qpu = vertex(nid + 1, "qpu", "/compute0/rack0/qdevice_ibm0/qpu0", {"ibm": ""})
	print(json.dumps({"graph": {"nodes": [rack, dev, qpu],
	                            "edges": [edge(rack["id"], nid), edge(nid, nid + 1)]}}))
	EOF2
'

test_expect_success 'add the device to the running graph' '
	flux ion-resource add-subgraph qdevice.json &&
	flux ion-resource find --format=jgf status=up > graph.out &&
	tail -1 graph.out | jq -e "[.graph.nodes[].metadata.type] | any(. == \"qpu\")"
'

test_expect_success 'write a jobspec for a core and an exclusive qpu' '
	cat >scout.json <<-EOF2
	{"version":1,"resources":[{"type":"node","count":1,"with":[{"type":"slot","count":1,"label":"scout","with":[{"type":"core","count":1}]}]},{"type":"qdevice_ibm","count":1,"with":[{"type":"qpu","count":1,"exclusive":true}]}],"attributes":{"system":{"duration":60}},"tasks":[{"command":["true"],"slot":"scout","count":{"per_slot":1}}]}
	EOF2
'

test_expect_success 'the job is allocated a core and a qpu together' '
	jobid=$(flux job submit --flags=waitable scout.json) &&
	flux job wait-event -vt10 ${jobid} alloc &&
	flux job wait-event -vt10 ${jobid} clean
'

test_expect_success 'the allocation contains the qpu' '
	jobid=$(flux job submit --flags=waitable scout.json) &&
	flux job wait-event -vt10 ${jobid} alloc &&
	flux job info ${jobid} R >R.out &&
	jq -e "[.scheduling.graph.nodes[]?.metadata.type] | any(. == \"qpu\")" R.out &&
	flux job wait-event -vt10 ${jobid} clean
'

test_expect_success 'a request for more qpus than exist is unsatisfiable' '
	cat >nope.json <<-EOF2 &&
	{"version":1,"resources":[{"type":"node","count":1,"with":[{"type":"slot","count":1,"label":"scout","with":[{"type":"core","count":1}]}]},{"type":"qdevice_ibm","count":1,"with":[{"type":"qpu","count":99,"exclusive":true}]}],"attributes":{"system":{"duration":60}},"tasks":[{"command":["true"],"slot":"scout","count":{"per_slot":1}}]}
	EOF2
	jobid=$(flux job submit nope.json) &&
	flux job wait-event -vt10 ${jobid} exception >ev.out 2>&1 &&
	grep -qi "unsatisf" ev.out
'

test_expect_success 'cleanup' '
	flux module remove -f sched-fluxion-qmanager &&
	flux module remove -f sched-fluxion-resource
'

test_done

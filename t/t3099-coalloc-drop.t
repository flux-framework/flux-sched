#!/bin/sh

test_description='match allocate silently drops a co-requested sibling device

When one jobspec co-requests compute (node -> core) AND a device that lives in a
sibling subtree under the same parent (ssd, a child of rack alongside node),
"resource-query match allocate" reports RESOURCES=ALLOCATED but the emitted R
does NOT contain the ssd -- it is silently dropped. The ssd allocates fine on
its own (shown below), so this is a genuine drop, not an unsupported type.

Graph: see issue 1284 resource set, cluster -> rack -> [ ssd , node -> core ].
'

. $(dirname $0)/sharness.sh

query="../../resource/utilities/resource-query"
rv1="${SHARNESS_TEST_SRCDIR}/data/resource/jgfs/issue1284.json"

test_expect_success 'extract the issue1284 scheduling graph as JGF' '
	jq "{graph: .scheduling.graph}" ${rv1} > g.jgf &&
	test -s g.jgf
'

test_expect_success 'write jobspecs (node+ssd co-request, and ssd alone)' '
	cat >core_ssd.json <<-EOF2 &&
	{"version":1,"resources":[{"type":"cluster","count":1,"with":[{"type":"rack","count":1,"with":[{"type":"node","count":1,"with":[{"type":"slot","count":1,"label":"s","with":[{"type":"core","count":1}]}]},{"type":"ssd","count":1}]}]}],"attributes":{"system":{"duration":60}},"tasks":[{"command":["x"],"slot":"s","count":{"per_slot":1}}]}
	EOF2
	cat >ssd.json <<-EOF2
	{"version":1,"resources":[{"type":"cluster","count":1,"with":[{"type":"rack","count":1,"with":[{"type":"slot","count":1,"label":"s","with":[{"type":"ssd","count":1}]}]}]}],"attributes":{"system":{"duration":60}},"tasks":[{"command":["x"],"slot":"s","count":{"per_slot":1}}]}
	EOF2
'

test_expect_success 'control: the ssd allocates on its own (R contains ssd)' '
	printf "match allocate ssd.json\nquit\n" | ${query} -L g.jgf -f jgf -F jgf > ssd.out 2>&1 &&
	grep -q "RESOURCES=ALLOCATED" ssd.out &&
	grep -q "\"type\": \"ssd\"" ssd.out
'

test_expect_success 'the node+ssd co-request reports ALLOCATED' '
	printf "match allocate core_ssd.json\nquit\n" | ${query} -L g.jgf -f jgf -F jgf > core_ssd.out 2>&1 &&
	grep -q "RESOURCES=ALLOCATED" core_ssd.out &&
	grep -q "\"type\": \"core\"" core_ssd.out
'

test_expect_success 'BUG: the co-requested ssd is silently dropped from the allocation' '
	test_must_fail grep -q "\"type\": \"ssd\"" core_ssd.out
'

test_done

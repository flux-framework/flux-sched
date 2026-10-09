#!/bin/sh

test_description='Test the resource counts after a partial release

These tests are for flux-framework/flux-sched#1560 and #1561. In #1560, a
shrink by rank subtracted from each job the counts of all jobs on a lost
rank, and the counts of all lost ranks. In #1561, a partial release below
an exclusive vertex that has no broker rank, for example a rack, did not
make that vertex non-exclusive.
'

. $(dirname $0)/sharness.sh

query="../../resource/utilities/resource-query"
jgf="${SHARNESS_TEST_SRCDIR}/data/resource/jgfs/elastic/tiny-partial-cancel.json"
jgf_2racks="${SHARNESS_TEST_SRCDIR}/data/resource/jgfs/elastic/two-racks.json"
rv1_3n="${SHARNESS_TEST_SRCDIR}/data/resource/rv1exec/three_nodes.json"
pc_dir="${SHARNESS_TEST_SRCDIR}/data/resource/rv1exec/cancel"
js_dir="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/partial_release"
core1="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/basics/test008.yaml"
excl_rack2="${js_dir}/excl-rack-2nodes.yaml"
excl_cluster2="${js_dir}/excl-cluster-2nodes.yaml"
node2x1="${js_dir}/node-2nodes-1core.yaml"
slot3_long="${js_dir}/slot-3cores-long.yaml"
node2x3="${js_dir}/node-2nodes-3cores.yaml"

excl_rack1="${js_dir}/excl-rack-1node.yaml"
node1="${js_dir}/node-1core.yaml"
excl_cluster="${js_dir}/excl-cluster.yaml"
# The rv1exec partial cancel reads only the ranks of a fragment
free0="${pc_dir}/rank0_cancel-jgfgraph.json"
free1="${pc_dir}/rank1_cancel.json"
free2="${pc_dir}/rank2_cancel.json"
free3="${pc_dir}/rank3_cancel.json"

# The graph of the two racks has no socket, so its jobspecs put a core
# below the slot. The cluster "tiny0" has rack0 with node0 (rank 0),
# node1 (rank 1) and node2 (rank 2), and rack1 with node3 (rank 3). Each
# node has two cores, and each rack has the rank -1.
excl_2racks="${js_dir}/excl-rack2-1node.yaml"
excl_1rack="${js_dir}/excl-rack1-1node.yaml"
node1_flat="${js_dir}/node-1core-nosocket.yaml"
# The cluster prunes on the rack also, so the cluster keeps a per-job
# count of the racks of the job
pf_rack="ALL:core,ALL:node,cluster:rack"

# Show the R_lite ranks of each R from resource-query, one R on each line
ranks_of () {
	grep "^{" "$1" | jq -r ".execution.R_lite | map(.rank) | join(\",\")"
}

# Remove the output of the first match or update command. Then the
# remaining output of two command sequences is comparable.
after_first_alloc () {
	sed "1,/SCHEDULED AT/d" "$1" | sed 1d
}

# Show the graph of one "find" result. $2 is the expression of the find
# and $3 is its occurrence in the file, because a test runs the same find
# before and after a shrink. A find emits its graph, then a separator, and
# then its INFO banner, so the graph is the second line above the banner.
# A find that matches no vertex emits no graph. The second line above the
# banner is then the last line of the command before, which is never a
# JSON object, so the result is empty.
find_jgf () {
	awk -v expr="INFO: EXPRESSION=\"$2\"" -v want="$3" '
		/^INFO: EXPRESSION=/ && $0 == expr && ++seen == want {
			if (above2 ~ /^\{/ && above1 ~ /^INFO: ==/)
				print above2
		}
		{ above2 = above1; above1 = $0 }' "$1"
}

# Show the aggregate filter counts of one "find" result, one line for each
# vertex and resource type. The arguments are those of find_jgf ().
agfilter_of () {
	find_jgf "$1" "$2" "$3" \
	| jq -r '.graph.nodes[] | select (.metadata.agfilter != null)
		| .metadata.paths.containment as $p
		| .metadata.agfilter | to_entries[]
		| "\($p) \(.key) \(.value)"' \
	| LC_ALL=C sort
}

# Remove the total of each line. The total of an ancestor decreases when a
# shrink removes a node below it, but the count of a job must not change.
used_only () {
	sed "s/, total:.*//" "$1"
}

#
# Issue #1560: rank-based shrink (resource-query "remove <idset> false")
#

test_expect_success 'a shrink of a rank that two jobs share causes no error' '
	cat >cmds001 <<-EOF &&
	match allocate ${core1}
	match allocate ${core1}
	remove 0 false
	match allocate ${core1}
	quit
	EOF
	${query} -f jgf -L ${jgf} -S CA -P lonode -F rv1_nosched \
		<cmds001 >001.out 2>&1 &&
	test_debug "cat 001.out" &&
	test_must_fail grep ERROR 001.out &&
	ranks_of 001.out >001.ranks &&
	printf "0\n0\n1\n" >001.expected &&
	test_cmp 001.expected 001.ranks
'

test_expect_success 'a shrink of two ranks decreases each job only by its own rank' '
	cat >cmds002 <<-EOF &&
	match allocate ${core1}
	match allocate ${core1}
	match allocate ${core1}
	remove 0-1 false
	match allocate ${core1}
	quit
	EOF
	${query} -f rv1exec -L ${rv1_3n} -S CA -P low -F rv1_nosched \
		<cmds002 >002.out 2>&1 &&
	test_debug "cat 002.out" &&
	test_must_fail grep ERROR 002.out &&
	ranks_of 002.out >002.ranks &&
	printf "0\n1\n2\n2\n" >002.expected &&
	test_cmp 002.expected 002.ranks
'

test_expect_success 'a shrink below an exclusive allocation and a reservation causes no error' '
	cat >cmds003 <<-EOF &&
	match allocate ${excl_cluster}
	match allocate_orelse_reserve ${excl_cluster}
	remove 0 false
	cancel 2
	cancel 1
	match allocate ${excl_cluster}
	quit
	EOF
	${query} -f rv1exec -L ${rv1_3n} -S CA -P lonode -F rv1_nosched \
		<cmds003 >003.out 2>&1 &&
	test_debug "cat 003.out" &&
	test_must_fail grep ERROR 003.out &&
	ranks_of 003.out >003.ranks &&
	printf "0-2\n0-2\n1-2\n" >003.expected &&
	test_cmp 003.expected 003.ranks
'

# Multi-tenancy: two jobs share rank 0, and a third job holds a
# reservation on the cluster. The shrink must decrease each job by its
# own counts only, and must count the reservation.
test_expect_success 'a shrink of a shared rank below a reservation causes no error' '
	cat >cmds004 <<-EOF &&
	match allocate ${core1}
	match allocate ${core1}
	match allocate_orelse_reserve ${excl_cluster}
	remove 0 false
	cancel 3
	cancel 1
	cancel 2
	match allocate ${excl_cluster}
	quit
	EOF
	${query} -f rv1exec -L ${rv1_3n} -S CA -P lonode -F rv1_nosched \
		<cmds004 >004.out 2>&1 &&
	test_debug "cat 004.out" &&
	test_must_fail grep ERROR 004.out &&
	ranks_of 004.out >004.ranks &&
	printf "0\n0\n0-2\n1-2\n" >004.expected &&
	test_cmp 004.expected 004.ranks
'

test_expect_success 'a second shrink of the same rank causes no error' '
	cat >cmds005 <<-EOF &&
	match allocate ${core1}
	remove 0 false
	remove 0 false
	match allocate ${core1}
	quit
	EOF
	${query} -f jgf -L ${jgf} -S CA -P lonode -F rv1_nosched \
		<cmds005 >005.out 2>&1 &&
	test_debug "cat 005.out" &&
	test_must_fail grep ERROR 005.out &&
	ranks_of 005.out >005.ranks &&
	printf "0\n1\n" >005.expected &&
	test_cmp 005.expected 005.ranks
'


# "remove <idset> false" runs a partial cancel by rank and then a
# structural removal. remove_subgraph () ignores a rank that it does not
# know, so the partial cancel must ignore it also. Otherwise an unknown
# rank skips the whole accounting cleanup, and the shrink still removes
# the known rank. The ancestors then keep the counts of a job that holds
# nothing. The test compares the two commands state by state.
test_expect_success 'run a shrink of a known rank and of a known plus an unknown rank' '
	cat >cmds020 <<-EOF &&
	match allocate ${core1}
	remove 0 false
	find jobid-span=1 and agfilter=true
	find jobid-tag=1
	find sched-now=allocated
	match allocate ${core1}
	quit
	EOF
	sed "s/^remove 0 false/remove 0,999 false/" cmds020 >cmds021 &&
	${query} -f jgf -L ${jgf} -S CA -P lonode -F jgf \
		<cmds020 >020.out 2>&1 &&
	${query} -f jgf -L ${jgf} -S CA -P lonode -F jgf \
		<cmds021 >021.out 2>&1 &&
	test_debug "cat 021.out"
'

# The cleanup of rank 0 succeeded, so the unknown rank 999 is a report of
# the caller, not a failure of the cleanup.
test_expect_success 'an unknown rank in a shrink gives a warning, not an error' '
	test_must_fail grep ERROR 021.out &&
	test_must_fail grep WARNING 020.out &&
	grep "WARNING.*999.*by_rank" 021.out
'

# Job 1 held one core on rank 0 only. After the shrink, no ancestor keeps
# a span of job 1. A skipped cleanup leaves "core used:1" on the rack and
# on the cluster.
test_expect_success 'an unknown rank in a shrink cleans the known ranks' '
	agfilter_of 020.out "jobid-span=1 and agfilter=true" 1 >020.span1 &&
	agfilter_of 021.out "jobid-span=1 and agfilter=true" 1 >021.span1 &&
	test_must_fail test -s 020.span1 &&
	test_cmp 020.span1 021.span1
'

# The rest of the state must be the same also: the tags of job 1, the
# allocated vertices, and the resources of the next job.
test_expect_success 'an unknown rank in a shrink changes nothing else' '
	grep -v "^WARNING" 021.out >021.state &&
	test_cmp 020.out 021.state
'

# The leak of the previous test costs real capacity: the rack keeps one
# core of job 1 in use, so only 35 of the 36 cores of the surviving node
# are schedulable while job 1 lives.
test_expect_success 'an unknown rank in a shrink keeps the capacity' '
	{ echo "match allocate ${core1}" &&
	  echo "remove 0,999 false" &&
	  i=0 &&
	  while test ${i} -lt 36; do
		echo "match allocate ${core1}" || return 1
		i=$((i + 1))
	  done &&
	  echo "quit"; } >cmds022 &&
	${query} -f jgf -L ${jgf} -S CA -P lonode -F rv1_nosched \
		<cmds022 >022.out 2>&1 &&
	test_debug "cat 022.out" &&
	test_must_fail grep ERROR 022.out &&
	test_must_fail grep "No matching resources found" 022.out
'

# Before the fix, the removal of the last child of a vertex made
# remove_metadata_outedges () loop without end.
test_expect_success 'a shrink of all ranks below a rack completes' '
	cat >cmds006 <<-EOF &&
	match allocate ${core1}
	remove 0-1 false
	match allocate ${core1}
	quit
	EOF
	${query} -f jgf -L ${jgf} -S CA -P lonode -F rv1_nosched \
		<cmds006 >006.out 2>&1 &&
	test_debug "cat 006.out" &&
	test_must_fail grep ERROR 006.out &&
	test $(grep -c "No matching resources found" 006.out) -eq 1 &&
	ranks_of 006.out >006.ranks &&
	printf "0\n" >006.expected &&
	test_cmp 006.expected 006.ranks
'

test_expect_success 'a subgraph removal of a whole rack completes' '
	cat >cmds007 <<-EOF &&
	remove /tiny0/rack0 true
	match allocate ${core1}
	quit
	EOF
	${query} -f jgf -L ${jgf} -S CA -P lonode -F rv1_nosched \
		<cmds007 >007.out 2>&1 &&
	test_debug "cat 007.out" &&
	test_must_fail grep ERROR 007.out &&
	grep "No matching resources found" 007.out
'

# flux-core frees each node of a job also if a shrink removed the node.
# The partial cancel of a rank that has no vertices must cause no error,
# and must find the complete cancel if the job holds nothing else.
test_expect_success 'a partial cancel of a rank that a shrink removed completes the job' '
	cat >cmds008 <<-EOF &&
	match allocate ${excl_rack2}
	remove 0-1 false
	partial-cancel 1 rv1exec ${free0}
	info 1
	find jobid-tag=1
	quit
	EOF
	${query} -f jgf -L ${jgf} -S CA -P lonode -F rv1_nosched \
		<cmds008 >008.out 2>&1 &&
	test_debug "cat 008.out" &&
	test_must_fail grep ERROR 008.out &&
	grep "^INFO: 1, CANCELED" 008.out &&
	ranks_of 008.out >008.ranks &&
	printf "0-1\n" >008.expected &&
	test_cmp 008.expected 008.ranks
'

#
# Issue #1560: exact counts of each job after a shrink of a shared rank
#

# Four jobs with unequal shares of the lost rank 0:
#   job 1: 1 core on rank 0 only,
#   job 2: 1 core on rank 0 and 1 core on rank 1,
#   job 3: 3 cores on rank 1 only, and a longer duration,
#   job 4: a reservation of 3 cores on rank 0 and 3 cores on rank 2.
# The shrink of rank 0 must decrease the aggregate span of each job by the
# resources of that job on rank 0 only. Job 1 must lose its spans, job 3
# must not change, and job 4 must keep its part on rank 2. A pooled
# reduction gives a different count for job 2 and job 4, or a warning from
# the upper bound of the reduction. The input is valid, so neither an
# error nor a warning is correct.
test_expect_success 'a shrink decreases each job by its own counts only' '
	cat >cmds023 <<-EOF &&
	match allocate ${core1}
	match allocate ${node2x1}
	match allocate ${slot3_long}
	match allocate_orelse_reserve ${node2x3}
	find jobid-span=1 and agfilter=true
	find jobid-span=2 and agfilter=true
	find jobid-span=3 and agfilter=true
	find jobid-span=4 and agfilter=true
	remove 0 false
	find jobid-span=1 and agfilter=true
	find jobid-span=2 and agfilter=true
	find jobid-span=3 and agfilter=true
	find jobid-span=4 and agfilter=true
	quit
	EOF
	${query} -f rv1exec -L ${rv1_3n} -S CA -P lonode -F jgf \
		<cmds023 >023.out 2>&1 &&
	test_debug "cat 023.out" &&
	test_must_fail grep -E "ERROR|WARNING" 023.out &&
	grep "^INFO: RESOURCES=RESERVED" 023.out &&
	grep "^INFO: SCHEDULED AT=3600" 023.out
'

test_expect_success 'the counts of each job before the shrink are exact' '
	for id in 1 2 3 4; do
		agfilter_of 023.out "jobid-span=${id} and agfilter=true" 1 \
			>023.before.${id} || return 1
	done &&
	cat >023.before.1.exp <<-EOF &&
	/cluster0 core used:1, total:12
	/cluster0 node used:0, total:3
	/cluster0/node0 core used:1, total:4
	EOF
	cat >023.before.2.exp <<-EOF &&
	/cluster0 core used:2, total:12
	/cluster0 node used:0, total:3
	/cluster0/node0 core used:1, total:4
	/cluster0/node1 core used:1, total:4
	EOF
	cat >023.before.3.exp <<-EOF &&
	/cluster0 core used:3, total:12
	/cluster0 node used:0, total:3
	/cluster0/node1 core used:3, total:4
	EOF
	cat >023.before.4.exp <<-EOF &&
	/cluster0 core used:6, total:12
	/cluster0 node used:0, total:3
	/cluster0/node0 core used:3, total:4
	/cluster0/node2 core used:3, total:4
	EOF
	for id in 1 2 3 4; do
		test_cmp 023.before.${id}.exp 023.before.${id} || return 1
	done
'

test_expect_success 'the counts of each job after the shrink are exact' '
	for id in 1 2 3 4; do
		agfilter_of 023.out "jobid-span=${id} and agfilter=true" 2 \
			>023.after.${id} || return 1
	done &&
	>023.after.1.exp &&
	cat >023.after.2.exp <<-EOF &&
	/cluster0 core used:1, total:8
	/cluster0 node used:0, total:2
	/cluster0/node1 core used:1, total:4
	EOF
	cat >023.after.3.exp <<-EOF &&
	/cluster0 core used:3, total:8
	/cluster0 node used:0, total:2
	/cluster0/node1 core used:3, total:4
	EOF
	cat >023.after.4.exp <<-EOF &&
	/cluster0 core used:3, total:8
	/cluster0 node used:0, total:2
	/cluster0/node2 core used:3, total:4
	EOF
	for id in 1 2 3 4; do
		test_cmp 023.after.${id}.exp 023.after.${id} || return 1
	done
'

# Job 3 holds nothing on the lost rank. Its counts must stay the same.
# Only the totals of the cluster decrease, because the cluster lost a node.
test_expect_success 'a shrink does not change a job outside the lost rank' '
	used_only 023.before.3 >023.before.3.used &&
	used_only 023.after.3 >023.after.3.used &&
	test_cmp 023.before.3.used 023.after.3.used
'

#
# Multi-tenancy: R-based partial cancel (resource-query "partial-cancel")
#

# Two jobs share node 0. The partial release of job 1 on rank 0 must not
# change the resources of job 2, must complete the cancel of job 1, and
# must make the core of job 1 available to a third job.
test_expect_success 'a partial cancel of one job on a shared node does not change the other job' '
	cat >cmds_mt_before <<-EOF &&
	match allocate ${core1}
	match allocate ${core1}
	find jobid-alloc=2
	quit
	EOF
	cat >cmds_mt_after <<-EOF &&
	match allocate ${core1}
	match allocate ${core1}
	partial-cancel 1 rv1exec ${free0}
	find jobid-alloc=2
	find jobid-alloc=1
	info 1
	match allocate ${core1}
	quit
	EOF
	${query} -f jgf -L ${jgf} -S CA -P lonode -F rv1_nosched \
		<cmds_mt_before >mt_before.out 2>&1 &&
	${query} -f jgf -L ${jgf} -S CA -P lonode -F rv1_nosched \
		<cmds_mt_after >mt_after.out 2>&1 &&
	test_debug "cat mt_after.out" &&
	test_must_fail grep ERROR mt_after.out &&
	grep "^{" mt_before.out | sed -n 3p >mt_before.job2 &&
	grep "^{" mt_after.out | sed -n 3p >mt_after.job2 &&
	test -s mt_before.job2 &&
	test_cmp mt_before.job2 mt_after.job2 &&
	grep "^INFO: 1, CANCELED" mt_after.out &&
	ranks_of mt_after.out >mt.ranks &&
	printf "0\n0\n0\n0\n" >mt.expected &&
	test_cmp mt.expected mt.ranks
'

#
# Issue #1561: partial release below an exclusive rack (rank -1)
#

cat >cmds_pc <<-EOF
match allocate ${excl_rack2}
partial-cancel 1 rv1exec ${free0}
find jobid-alloc=1
find jobid-tag=1
find jobid-span=1
find sched-now=allocated
match allocate ${excl_rack1}
match allocate ${node1}
partial-cancel 1 rv1exec ${free1}
info 1
find jobid-tag=1
quit
EOF

# Checks for a command sequence in which job 1 holds the rack exclusively
# and a partial release removed node 0 (rank 0) from job 1. The scheduler
# must not accept an exclusive rack request. A node job must start on
# node 0. The release of node 1 must then complete the release of job 1.
check_released_rack () {
	test_must_fail grep ERROR $1 &&
	test $(grep -c "No matching resources found" $1) -eq 1 &&
	ranks_of $1 | tail -1 >$1.last &&
	echo 0 >$1.expected &&
	test_cmp $1.expected $1.last &&
	grep "^INFO: 1, CANCELED" $1
}

for pf in "ALL:core,ALL:node" "ALL:core,ALL:node,cluster:rack"; do
	test_expect_success "a partial release below an exclusive rack frees the node ($pf)" '
		${query} -f jgf -L ${jgf} -S CA -P lonode -F rv1_nosched \
			--prune-filters=$pf <cmds_pc >pc.$pf.out 2>&1 &&
		test_debug "cat pc.$pf.out" &&
		check_released_rack pc.$pf.out
	'
done

# The job holds the cluster and the rack exclusively. The partial release
# must remove the exclusive spans of both, and must subtract the rack
# from the cluster before the visit to the cluster.
for pf in "ALL:core,ALL:node" "ALL:core,ALL:node,cluster:rack"; do
	test_expect_success "a partial release below an exclusive cluster frees the node ($pf)" '
		sed "1c match allocate ${excl_cluster2}" cmds_pc >cmds_pc_cluster &&
		${query} -f jgf -L ${jgf} -S CA -P lonode -F rv1_nosched \
			--prune-filters=$pf <cmds_pc_cluster >pcc.$pf.out 2>&1 &&
		test_debug "cat pcc.$pf.out" &&
		check_released_rack pcc.$pf.out
	'
done

#
# Issue #1561: a job that holds two exclusive racks (rank -1)
#
# Each partial release below rack0 visits rack0, which holds an exclusive
# span of job 1. The visit replaces that span with a placeholder of zero
# resources, and reports the size of rack0 to the cluster. The cluster
# then subtracts one rack from the count of the job. The placeholder is
# empty, so a later release below rack0 must report nothing. If it reports
# the size of rack0 again, the rack count of the cluster falls to zero
# although job 1 still holds rack1.
#

# Job 1 holds both racks. Then the test releases rank 0, rank 1, rank 2
# and rank 3, one rank at a time. Job 2 asks for one exclusive rack while
# job 1 holds rack1 and a part of rack0, so the scheduler must refuse it.
# Job 3 asks for one node after the release of rank 2, so it must start on
# the freed node 0. The test cancels job 3 to show the state of job 1 only
# in the last three finds.
cat >cmds_2racks <<-EOF
match allocate ${excl_2racks}
find jobid-span=1 and agfilter=true
partial-cancel 1 rv1exec ${free0}
find jobid-span=1 and agfilter=true
match allocate ${excl_1rack}
partial-cancel 1 rv1exec ${free1}
find jobid-span=1 and agfilter=true
partial-cancel 1 rv1exec ${free2}
find jobid-span=1 and agfilter=true
match allocate ${node1_flat}
cancel 3
partial-cancel 1 rv1exec ${free3}
info 1
find jobid-tag=1
find jobid-alloc=1
find sched-now=allocated
quit
EOF

test_expect_success 'a release of each rank below two exclusive racks completes the job' '
	${query} -f jgf -L ${jgf_2racks} -S CA -P lonode -F jgf \
		--prune-filters=${pf_rack} <cmds_2racks >2racks.out 2>&1 &&
	test_debug "cat 2racks.out" &&
	test_must_fail grep -E "ERROR|WARNING" 2racks.out &&
	grep "^INFO: 1, CANCELED" 2racks.out
'

# The rack count of the cluster is 2 after the match, and 1 after the
# release of rank 0. It must stay 1 after the release of rank 1 and after
# the release of rank 2, because job 1 still holds rack1. A second
# demotion of rack0 gives 0 instead. rack1 keeps every rank of its node,
# so its three lines must not change at all.
test_expect_success 'the cluster keeps the count of the rack that the job still holds' '
	for n in 1 2 3 4; do
		agfilter_of 2racks.out "jobid-span=1 and agfilter=true" ${n} \
			>2racks.${n} || return 1
	done &&
	cat >2racks.1.exp <<-EOF &&
	/tiny0 core used:8, total:8
	/tiny0 node used:4, total:4
	/tiny0 rack used:2, total:2
	/tiny0/rack0 core used:6, total:6
	/tiny0/rack0 node used:3, total:3
	/tiny0/rack0/node0 core used:2, total:2
	/tiny0/rack0/node1 core used:2, total:2
	/tiny0/rack0/node2 core used:2, total:2
	/tiny0/rack1 core used:2, total:2
	/tiny0/rack1 node used:1, total:1
	/tiny0/rack1/node3 core used:2, total:2
	EOF
	cat >2racks.2.exp <<-EOF &&
	/tiny0 core used:6, total:8
	/tiny0 node used:3, total:4
	/tiny0 rack used:1, total:2
	/tiny0/rack0 core used:4, total:6
	/tiny0/rack0 node used:2, total:3
	/tiny0/rack0/node1 core used:2, total:2
	/tiny0/rack0/node2 core used:2, total:2
	/tiny0/rack1 core used:2, total:2
	/tiny0/rack1 node used:1, total:1
	/tiny0/rack1/node3 core used:2, total:2
	EOF
	cat >2racks.3.exp <<-EOF &&
	/tiny0 core used:4, total:8
	/tiny0 node used:2, total:4
	/tiny0 rack used:1, total:2
	/tiny0/rack0 core used:2, total:6
	/tiny0/rack0 node used:1, total:3
	/tiny0/rack0/node2 core used:2, total:2
	/tiny0/rack1 core used:2, total:2
	/tiny0/rack1 node used:1, total:1
	/tiny0/rack1/node3 core used:2, total:2
	EOF
	cat >2racks.4.exp <<-EOF &&
	/tiny0 core used:2, total:8
	/tiny0 node used:1, total:4
	/tiny0 rack used:1, total:2
	/tiny0/rack1 core used:2, total:2
	/tiny0/rack1 node used:1, total:1
	/tiny0/rack1/node3 core used:2, total:2
	EOF
	for n in 1 2 3 4; do
		test_cmp 2racks.${n}.exp 2racks.${n} || return 1
	done
'

# The release of rank 2 gave the whole of rack0 back. Job 1 then has no
# span below rack0, so the find gives no line of rack0 and no line of its
# nodes.
test_expect_success 'a fully released rack keeps no count of the job' '
	test_must_fail grep "^/tiny0/rack0" 2racks.4 &&
	grep "^/tiny0 rack used:1," 2racks.4
'

test_expect_success 'the last release of a rank frees all the resources of the job' '
	for e in "jobid-tag=1" "jobid-alloc=1" "sched-now=allocated"; do
		find_jgf 2racks.out "$e" 1 >2racks.empty || return 1
		test_must_fail test -s 2racks.empty || return 1
	done
'

# The same commands with an R output show the placement. Job 1 gets the
# four ranks of the two racks, job 2 gets nothing, and job 3 gets rank 0.
test_expect_success 'a partly held rack refuses an exclusive request and keeps the free node' '
	${query} -f jgf -L ${jgf_2racks} -S CA -P lonode -F rv1_nosched \
		--prune-filters=${pf_rack} <cmds_2racks >2racks.r.out 2>&1 &&
	test_debug "cat 2racks.r.out" &&
	test $(grep -c "No matching resources found" 2racks.r.out) -eq 1 &&
	ranks_of 2racks.r.out >2racks.ranks &&
	printf "0-3\n0\n" >2racks.ranks.exp &&
	test_cmp 2racks.ranks.exp 2racks.ranks
'

# "remove <idset> false" runs a partial cancel by rank before it removes
# the vertices. The rack count of the cluster must behave as it does for
# an R-based release. Only the totals decrease, because the shrink takes
# the nodes out of the graph.
test_expect_success 'a shrink below one of two exclusive racks keeps the other rack' '
	cat >cmds_2racks_rm <<-EOF &&
	match allocate ${excl_2racks}
	find jobid-span=1 and agfilter=true
	remove 0 false
	find jobid-span=1 and agfilter=true
	remove 1 false
	find jobid-span=1 and agfilter=true
	quit
	EOF
	${query} -f jgf -L ${jgf_2racks} -S CA -P lonode -F jgf \
		--prune-filters=${pf_rack} <cmds_2racks_rm >2racks.rm.out 2>&1 &&
	test_debug "cat 2racks.rm.out" &&
	test_must_fail grep -E "ERROR|WARNING" 2racks.rm.out &&
	for n in 1 2 3; do
		agfilter_of 2racks.rm.out "jobid-span=1 and agfilter=true" ${n} \
			>2racks.rm.${n} || return 1
	done &&
	cat >2racks.rm.2.exp <<-EOF &&
	/tiny0 core used:6, total:6
	/tiny0 node used:3, total:3
	/tiny0 rack used:1, total:2
	/tiny0/rack0 core used:4, total:4
	/tiny0/rack0 node used:2, total:2
	/tiny0/rack0/node1 core used:2, total:2
	/tiny0/rack0/node2 core used:2, total:2
	/tiny0/rack1 core used:2, total:2
	/tiny0/rack1 node used:1, total:1
	/tiny0/rack1/node3 core used:2, total:2
	EOF
	cat >2racks.rm.3.exp <<-EOF &&
	/tiny0 core used:4, total:4
	/tiny0 node used:2, total:2
	/tiny0 rack used:1, total:2
	/tiny0/rack0 core used:2, total:2
	/tiny0/rack0 node used:1, total:1
	/tiny0/rack0/node2 core used:2, total:2
	/tiny0/rack1 core used:2, total:2
	/tiny0/rack1 node used:1, total:1
	/tiny0/rack1/node3 core used:2, total:2
	EOF
	test_cmp 2racks.1.exp 2racks.rm.1 &&
	test_cmp 2racks.rm.2.exp 2racks.rm.2 &&
	test_cmp 2racks.rm.3.exp 2racks.rm.3
'

# Job 2 reserves the two racks for the time after job 1. A shrink visits
# the reserved spans also, so the rack count of job 2 must behave as the
# rack count of job 1 does.
test_expect_success 'a shrink below two exclusive racks keeps the rack of a reservation' '
	cat >cmds_2racks_res <<-EOF &&
	match allocate ${excl_2racks}
	match allocate_orelse_reserve ${excl_2racks}
	find jobid-span=1 and agfilter=true
	find jobid-span=2 and agfilter=true
	remove 0 false
	find jobid-span=1 and agfilter=true
	find jobid-span=2 and agfilter=true
	remove 1 false
	find jobid-span=1 and agfilter=true
	find jobid-span=2 and agfilter=true
	quit
	EOF
	${query} -f jgf -L ${jgf_2racks} -S CA -P lonode -F jgf \
		--prune-filters=${pf_rack} <cmds_2racks_res >2racks.res.out 2>&1 &&
	test_debug "cat 2racks.res.out" &&
	test_must_fail grep -E "ERROR|WARNING" 2racks.res.out &&
	grep "^INFO: RESOURCES=RESERVED" 2racks.res.out &&
	grep "^INFO: SCHEDULED AT=3600" 2racks.res.out &&
	for n in 1 2 3; do
		for id in 1 2; do
			agfilter_of 2racks.res.out \
				"jobid-span=${id} and agfilter=true" ${n} \
				>2racks.res.${id}.${n} || return 1
		done
	done &&
	test_cmp 2racks.1.exp 2racks.res.1.1 &&
	test_cmp 2racks.rm.2.exp 2racks.res.1.2 &&
	test_cmp 2racks.rm.3.exp 2racks.res.1.3 &&
	for n in 1 2 3; do
		test_cmp 2racks.res.1.${n} 2racks.res.2.${n} || return 1
	done &&
	grep "^/tiny0 rack used:1," 2racks.res.2.3
'

# The next two tests reload this R. The free_ranks key tells the reader to
# skip rank 0, and the bad edge makes the reload fail after the reader
# demoted the exclusive racks.
test_expect_success 'write the JGF of the two rack job with a freed rank 0' '
	printf "match allocate ${excl_2racks}\nquit\n" >cmds_2racks_emit &&
	${query} -f jgf -L ${jgf_2racks} -S CA -P lonode -F jgf \
		--prune-filters=${pf_rack} <cmds_2racks_emit >2racks.emit.out 2>&1 &&
	grep "^{" 2racks.emit.out | jq -c ". + {\"free_ranks\": \"0\"}" \
		>2racks.R.free0.json &&
	jq -c ".graph.edges[-1].target = .graph.edges[-1].source" \
		2racks.R.free0.json >2racks.R.badedge.json &&
	test -s 2racks.R.free0.json &&
	test -s 2racks.R.badedge.json
'

# A shrink removed rank 0 before the reload, so the graph has no vertex of
# rank 0. The reader finds the ancestors of the freed rank by path and
# demotes rack0. A later release below rack0 must not demote rack0 again,
# so the rack count of the cluster must stay 1. The counts of the reload
# and of the live shrink agree, and only the totals are different, because
# the release of rank 1 keeps node1 in the graph.
test_expect_success 'a release after a reload with a freed rank keeps the other rack' '
	cat >cmds_2racks_rl <<-EOF &&
	remove 0 false
	update allocate jgf 2racks.R.free0.json 1 0 3600
	find jobid-span=1 and agfilter=true
	partial-cancel 1 rv1exec ${free1}
	find jobid-span=1 and agfilter=true
	quit
	EOF
	${query} -f jgf -L ${jgf_2racks} -S CA -P lonode -F jgf \
		--prune-filters=${pf_rack} <cmds_2racks_rl >2racks.rl.out 2>&1 &&
	test_debug "cat 2racks.rl.out" &&
	test_must_fail grep -E "ERROR|WARNING" 2racks.rl.out &&
	for n in 1 2; do
		agfilter_of 2racks.rl.out "jobid-span=1 and agfilter=true" ${n} \
			>2racks.rl.${n} || return 1
	done &&
	cat >2racks.rl.2.exp <<-EOF &&
	/tiny0 core used:4, total:6
	/tiny0 node used:2, total:3
	/tiny0 rack used:1, total:2
	/tiny0/rack0 core used:2, total:4
	/tiny0/rack0 node used:1, total:2
	/tiny0/rack0/node2 core used:2, total:2
	/tiny0/rack1 core used:2, total:2
	/tiny0/rack1 node used:1, total:1
	/tiny0/rack1/node3 core used:2, total:2
	EOF
	test_cmp 2racks.rm.2.exp 2racks.rl.1 &&
	test_cmp 2racks.rl.2.exp 2racks.rl.2 &&
	used_only 2racks.rm.3.exp >2racks.rm.3.used &&
	used_only 2racks.rl.2 >2racks.rl.2.used &&
	test_cmp 2racks.rm.3.used 2racks.rl.2.used
'

# A reload can fail after the reader demoted an exclusive rack to a
# placeholder, for example when the last edge of R is not in the graph.
# The rollback must then remove the placeholders also, and must keep the
# resources of every other job. Job 1 is an unrelated node job on rank 0,
# and R has a freed rank 0, so the two jobs share no resource.
test_expect_success 'a failed reload after a demotion keeps the resources of another job' '
	cat >cmds_2racks_badedge <<-EOF &&
	match allocate ${node1_flat}
	find jobid-alloc=1
	find jobid-tag=1
	find sched-now=allocated
	find jobid-span=1 and agfilter=true
	update allocate jgf 2racks.R.badedge.json 2 0 3600
	find jobid-alloc=2
	find jobid-tag=2
	find sched-now=allocated
	find jobid-alloc=1
	find jobid-tag=1
	find jobid-span=1 and agfilter=true
	update allocate jgf 2racks.R.free0.json 2 0 3600
	find jobid-alloc=2
	update reserve jgf 2racks.R.badedge.json 3 3600 3600
	update reserve jgf 2racks.R.free0.json 3 3600 3600
	quit
	EOF
	${query} -f jgf -L ${jgf_2racks} -S CA -P lonode -F jgf \
		--prune-filters=${pf_rack} <cmds_2racks_badedge \
		>2racks.badedge.out 2>&1 &&
	test_debug "cat 2racks.badedge.out" &&
	test $(grep -c "not found in resource graph" 2racks.badedge.out) -eq 2
'

test_expect_success 'a failed reload changes no resource of the other job' '
	for e in "jobid-alloc=1" "jobid-tag=1" "sched-now=allocated" \
		"jobid-span=1 and agfilter=true"; do
		find_jgf 2racks.badedge.out "$e" 1 >2racks.be.before || return 1
		find_jgf 2racks.badedge.out "$e" 2 >2racks.be.after || return 1
		test -s 2racks.be.before || return 1
		test_cmp 2racks.be.before 2racks.be.after || return 1
	done
'

test_expect_success 'a failed reload keeps no resource of the failed job' '
	for e in "jobid-alloc=2" "jobid-tag=2"; do
		find_jgf 2racks.badedge.out "$e" 1 >2racks.be.job2 || return 1
		test_must_fail test -s 2racks.be.job2 || return 1
	done
'

test_expect_success 'a correct reload after a failed reload succeeds' '
	find_jgf 2racks.badedge.out "jobid-alloc=2" 2 >2racks.be.job2.ok &&
	test -s 2racks.be.job2.ok &&
	test $(grep -c "^INFO: RESOURCES=ALLOCATED" 2racks.badedge.out) -eq 2 &&
	test $(grep -c "^INFO: RESOURCES=RESERVED" 2racks.badedge.out) -eq 1
'

test_expect_success 'write the full and the shorthand JGF of an exclusive rack job' '
	printf "match allocate ${excl_rack2}\nquit\n" >cmds_emit &&
	for fmt in jgf jgf_shorthand; do
		${query} -f jgf -L ${jgf} -S CA -P lonode -F $fmt \
			<cmds_emit >emit.$fmt.out 2>&1 &&
		grep "^{" emit.$fmt.out \
			| jq -c ". + {\"free_ranks\": \"0\"}" >R.$fmt.free0.json ||
		return 1
	done
'

for fmt in jgf jgf_shorthand; do
	test_expect_success "a reload from $fmt with a freed rank below an exclusive rack is correct" '
		sed "1,2c update allocate $fmt R.$fmt.free0.json 1 0 3600" \
			cmds_pc >cmds_rl.$fmt &&
		${query} -f jgf -L ${jgf} -S CA -P lonode -F rv1_nosched \
			<cmds_rl.$fmt >rl.$fmt.out 2>&1 &&
		test_debug "cat rl.$fmt.out" &&
		check_released_rack rl.$fmt.out
	'
	test_expect_success "a reload from $fmt gives the same state as a partial release" '
		${query} -f jgf -L ${jgf} -S CA -P lonode -F simple \
			<cmds_pc >pc.simple.out 2>&1 &&
		${query} -f jgf -L ${jgf} -S CA -P lonode -F simple \
			<cmds_rl.$fmt >rl.$fmt.simple.out 2>&1 &&
		after_first_alloc pc.simple.out | grep -v "^INFO: 1, " >pc.state &&
		after_first_alloc rl.$fmt.simple.out | grep -v "^INFO: 1, " \
			>rl.$fmt.state &&
		test_cmp pc.state rl.$fmt.state
	'
done

# A reload can fail after the reader added the schedule spans of the job,
# for example when an edge of R is not in the graph. The reader must then
# remove the spans, and a correct reload must succeed afterwards.
for fmt in jgf jgf_shorthand; do
	test_expect_success "a failed reload from $fmt with a bad edge keeps no resources" '
		grep "^{" emit.$fmt.out >R.$fmt.json &&
		jq -c ".graph.edges[-1].target = .graph.edges[-1].source" \
			R.$fmt.json >R.$fmt.badedge.json &&
		cat >cmds_badedge.$fmt <<-EOF &&
		update allocate $fmt R.$fmt.badedge.json 1 0 3600
		find sched-now=allocated
		find jobid-alloc=1
		update reserve $fmt R.$fmt.badedge.json 2 100 3600
		find sched-future=reserved
		update allocate $fmt R.$fmt.json 1 0 3600
		find sched-now=allocated
		quit
		EOF
		${query} -f jgf -L ${jgf} -S CA -P lonode -F rv1_nosched \
			<cmds_badedge.$fmt >badedge.$fmt.out 2>&1 &&
		test_debug "cat badedge.$fmt.out" &&
		test $(grep -c "not found in resource graph" badedge.$fmt.out) -eq 2 &&
		ranks_of badedge.$fmt.out >badedge.$fmt.ranks &&
		printf "0-1\n0-1\n" >badedge.expected &&
		test_cmp badedge.expected badedge.$fmt.ranks
	'
done

# A shrink removed rank 0 before the reload, so the graph has no vertex of
# rank 0. The full JGF R still has the paths of the freed vertices, and the
# reload must demote the rack as the live shrink does. Then a recovered
# node 0 is available to a node job. A shorthand R has no path of a freed
# vertex, so that reload cannot demote the rack: a known limitation.
test_expect_success 'write the subgraph of node 0 for a later attach' '
	jq -c "(.graph.nodes | map(select(.metadata.paths.containment as \$p
	        | (\$p == \"/tiny0\" or \$p == \"/tiny0/rack0\"
	           or \$p == \"/tiny0/rack0/node0\"
	           or (\$p | startswith(\"/tiny0/rack0/node0/\")))))) as \$nodes
	    | (\$nodes | map(.id)) as \$ids
	    | {graph: {nodes: \$nodes, edges: (.graph.edges | map(select(
	          (.source as \$s | \$ids | index(\$s)) != null
	          and (.target as \$t | \$ids | index(\$t)) != null)))}}" \
		${jgf} >grow-node0.json &&
	test $(jq ".graph.nodes | length" grow-node0.json) -eq 51
'

cat >cmds_grow <<-EOF
match allocate ${excl_rack2}
remove 0 false
attach grow-node0.json
find jobid-alloc=1
match allocate ${node1}
match allocate ${excl_rack1}
info 1
quit
EOF

test_expect_success 'after a shrink and a recovery of node 0, a node job can use node 0' '
	${query} -f jgf -L ${jgf} -S CA -P lonode -F rv1_nosched \
		<cmds_grow >grow.live.out 2>&1 &&
	test_debug "cat grow.live.out" &&
	test_must_fail grep ERROR grow.live.out &&
	test $(grep -c "No matching resources found" grow.live.out) -eq 1 &&
	ranks_of grow.live.out | tail -1 >grow.live.last &&
	echo 0 >grow.expected &&
	test_cmp grow.expected grow.live.last &&
	grep "^INFO: 1, ALLOCATED" grow.live.out
'

test_expect_success 'a reload from jgf after the shrink gives the same state as the live shrink' '
	sed "1,2c remove 0 false\nupdate allocate jgf R.jgf.free0.json 1 0 3600" \
		cmds_grow >cmds_grow_rl.jgf &&
	${query} -f jgf -L ${jgf} -S CA -P lonode -F rv1_nosched \
		<cmds_grow_rl.jgf >grow.rl.jgf.out 2>&1 &&
	test_debug "cat grow.rl.jgf.out" &&
	test_must_fail grep ERROR grow.rl.jgf.out &&
	ranks_of grow.rl.jgf.out | tail -1 >grow.rl.jgf.last &&
	test_cmp grow.expected grow.rl.jgf.last &&
	${query} -f jgf -L ${jgf} -S CA -P lonode -F simple \
		<cmds_grow >grow.live.simple.out 2>&1 &&
	${query} -f jgf -L ${jgf} -S CA -P lonode -F simple \
		<cmds_grow_rl.jgf >grow.rl.jgf.simple.out 2>&1 &&
	after_first_alloc grow.live.simple.out | grep -v "^INFO: 1, " >grow.live.state &&
	after_first_alloc grow.rl.jgf.simple.out | grep -v "^INFO: 1, " >grow.rl.jgf.state &&
	test_cmp grow.live.state grow.rl.jgf.state
'

test_expect_failure 'a reload from jgf_shorthand after the shrink frees a recovered node 0' '
	sed "1,2c remove 0 false\nupdate allocate jgf_shorthand R.jgf_shorthand.free0.json 1 0 3600" \
		cmds_grow >cmds_grow_rl.jgf_shorthand &&
	${query} -f jgf -L ${jgf} -S CA -P lonode -F rv1_nosched \
		<cmds_grow_rl.jgf_shorthand >grow.rl.jgf_shorthand.out 2>&1 &&
	test_debug "cat grow.rl.jgf_shorthand.out" &&
	ranks_of grow.rl.jgf_shorthand.out | tail -1 >grow.rl.jgf_shorthand.last &&
	test_cmp grow.expected grow.rl.jgf_shorthand.last
'

test_expect_success 'a shorthand reload is correct when uniq_id and vertex order are different' '
	jq ".graph.nodes |= map(.id = ((.id | tonumber) + 1000 | tostring)
	        | .metadata.uniq_id += 1000)
	    | .graph.edges |= map(.source = ((.source | tonumber) + 1000 | tostring)
	        | .target = ((.target | tonumber) + 1000 | tostring))" \
		${jgf} >offset.json &&
	${query} -f jgf -L offset.json -S CA -P lonode -F jgf_shorthand \
		<cmds_emit >emit.offset.out 2>&1 &&
	grep "^{" emit.offset.out >R.offset.json &&
	cat >cmds_offset <<-EOF &&
	update allocate jgf_shorthand R.offset.json 1 0 3600
	match allocate ${node1}
	quit
	EOF
	${query} -f jgf -L offset.json -S CA -P lonode -F rv1_nosched \
		<cmds_offset >offset.out 2>&1 &&
	test_debug "cat offset.out" &&
	test_must_fail grep ERROR offset.out &&
	grep "No matching resources found" offset.out
'

test_done

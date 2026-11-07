#!/bin/sh

test_description='Test that JGF execution target ranks are validated against R

R assigns execution targets by position: the Nth host of
.execution.nodelist is the host of the Nth lowest rank in .execution.R_lite.
The JGF in .scheduling carries its own rank per vertex. Fluxion does not
reconcile the two -- when they disagree one of them is misconfigured and
fluxion cannot tell which -- it validates them and refuses to build a graph
it cannot trust. These tests cover what that validation has to get right: a
host that owns several ranks (every host does, in an instance running more
than one broker per node), a storage_node owning an execution target just as
a node does, and the types and hosts that are deliberately left unchecked.
'

. $(dirname $0)/sharness.sh

# module-nopanic lets a failed `flux module load` return an error instead of
# taking the broker -- and thus the rest of this file -- down with it.
test_under_flux 2 full -Sbroker.module-nopanic=1

query="${SHARNESS_BUILD_DIRECTORY}/resource/utilities/resource-query"

# Usage: jgf_from_r R-file > jgf-file
# Emit the full resource graph of an R as JGF. This is the same graph fluxion
# would build from that R with the rv1exec reader, so the ranks it carries
# agree with R by construction and each test below perturbs only what it
# means to test.
jgf_from_r() {
	printf "find status=up\nquit\n" | \
	    ${query} -L $1 -f rv1exec -F jgf -S CA -P high | head -1
}

# Usage: load_fluxion
# The stats RPC is the sync point: `flux module load` returns before mod_main
# has built the resource graph, so without it a graph that fluxion rejects
# still looks like a successful load here.
load_fluxion() {
	load_resource &&
	flux module stats sched-fluxion-resource >/dev/null
}

# Usage: load_jgf jgf-file
# Install jgf-file as R's .scheduling key and load fluxion against it.
load_jgf() {
	jq --slurpfile jgf $1 ".scheduling = \$jgf[0]" base.R >reload.R &&
	flux kvs put resource.R="$(cat reload.R)" &&
	flux dmesg -C &&
	flux module reload resource &&
	load_fluxion
}

# Usage: config_jgf jgf-file
# As load_jgf, for the configured instance below. The KVS route is not open
# there: with a [[resource.config]] table the resource module regenerates R
# from the config on every reload, so .scheduling has to arrive by the file
# that [resource] scheduling names.
config_jgf() {
	cp $1 sched.json &&
	flux dmesg -C &&
	flux module reload resource &&
	load_fluxion
}

test_expect_success 'unload sched-simple' '
	flux module remove -f sched-simple
'

#
# A host that owns several ranks. Every host does when an instance runs more
# than one broker per node, which is exactly how this test instance runs: its
# nodelist names one host twice, once for rank 0 and once for rank 1.
#

test_expect_success 'this instance puts two ranks on one host' '
	flux kvs get resource.R >base.R &&
	test $(jq -r ".execution.nodelist[0]" base.R | flux hostlist -c) -eq 2 &&
	test $(jq -r ".execution.nodelist[0]" base.R | sort -u | wc -l) -eq 1
'

test_expect_success 'its JGF has one node vertex per rank' '
	jgf_from_r base.R >multirank.jgf &&
	test $(jq "[.graph.nodes[] |
	    select(.metadata.type == \"node\")] | length" multirank.jgf) -eq 2 &&
	test $(jq "[.graph.nodes[] |
	    select(.metadata.type == \"node\") |
	    .metadata.paths.containment] | unique | length" multirank.jgf) -eq 1
'

test_expect_success 'fluxion loads a JGF whose host owns two ranks' '
	load_jgf multirank.jgf &&
	load_qmanager &&
	test $(flux resource list -s free -no {nnodes}) -eq 2
'

test_expect_success 'a job can use both ranks' '
	run_timeout 60 flux run -N2 -n2 hostname
'

test_expect_success 'unload fluxion' '
	remove_qmanager &&
	remove_resource
'

test_expect_success 'a JGF rank the host does not own is rejected' '
	jq "(.graph.nodes[] | select(.metadata.rank == 1) | .metadata.rank) = 7" \
	    multirank.jgf >badrank.jgf &&
	test_must_fail load_jgf badrank.jgf &&
	flux dmesg | grep validate_vtx_rank
'

test_expect_success 'a node vertex with no rank at all is rejected' '
	jq "del(.graph.nodes[] | select(.metadata.rank == 1) | .metadata.rank)" \
	    multirank.jgf >norank.jgf &&
	test_must_fail load_jgf norank.jgf &&
	flux dmesg | grep validate_vtx_rank
'

#
# One rank per host, so that a wrong rank is a plain contradiction rather
# than a host owning the wrong one of its own ranks. Renaming the hosts also
# makes them distinguishable, which the cases below need.
#

test_expect_success 'reconfigure the instance with one host per rank' '
	cat >resource.toml <<-EOF &&
	[resource]
	noverify = true
	norestrict = true

	[[resource.config]]
	hosts = "fake[0-1]"
	cores = "0-1"
	EOF
	flux config load resource.toml &&
	flux module reload resource &&
	flux kvs get resource.R >base.R &&
	test "$(jq -r ".execution.nodelist[0]" base.R)" = "fake[0-1]" &&
	jgf_from_r base.R >onerank.jgf
'

test_expect_success 'point the configuration at a JGF file' '
	cp onerank.jgf sched.json &&
	sed -e "s|^\[resource\]$|[resource]\nscheduling = \"${PWD}/sched.json\"|" \
	    resource.toml >resource-jgf.toml &&
	flux config load resource-jgf.toml &&
	flux module reload resource
'

test_expect_success 'fluxion loads a JGF with one rank per host' '
	config_jgf onerank.jgf &&
	test $(flux resource list -s free -no {nnodes}) -eq 2 &&
	remove_resource
'

test_expect_success 'a JGF rank that contradicts R is rejected' '
	jq "(.graph.nodes[] |
	    select(.metadata.paths.containment == \"/cluster0/fake1\") |
	    .metadata.rank) = 0" onerank.jgf >wrongrank.jgf &&
	test_must_fail config_jgf wrongrank.jgf &&
	flux dmesg | grep "JGF rank=0 for hostname=fake1"
'

#
# storage_node owns an execution target just as node does, so its rank is
# validated on the same terms.
#

test_expect_success 'a storage_node vertex is validated like a node' '
	jq "(.graph.nodes[] |
	    select(.metadata.paths.containment == \"/cluster0/fake1\") |
	    .metadata.type) = \"storage_node\"" onerank.jgf >storage.jgf &&
	config_jgf storage.jgf &&
	remove_resource
'

test_expect_success 'a storage_node rank that contradicts R is rejected' '
	jq "(.graph.nodes[] |
	    select(.metadata.paths.containment == \"/cluster0/fake1\") |
	    .metadata) += {type: \"storage_node\", rank: 0}" \
	    onerank.jgf >badstorage.jgf &&
	test_must_fail config_jgf badstorage.jgf &&
	flux dmesg | grep "JGF rank=0 for hostname=fake1"
'

#
# Only a node or a storage_node owns an execution target. A vertex of any
# other type is not matched against the nodelist even when it is named like
# a host, and a host R does not name is not checked at all.
#

# A vertex with no `name` key takes its name from basename and id, so this
# names a core under fake0 "fake1" -- the name R gives the *other* host, at
# the other rank. It loads because a core is not matched against the
# nodelist at all.
test_expect_success "a non-host vertex named like a host is not validated" '
	jq "(.graph.nodes[] |
	    select(.metadata.paths.containment == \"/cluster0/fake0/core1\") |
	    .metadata) += {basename: \"fake\", id: 1}" \
	    onerank.jgf >namesake.jgf &&
	jq -e "[.graph.nodes[] |
	    select(.metadata.paths.containment == \"/cluster0/fake0/core1\") |
	    .metadata.rank] == [0]" namesake.jgf >/dev/null &&
	config_jgf namesake.jgf &&
	test $(flux resource list -s free -no {nnodes}) -eq 2 &&
	remove_resource
'

# Renaming the host to one R does not list makes the nodelist lookup miss.
# A miss is not an error: a JGF may describe resources the instance was not
# given. Were a miss treated as one, this would be rejected.
test_expect_success "a host R does not name is not validated" '
	jq "(.graph.nodes[] |
	    select(.metadata.paths.containment == \"/cluster0/fake1\") |
	    .metadata) += {basename: \"notinr\", id: 0}" \
	    onerank.jgf >unknownhost.jgf &&
	test $(jq -r ".execution.nodelist[0]" base.R | \
	    flux hostlist -n | grep -c "^notinr0$") -eq 0 &&
	config_jgf unknownhost.jgf &&
	remove_resource
'

#
# A containment edge may not join two vertices that both declare a rank and
# disagree, however that disagreement arises.
#

test_expect_success 'a core whose rank contradicts its node is rejected' '
	jq "(.graph.nodes[] |
	    select(.metadata.paths.containment == \"/cluster0/fake1/core0\") |
	    .metadata.rank) = 0" onerank.jgf >splitrank.jgf &&
	test_must_fail config_jgf splitrank.jgf &&
	flux dmesg | grep validate_edge_ranks
'

test_expect_success 'a core that declares no rank is left alone' '
	jq "del(.graph.nodes[] |
	    select(.metadata.paths.containment == \"/cluster0/fake1/core0\") |
	    .metadata.rank)" onerank.jgf >coreneutral.jgf &&
	config_jgf coreneutral.jgf &&
	test $(flux resource list -s free -no {nnodes}) -eq 2 &&
	remove_resource
'

test_done

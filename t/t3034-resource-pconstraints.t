#!/bin/sh

test_description='Test property constraints-based matching'

. $(dirname $0)/sharness.sh

query="../../resource/utilities/resource-query"

test_expect_success 'pconstraints: configuring a heterogeneous machine works' '
	flux R encode -r 0 -c 0-1 -g 0-1 -p "arm-v9@core:0" -H foo2 > out &&
	flux R encode -r 1 -c 0 -H foo3 -p "arm-v8@core:1" >> out &&
	flux R encode -r 2-3 -c 0-1 -g 0-1 -p "arm-v9@core:2-3" \
	    -p "amd-mi60@gpu:2-3" -H foo[1,4] >> out &&
	cat out | flux R append > hetero.json
'

test_expect_success 'pconstraints: generate property-based jobspecs' '
	flux submit --requires="arm-v9@core" --dry-run hostname \
		> job.arm-v9.json &&
	flux submit --requires="arm-v8@core,amd-mi60@gpu" \
		 --dry-run hostname > job.arm-v9+amd-mi60.json &&
	flux submit --requires="amd-m50@gpu" --dry-run hostname \
		> job.amd-mi50.json &&
	flux submit -n2 -c1 -g2 --requires="arm-v9@core" \
		 --dry-run hostname > job.arm-v9+gpu.json &&
	flux submit -n1 --requires="^arm-v9@core" \
		--dry-run hostname > job.not-arm-v9.json &&
	flux submit -n1 --requires="ar^m-v9@core" \
		--dry-run hostname > job.invalid.json &&
	flux submit -n1 -c1 -g1 --dry-run hostname > job.gpu.json
'

#
# Selection Policy -- High node first with node exclusivity (-P hinodex)
#     Selection behavior is identical to hinode except that
#     it marks each selected node as exclusive even if the
#     jobspec does not require node exclusivity and
#     that it selects and emits all of the node-local resources
#     for each node where at least one node-local resource is selected.
#
#     For a jobspec with node[1]->slot[1]->core[1], it selects
#     36 cores from the selected node if there is a total of
#     36 cores in that node.
#
#     For a jobspec with slot[18]->core[1], it selects
#     again all 36 cores from the current available highest node.
#

test_expect_success 'allocate 1 job with arm-v9@core (pol=hinodex)' '
	cat <<-EOF > cmds001 &&
	match allocate job.arm-v9.json
	quit
	EOF
	${query} -L hetero.json -f rv1exec -S CA -P hinodex -t 001.R.out \
		-F rv1_nosched < cmds001 &&
	host=$(cat 001.R.out | grep -v INFO | jq -r ".execution.nodelist[]") &&
	test "${host}" = "foo4"
'

test_expect_success 'allocate 1 job with unsat properties (pol=hinodex)' '
	cat <<-EOF > cmds002 &&
	match allocate job.arm-v9+amd-mi60.json
	quit
	EOF
	${query} -L hetero.json -f rv1exec -S CA -P hinodex -t 002.R.out \
		-F rv1_nosched < cmds002 &&
	grep "No matching resources found" 002.R.out
'

test_expect_success 'allocate 1 job with nonexistent property (pol=hinodex)' '
	cat <<-EOF > cmds003 &&
	match allocate job.amd-mi50.json
	quit
	EOF
	${query} -L hetero.json -f rv1exec -S CA -P hinodex -t 003.R.out \
		-F rv1_nosched < cmds003 &&
	grep "No matching resources found" 003.R.out
'

test_expect_success 'allocate 1 job with heterogeneous GPUs (pol=hinodex)' '
	cat <<-EOF > cmds004 &&
	match allocate job.arm-v9+gpu.json
	quit
	EOF
	${query} -L hetero.json -f rv1exec -S CA -P hinodex -t 004.R.out \
		-F rv1_nosched < cmds004 &&
	host=$(cat 004.R.out | grep -v INFO | jq -r ".execution.nodelist[]") &&
	test "${host}" = "foo[2,4]"
'

test_expect_success 'allocate 1 job with generic GPU (pol=hinodex)' '
	cat <<-EOF > cmds005 &&
	match allocate job.gpu.json
	quit
	EOF
	${query} -L hetero.json -f rv1exec -S CA -P hinodex -t 005.R.out \
		-F rv1_nosched < cmds005 &&
	host=$(cat 005.R.out | grep -v INFO | jq -r ".execution.nodelist[]") &&
	test "${host}" = "foo4"
'

test_expect_success 'allocate 1 job with ^arm-v9@core (pol=hinodex)' '
	cat <<-EOF > cmds006 &&
	match allocate job.not-arm-v9.json
	quit
	EOF
	${query} -L hetero.json -f rv1exec -S CA -P hinodex -t 006.R.out \
		-F rv1_nosched < cmds006 &&
	host=$(cat 006.R.out | grep -v INFO | jq -r ".execution.nodelist[]") &&
	test "${host}" = "foo3"
'

test_expect_success 'allocate 1 job with invalid property (pol=hinodex)' '
	cat <<-EOF > cmds007 &&
	match allocate job.invalid.json
	quit
	EOF
	${query} -L hetero.json -f rv1exec -S CA -P hinodex -t 007.R.out \
		-F rv1_nosched < cmds007 2> 007.R.out2 &&
	grep "Jobspec error" 007.R.out2
'

test_expect_success 'graphml output can be generated with properties' '
	cat <<-EOF > cmds008 &&
	quit
	EOF
	${query} -L hetero.json -f rv1exec -S CA -P hinodex -t 008.R.out \
		-o 008 -g graphml -F rv1_nosched < cmds008
'

test_expect_success 'allocate 1 job with arm-v9@core and rv1 (pol=hinodex)' '
	cat <<-EOF > cmds011 &&
	match allocate job.arm-v9.json
	quit
	EOF
	cat <<-EOF > prop11.exp &&
	amd-mi60@gpu
	arm-v9@core
	EOF
	${query} -L hetero.json -f rv1exec -S CA -P hinodex -t 011.R.out \
		-F rv1 < cmds011 &&
	cat 011.R.out | grep -v INFO | jq -r ".scheduling.graph.nodes[] | \
		select (.metadata.type == \"node\") | .metadata.properties \
		| keys[]" > prop11.out &&
	test_cmp prop11.exp prop11.out &&
	host=$(cat 011.R.out | grep -v INFO | jq -r ".execution.nodelist[]") &&
	jgfh=$(cat 011.R.out | grep -v INFO | \
		jq -r ".scheduling.graph.nodes[] | \
		select (.metadata.type == \"node\") | .metadata.basename") &&
	jgfid=$(cat 011.R.out | grep -v INFO | \
		jq -r ".scheduling.graph.nodes[] | \
		select (.metadata.type == \"node\") | .metadata.id") &&
	test "$host" = "${jgfh}${jgfid}"
'

test_expect_success 'allocate 1 job with arm-v9@core and JGF (pol=hinodex)' '
	cat <<-EOF > cmds012 &&
	match allocate job.arm-v9.json
	quit
	EOF
	cat <<-EOF > prop12.exp &&
	amd-mi60@gpu
	arm-v9@core
	EOF
	${query} -L hetero.json -f rv1exec -S CA -P hinodex -t 012.R.out \
		-F rv1 < cmds012 &&
	cat 012.R.out | grep -v INFO | jq -r ".scheduling" > jgf.json &&
	${query} -L jgf.json -f jgf -S CA -P hinodex -t 012.R2.out \
		-F rv1_nosched < cmds012 &&
        host=$(cat 012.R2.out | grep -v INFO | jq -r ".execution.nodelist[]") &&
        test "${host}" = "foo4"
'

test_done

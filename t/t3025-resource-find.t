#!/bin/sh

test_description='Test Resource Find On Tiny Machine Configuration'

. $(dirname $0)/sharness.sh

sock1_job="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/find/sock1.yaml"
c1g1m1_job="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/find/c1g1m1.yaml"
node1_job="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/find/node1.yaml"
full_job="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/find/full.yaml"
cmd_dir="${SHARNESS_TEST_SRCDIR}/data/resource/commands/find"
exp_dir="${SHARNESS_TEST_SRCDIR}/data/resource/expected/find"
grugs="${SHARNESS_TEST_SRCDIR}/data/resource/grugs/tiny.graphml"
jgf="${SHARNESS_TEST_SRCDIR}/data/resource/jgfs/tiny.json"
query="../../resource/utilities/resource-query"

remove_times() {
    cat ${1} | jq 'del (.execution.starttime) | del (.execution.expiration)'
}


test_expect_success 'a jobspec requesting all resources works' '
    cat > cmds001 <<-EOF &&
	match allocate ${full_job}
	quit
EOF
    ${query} -L ${grugs} -F rv1_nosched -S CA -P high -t full.out < cmds001 &&
    cat full.out | grep -v "INFO" > full.json &&
    remove_times full.json > full.filt.json
'

test_expect_success 'find status=up returns the entire system' '
    cat > cmds002 <<-EOF &&
	find status=up
	quit
EOF
    ${query} -L ${grugs} -F rv1_nosched -S CA -P high -t up.out < cmds002 &&
    cat up.out | grep -v "INFO" > up.json &&
    remove_times up.json > up.filt.json &&
    test_cmp up.filt.json full.filt.json
'

test_expect_success 'mark down 3 cores and find status=down' '
    cat > cmds003 <<-EOF &&
	set-status /tiny0/rack0/node0/socket0/core3 down
	set-status /tiny0/rack0/node0/socket0/core4 down
	set-status /tiny0/rack0/node0/socket0/core7 down
	find status=down
	quit
EOF
    ${query} -L ${grugs} -F rv1_nosched -S CA -P high -t down.out < cmds003 &&
    cat down.out | grep -v "INFO" > down.json &&
    cores=$(cat down.json | jq " .execution.R_lite[].children.core ") &&
    test ${cores} = "\"3-4,7\""
'

test_expect_success 'mark down all but 1 socket and find status=up' '
    cat > cmds004 <<-EOF &&
	set-status /tiny0/rack0/node0 down
	set-status /tiny0/rack0/node1/socket1 down
	find status=up
	quit
EOF
    ${query} -L ${grugs} -F rv1_nosched -S CA -P high -t down2.out < cmds004 &&
    cat down2.out | grep -v "INFO" > down2.json &&
    cores=$(cat down2.json | jq " .execution.R_lite[].children.core ") &&
    gpus=$(cat down2.json | jq " .execution.R_lite[].children.gpu ") &&
    test ${cores} = "\"0-17\"" &&
    test ${gpus} = "\"0\""
'

test_expect_success 'mark down 1 node and find status=up and status=down' '
    cat > cmds005 <<-EOF &&
	set-status /tiny0/rack0/node1 down
	find status=up and status=down
	quit
EOF
    ${query} -L ${grugs} -F rv1_nosched -S CA -P high -t down3.out < cmds005 &&
    test $(wc -l <down3.out) -eq 3
'

test_expect_success 'mark down 1 node and find status=up or status=down' '
    cat > cmds006 <<-EOF &&
	set-status /tiny0/rack0/node0 down
	find status=up or status=down
	quit
EOF
    ${query} -L ${grugs} -F rv1_nosched -S CA -P high -t down4.out < cmds006 &&
    cat down4.out | grep -v "INFO" > down4.json &&
    remove_times down4.json > down4.filt.json &&
    test_cmp down4.filt.json full.filt.json
'

test_expect_success 'allocate 1 core and 1 gpu and find sched-now=allocated' '
    cat > cmds007 <<-EOF &&
	match allocate ${c1g1m1_job}
	find sched-now=allocated
	quit
EOF
    ${query} -L ${grugs} -F rv1_nosched -S CA -P high -t alloc.out < cmds007 &&
    cat alloc.out | awk "{if(NR==1) print \$0}" > alloc.1.json &&
    cat alloc.out | awk "{if(NR==7) print \$0}" > alloc.2.json &&
    remove_times alloc.1.json > alloc.1.filt.json &&
    remove_times alloc.2.json > alloc.2.filt.json &&
    test_cmp alloc.1.filt.json alloc.2.filt.json
'

test_expect_success 'allocate 1 node and 1 socket and find sched-now=free' '
    cat > cmds008 <<-EOF &&
	match allocate ${node1_job}
	match allocate ${sock1_job}
	find sched-now=free
	quit
EOF
    ${query} -L ${grugs} -F rv1_nosched -S CA -P high -t alloc2.out < cmds008 &&
    cat alloc2.out | tail -n4 | grep -v "INFO" > alloc2.1.json &&
    cores=$(cat alloc2.1.json | jq " .execution.R_lite[].children.core ") &&
    gpus=$(cat alloc2.1.json  | jq " .execution.R_lite[].children.gpu ") &&
    test ${cores} = "\"0-17\"" &&
    test ${gpus} = "\"0\""
'

test_expect_success 'reserve 1 core and 1 gpu and find sched-future=reserved' '
    cat > cmds009 <<-EOF &&
	match allocate ${full_job}
	match allocate_orelse_reserve ${c1g1m1_job}
	find sched-future=reserved
	quit
EOF
    ${query} -L ${grugs} -F rv1_nosched -S CA -P high -t reserve.out < cmds009 &&
    cat reserve.out | tail -n4 | grep -v "INFO" > reserve.json &&
    cores=$(cat reserve.json | jq " .execution.R_lite[].children.core ") &&
    gpus=$(cat reserve.json | jq " .execution.R_lite[].children.gpu ") &&
    echo $cores > cores &&
    echo $gpus > gpus &&
    test ${cores} = "\"35\"" &&
    test ${gpus} = "\"1\""
'

test_expect_success 'reserve 1 node and 1 socket and find sched-future=free' '
    cat > cmds010 <<-EOF &&
	match allocate ${full_job}
	match allocate_orelse_reserve ${node1_job}
	match allocate_orelse_reserve ${sock1_job}
	find sched-future=free
	quit
EOF
    ${query} -L ${grugs} -F rv1_nosched -S CA -P high -t reserve2.out < cmds010 &&
    cat reserve2.out | tail -n4 | grep -v "INFO" > reserve2.json &&
    cores=$(cat reserve2.json | jq " .execution.R_lite[].children.core ") &&
    gpus=$(cat reserve2.json | jq " .execution.R_lite[].children.gpu ") &&
    test ${cores} = "\"0-17\"" &&
    test ${gpus} = "\"0\""
'

test_expect_success 'compound expression (status=down sched-now=allocated) works' '
    cat > cmds011 <<-EOF &&
	match allocate ${full_job}
	set-status /tiny0/rack0/node0 down
	match allocate_orelse_reserve ${node1_job}
	match allocate_orelse_reserve ${sock1_job}
	find status=down sched-now=allocated
	quit
EOF
    ${query} -L ${grugs} -F rv1_nosched -S CA -P high -t mixed.out < cmds011 &&
    cat mixed.out | tail -n4 | grep -v "INFO" > mixed.json &&
    cores=$(cat mixed.json | jq " .execution.R_lite[].children.core ") &&
    gpus=$(cat mixed.json | jq " .execution.R_lite[].children.gpu ") &&
    test ${cores} = "\"0-35\"" &&
    test ${gpus} = "\"0-1\""
'

test_expect_success 'compound expression (sched-future=reserved status=up) works' '
    cat > cmds012 <<-EOF &&
	match allocate ${full_job}
	set-status /tiny0/rack0/node0 down
	match allocate_orelse_reserve ${node1_job}
	match allocate_orelse_reserve ${sock1_job}
	find sched-future=reserved status=up
	quit
EOF
    ${query} -L ${grugs} -F rv1_nosched -S CA -P high -t mixed2.out < cmds012 &&
    cat mixed2.out | tail -n4 | grep -v "INFO" > mixed2.json &&
    cores=$(cat mixed2.json | jq " .execution.R_lite[].children.core ") &&
    gpus=$(cat mixed2.json | jq " .execution.R_lite[].children.gpu ") &&
    test ${cores} = "\"0-35\"" &&
    test ${gpus} = "\"0-1\""
'

# NOTE: this should never happen because on second DOWN, the reservation
# will be freed by the qmanager. But fluxion resource infrastructure
# is external driven so this condition can be tested.
test_expect_success 'status=up (sched-now=allocated sched-future=reserved)' '
    cat > cmds013 <<-EOF &&
	match allocate ${full_job}
	set-status /tiny0/rack0/node0 down
	match allocate_orelse_reserve ${sock1_job}
	set-status /tiny0/rack0/node1/socket1/core19 down
	find status=up (sched-now=allocated sched-future=reserved)
	quit
EOF
    ${query} -L ${grugs} -F rv1_nosched -S CA -P high -t mixed3.out < cmds013 &&
    cat mixed3.out | tail -n4 | grep -v "INFO" > mixed3.json &&
    cores=$(cat mixed3.json | jq " .execution.R_lite[].children.core ") &&
    gpus=$(cat mixed3.json | jq " .execution.R_lite[].children.gpu ") &&
    test ${cores} = "\"18,20-35\"" &&
    test ${gpus} = "\"1\""
'

test_expect_success 'status=up sched-now=allocated or sched-future=reserved)' '
    cat > cmds014 <<-EOF &&
	match allocate ${node1_job}
	match allocate ${sock1_job}
	set-status /tiny0/rack0/node1 down
	set-status /tiny0/rack0/node0/socket1 down
	match allocate ${sock1_job}
	match allocate_orelse_reserve ${sock1_job}
	set-status /tiny0/rack0/node0/socket1 up
	set-status /tiny0/rack0/node0/socket1/core19 down
	set-status /tiny0/rack0/node0/socket0/core16 down
	find (status=up and sched-now=allocated) or sched-future=reserved
	quit
EOF
    ${query} -L ${grugs} -F rv1_nosched -S CA -P high -t mixed4.out < cmds014 &&
    cat mixed4.out | tail -n4 | grep -v "INFO" > mixed4.json &&
    cores=$(cat mixed4.json | jq " .execution.R_lite[].children.core ") &&
    gpus=$(cat mixed4.json | jq " .execution.R_lite[].children.gpu ") &&
    test ${cores} = "\"0-18,20-35\"" &&
    test ${gpus} = "\"0-1\""
'

test_expect_success 'status=up (sched-now=allocated or sched-future=reserved)' '
    cat > cmds015 <<-EOF &&
	match allocate ${node1_job}
	match allocate ${sock1_job}
	set-status /tiny0/rack0/node1 down
	set-status /tiny0/rack0/node0/socket1 down
	match allocate_orelse_reserve ${sock1_job}
	set-status /tiny0/rack0/node0/socket1 up
	set-status /tiny0/rack0/node0/socket1/core19 down
	set-status /tiny0/rack0/node0/socket0/core16 down
	find status=up and (sched-now=allocated or sched-future=reserved)
	quit
EOF
    ${query} -L ${grugs} -F rv1_nosched -S CA -P high -t mixed5.out < cmds015 &&
    cat mixed5.out | tail -n4 | grep -v "INFO" > mixed5.json &&
    cores=$(cat mixed5.json | jq " .execution.R_lite[].children.core ") &&
    gpus=$(cat mixed5.json | jq " .execution.R_lite[].children.gpu ") &&
    test ${cores} = "\"0-15,17-18,20-35\"" &&
    test ${gpus} = "\"0-1\""
'

test_expect_success 'mix upper and lower cases for predicate values' '
    cat > cmds016 <<-EOF &&
	set-status /tiny0/rack0/node0 down
	find status=UP or status=Down
	quit
EOF
    ${query} -L ${grugs} -F rv1_nosched -S CA -P high -t down5.out < cmds016 &&
    cat down5.out | grep -v "INFO" > down5.json &&
    remove_times down5.json > down5.filt.json &&
    test_cmp down5.filt.json full.filt.json
'

test_expect_success 'find status=up works with simple writer' '
    cat > cmds017 <<-EOF &&
	set-status /tiny0/rack0/node0/socket0/core0 down
	find status=down
	quit
EOF
    ${query} -L ${grugs} -F simple -S CA -P high -t down17.out < cmds017 &&
    grep core0 down17.out &&
    grep node0 down17.out
'

test_expect_success 'find status=up works with pretty_simple writer' '
    cat > cmds018 <<-EOF &&
	set-status /tiny0/rack0/node0/socket0/core0 down
	find status=down
	quit
EOF
    ${query} -L ${grugs} -F pretty_simple -S CA -P high -t down18.out \
< cmds018 &&
    grep core0 down18.out &&
    grep node0 down18.out
'

test_expect_success 'find status=up works with jgf writer' '
    cat > cmds019 <<-EOF &&
	set-status /tiny0/rack0/node0/socket0/core0 down
	find status=down
	quit
EOF
    ${query} -L ${grugs} -F jgf -S CA -P high -t down19.out < cmds019  &&
    grep core0 down19.out &&
    grep node0 down19.out
'

test_expect_success 'find status=up works with rlite writer' '
    cat > cmds020 <<-EOF &&
	set-status /tiny0/rack0/node0/socket0/core0 down
	find status=down
	quit
EOF
    ${query} -L ${grugs} -F rlite -S CA -P high -t down20.out < cmds020 &&
    grep -v INFO down20.out > down20.filt.out &&
    core=$(cat down20.filt.out | jq " .[].children.core ") &&
    test ${core} = "\"0\""
'

test_expect_success 'setting vertex down in JGF same as set-status' '
    cat > cmds021 <<-EOF &&
	set-status /tiny0/rack0/node0/socket0/core0 down
	find status=down
	quit
EOF
    cat > cmds022 <<-EOF &&
	find status=down
	quit
EOF
    ${query} -L ${jgf} -f jgf -S CA -P high -t down21.out < cmds021 &&
    sed "/\"uniq_id\": 8,/a \ \ \ \ \ \ \ \ \ \ \"status\": 1," ${jgf} > down21.json &&
    ${query} -L ./down21.json -f jgf -S CA -P high -t down22.out < cmds022 &&
    test_cmp down21.out down22.out
'

test_expect_success 'find names=node[0-1] works' "
    cat > cmds023 <<-EOF &&
    find names=node[0-1]
    quit
EOF
    ${query} -L ${grugs} -F jgf -S CA -P high -t nodes.out < cmds023 &&
    cat nodes.out | grep -v INFO > nodes.json &&
    jq '.graph.nodes[].metadata | select(.type==\"node\")' nodes.json > nodes_selected.json &&
    grep node0 nodes_selected.json &&
    grep node1 nodes_selected.json
"

test_expect_success 'find names=node[0] works' "
    cat > cmds024 <<-EOF &&
    find names=node[0]
    quit
EOF
    ${query} -L ${grugs} -F jgf -S CA -P high -t nodes.out < cmds024 &&
    cat nodes.out | grep -v INFO > nodes2.json &&
    jq '.graph.nodes[].metadata | select(.type==\"node\")' nodes2.json > nodes_selected2.json &&
    grep node0 nodes_selected2.json &&
    test_must_fail grep node1 nodes_selected2.json
"

test_expect_success 'find names=node1 works' "
    cat > cmds025 <<-EOF &&
    find names=node1
    quit
EOF
    ${query} -L ${grugs} -F jgf -S CA -P high -t nodes.out < cmds025 &&
    cat nodes.out | grep -v INFO > nodes3.json &&
    jq '.graph.nodes[].metadata | select(.type==\"node\")' nodes3.json > nodes_selected3.json &&
    grep node1 nodes_selected3.json &&
    test_must_fail grep node0 nodes_selected3.json
"

test_expect_success 'find names=core[0-72] works' "
    cat > cmds026 <<-EOF &&
    find names=core[0-72]
    quit
EOF
    ${query} -L ${grugs} -F jgf -S CA -P high -t cores.out < cmds026 &&
    cat cores.out | grep -v INFO > cores.json &&
    jq '.graph.nodes[].metadata | select(.type==\"core\")' cores.json > cores_selected.json &&
    grep core1 cores_selected.json &&
    grep core0 cores_selected.json &&
    grep core19 cores_selected.json &&
    test_must_fail grep core71 cores_selected.json
"

test_expect_success 'find names=core[0-18] works' "
    cat > cmds027 <<-EOF &&
    find names=core[0-18]
    quit
EOF
    ${query} -L ${grugs} -F jgf -S CA -P high -t cores2.out < cmds027 &&
    cat cores2.out | grep -v INFO > cores2.json &&
    jq '.graph.nodes[].metadata | select(.type==\"core\")' cores2.json > cores_selected2.json &&
    grep core1 cores_selected2.json &&
    grep core18 cores_selected2.json &&
    test_must_fail grep core19 cores_selected2.json
"

test_expect_success 'find names=socket[af[] fails' "
    cat > cmds028 <<-EOF &&
    find names=socket[af[]
    quit
EOF
    ${query} -L ${grugs} -F jgf -S CA -P high < cmds028 2>&1 | grep 'invalid criteria'
"

test_done

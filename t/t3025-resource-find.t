#!/bin/sh

test_description='Test Resource Find On Tiny Machine Configuration'

. $(dirname $0)/sharness.sh

sock1_job="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/find/test001.yaml"
c1g1m1_job="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/find/test002.yaml"
node1_job="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/find/test003.yaml"
full_job="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/find/test004.yaml"
cmd_dir="${SHARNESS_TEST_SRCDIR}/data/resource/commands/find"
exp_dir="${SHARNESS_TEST_SRCDIR}/data/resource/expected/find"
grugs="${SHARNESS_TEST_SRCDIR}/data/resource/grugs/tiny.graphml"
query="../../resource/utilities/resource-query"

skip_all_unless_have jq

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
    diff up.filt.json full.filt.json
'

test_expect_success 'mark down 2 cores and find status=down' '
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
    nl=$(wc -l down3.out | awk "{ print \$1 }") &&
    test ${nl} -eq 3
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
    diff down4.filt.json full.filt.json
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
    diff alloc.1.filt.json alloc.2.filt.json
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

# NOTE: this should never happend because on second DOWN, the reservation
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


test_done

#!/bin/sh

test_description='Test emitted resource-set correctness as schema changes'

. $(dirname $0)/sharness.sh

skip_all_unless_have jq

full_job="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/RV/full.yaml"
exp_dir="${SHARNESS_TEST_SRCDIR}/data/resource/expected/RV"
jgf_dir="${SHARNESS_TEST_SRCDIR}/data/resource/jgfs"
jgf_orig="${jgf_dir}/4node.rank0123.nid0123.orig.json"
jgf_mod1="${jgf_dir}/4node.rank0123.nid0123.mod.json"
jgf_mod2="${jgf_dir}/4node.rank1230.nid0123.mod.json"
query="../../resource/utilities/resource-query"

# print_ranks_nodes <RV1 JSON filename>
print_ranks_nodes() {
    jq -r ".execution.R_lite[].rank, .execution.nodelist[0]" $1 | sort
}

test_expect_success 'RV1 is correct on rank and node ID order match' '
    cat > cmds001 <<-EOF &&
	match allocate ${full_job}
	quit
EOF
    ${query} -L ${jgf_orig} -f jgf -F rv1_nosched -t R1.out -P high < cmds001 &&
    test_cmp R1.out ${exp_dir}/R1.out
'

test_expect_success 'RV1 is correct on core ID modified' '
    cat > cmds002 <<-EOF &&
	match allocate ${full_job}
	quit
EOF
    ${query} -L ${jgf_mod1} -f jgf -F rv1_nosched -t R2.out -P high < cmds002 &&
    test_cmp R2.out ${exp_dir}/R2.out
'

test_expect_success 'RV1 correct on rank/node ID mismatch + core ID modified' '
    cat > cmds003 <<-EOF &&
	match allocate ${full_job}
	quit
EOF
    ${query} -L ${jgf_mod2} -f jgf -F rv1_nosched -t R3.out -P high < cmds003 &&
    test_cmp R3.out ${exp_dir}/R3.out
'

test_expect_success 'RV1 correct on heterogeneous configuration' '
    flux mini submit -n 28 --dry-run hostname > n28.json &&
    cat > cmds004 <<-EOF &&
        match allocate n28.json
        quit
EOF
    flux R encode -r 79-83 -c 0-3 -H fluke[82-86] > out4 &&
    flux R encode -r 91-92 -c 0-2 -H fluke[94-95] >> out4 &&
    flux R encode -r 97,99 -c 3 -H fluke[100,102] >> out4 &&
    cat out4 | flux R append > c4.json &&
    print_ranks_nodes c4.json > exp4 &&
    cat c4.json | flux ion-R encode > a4.json &&
    jq .scheduling a4.json > jgf4.json &&
    ${query} -L jgf4.json -f jgf -F rv1_nosched -t R4.out -P lonode < cmds004 &&
    grep -v INFO R4.out > R4.json &&
    print_ranks_nodes R4.json > res4 &&
    test_cmp exp4 res4
'

test_expect_success 'RV1 correct on heterogeneous configuration 2' '
    flux mini submit -n 14 --dry-run hostname > n14.json &&
    cat > cmds005 <<-EOF &&
        match allocate n14.json
        quit
EOF
    cat > exp5 <<-EOF &&
	79-81
	82
	fluke[82-85]
EOF
    ${query} -L jgf4.json -f jgf -F rv1_nosched -t R5.out -P lonode < cmds005 &&
    grep -v INFO R5.out > R5.json &&
    print_ranks_nodes R5.json > res5 &&
    test_cmp exp5 res5
'

test_expect_success 'RV1 correct on heterogeneous configuration 3' '
    cat > exp6 <<-EOF &&
	82
	83
	91-92
	97,99
	fluke[85-86,94-95,100,102]
EOF
    ${query} -L jgf4.json -f jgf -F rv1_nosched -t R6.out -P hinode < cmds005 &&
    grep -v INFO R6.out > R6.json &&
    print_ranks_nodes R6.json > res6 &&
    test_cmp exp6 res6
'

test_expect_success 'RV1 with nosched correct on heterogeneous configuration' '
    flux mini submit -n 28 --dry-run hostname > n28.json &&
    cat > cmds007 <<-EOF &&
        match allocate n28.json
        quit
EOF
    flux R encode -r 79-83 -c 0-3 -H fluke[82-86] > out7 &&
    flux R encode -r 91-92 -c 0-2 -H fluke[94-95] >> out7 &&
    flux R encode -r 97,99 -c 3 -H fluke[100,102] >> out7 &&
    cat out7 | flux R append > c7.json &&
    print_ranks_nodes c7.json > exp7 &&
    ${query} -L c7.json -f rv1exec -F rv1_nosched -t R7.out -P lonode < cmds007 &&
    grep -v INFO R7.out > R7.json &&
    print_ranks_nodes R7.json > res7 &&
    test_cmp exp7 res7
'

test_expect_success 'RV1 with nosched correct on heterogeneous config 2' '
    flux mini submit -n 14 --dry-run hostname > n14.json &&
    cat > cmds008 <<-EOF &&
        match allocate n14.json
        quit
EOF
    cat > exp8 <<-EOF &&
	79-81
	82
	fluke[82-85]
EOF
    ${query} -L c7.json -f rv1exec -F rv1_nosched -t R8.out -P lonode < cmds008 &&
    grep -v INFO R8.out > R8.json &&
    print_ranks_nodes R8.json > res8 &&
    test_cmp exp8 res8
'

test_expect_success 'RV1 with nosched correct on heterogeneous config 3' '
    cat > exp9 <<-EOF &&
	82
	83
	91-92
	97,99
	fluke[85-86,94-95,100,102]
EOF
    ${query} -L c7.json -f rv1exec -F rv1_nosched -t R9.out -P hinode < cmds008 &&
    grep -v INFO R9.out > R9.json &&
    print_ranks_nodes R9.json > res9 &&
    test_cmp exp9 res9
'

test_expect_success 'RV1 with nosched correct on nonconforming hostnames' '
    flux mini submit -n 8 --dry-run hostname > n8.json &&
    cat > cmds010 <<-EOF &&
        match allocate n8.json
        quit
EOF
    flux R encode -r 0-1 -c 0-3 -H foo,bar > c10.json &&
    print_ranks_nodes c10.json > exp10 &&
    ${query} -L c10.json -f rv1exec -F rv1_nosched -t R10.out -P lonode < cmds010 &&
    grep -v INFO R10.out > R10.json &&
    print_ranks_nodes R10.json > res10 &&
    test_cmp exp10 res10
'

test_expect_success 'RV1 with same hostnames work' '
    flux mini submit -n 8 --dry-run hostname > n8.json &&
    cat > cmds011 <<-EOF &&
	match allocate n8.json
	quit
EOF
    flux R encode -r 0-1 -c 0-3 -H fluke,fluke | flux ion-R encode > out11 &&
    print_ranks_nodes out11 > exp11 &&
    jq ".scheduling" out11 > c11.json &&
    ${query} -L c11.json -f jgf -F rv1_nosched -t R11.out -P lonode \
	< cmds011 &&
    grep -v INFO R11.out > R11.json &&
    print_ranks_nodes R11.json > res11 &&
    test_cmp exp11 res11
'

test_done

#!/bin/sh

test_description='Test emitted resource-set correctness as schema changes'

. $(dirname $0)/sharness.sh

full_job="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/RV/full.yaml"
duration3600="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/RV/duration3600.yaml"
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
	flux submit -n 28 --dry-run hostname > n28.json &&
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
	flux submit -n 14 --dry-run hostname > n14.json &&
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
	flux submit -n 28 --dry-run hostname > n28.json &&
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
	flux submit -n 14 --dry-run hostname > n14.json &&
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
	flux submit -n 8 --dry-run hostname > n8.json &&
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
	flux submit -n 8 --dry-run hostname > n8.json &&
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

test_expect_success 'Scheduling RV1 with high node num works (pol=lonode)' '
	flux submit -n 16 -c 94 --dry-run hostname > n94.json &&
	cat > cmds012 <<-EOF &&
	match allocate n94.json
	quit
	EOF
	flux R encode -r 0-15 -c 0-93 \
		-H fluke[1-10,4432,4832,5318,5761,6204,6647] > out12.json &&
	print_ranks_nodes out12.json > exp12 &&
	${query} -L out12.json -f rv1exec -F rv1_nosched \
		-t R12.out -P lonode < cmds012 &&
	grep -v INFO R12.out > R12.json &&
	print_ranks_nodes R12.json > res12 &&
	test_cmp exp12 res12
'

test_expect_success 'Scheduling RV1 with extreme node num works (pol=lonode)' '
	cat > cmds013 <<-EOF &&
	match allocate n94.json
	quit
	EOF
	flux R encode -r 0-15 -c 0-93 \
		-H fluke[1-10,4432,4832,5318,5761,6204000000,6647000000] \
		> out13.json &&
	print_ranks_nodes out13.json > exp13 &&
	${query} -L out13.json -f rv1exec -F rv1_nosched \
		-t R13.out -P lonode < cmds013 &&
	grep -v INFO R13.out > R13.json &&
	print_ranks_nodes R13.json > res13 &&
	test_cmp exp13 res13
'

test_expect_success 'Scheduling RV1-JGF with high node num works (pol=lonode)' '
	cat > cmds014 <<-EOF &&
	match allocate n94.json
	quit
	EOF
	flux R encode -r 0-15 -c 0-93 \
		-H fluke[1-10,4432,4832,5318,5761,6204,6647] | \
		flux ion-R encode > out14.json &&
	print_ranks_nodes out14.json > exp14 &&
	jq ".scheduling" out14.json > c14.json &&
	${query} -L c14.json -f jgf -F rv1_nosched \
		-t R14.out -P lonode < cmds014 &&
	grep -v INFO R14.out > R14.json &&
	print_ranks_nodes R14.json > res14 &&
	test_cmp exp14 res14
'

test_expect_success 'Scheduling RV1-JGF with extreme num work (pol=lonode)' '
	cat > cmds015 <<-EOF &&
	match allocate n94.json
	quit
	EOF
	flux R encode -r 0-15 -c 0-93 \
		-H fluke[1-10,4432,4832,5318,5761,6204000000,6647000000] | \
		flux ion-R encode > out15.json &&
	print_ranks_nodes out15.json > exp15 &&
	jq ".scheduling" out15.json > c15.json &&
	${query} -L c15.json -f jgf -F rv1_nosched \
		-t R15.out -P lonode < cmds015 &&
	grep -v INFO R15.out > R15.json &&
	print_ranks_nodes R15.json > res15 &&
	test_cmp exp15 res15
'

test_expect_success 'Scheduling RV1 with high node num works (pol=first)' '
	cat > cmds016 <<-EOF &&
	match allocate n94.json
	quit
	EOF
	flux R encode -r 0-15 -c 0-93 \
		-H fluke[1-10,4432,4832,5318,5761,6204,6647] > out16.json &&
	print_ranks_nodes out16.json > exp16 &&
	${query} -L out16.json -f rv1exec -F rv1_nosched \
		-t R16.out -P first < cmds016 &&
	grep -v INFO R16.out > R16.json &&
	print_ranks_nodes R16.json > res16 &&
	test_cmp exp16 res16
'

test_expect_success 'Scheduling RV1 with extreme node num works (pol=first)' '
	cat > cmds017 <<-EOF &&
	match allocate n94.json
	quit
	EOF
	flux R encode -r 0-15 -c 0-93 \
		-H fluke[1-10,4432,4832,5318,5761,6204000000,6647000000] \
		> out17.json &&
	print_ranks_nodes out17.json > exp17 &&
	${query} -L out17.json -f rv1exec -F rv1_nosched \
		-t R17.out -P first < cmds017 &&
	grep -v INFO R17.out > R17.json &&
	print_ranks_nodes R17.json > res17 &&
	test_cmp exp17 res17
'

test_expect_success 'node num > max(int64_t) must fail to load' '
	cat > cmds018 <<-EOF &&
	quit
	EOF
	flux R encode -r 0 -c 0-93 -H fluke1 | \
		jq " .execution.nodelist |= [\"fluke9223372036854775808\"]" \
		> out18.json &&
	print_ranks_nodes out18.json > exp18 &&
	test_must_fail ${query} -L out18.json -f rv1exec -F rv1_nosched < cmds018
'

test_expect_success 'Misconfigured RV1 is handled correctly' '
        cat > cmds019 <<-EOF &&
	quit
	EOF
	flux R encode -r 0 -c 0-1 -H fluke1 | \
		jq " .execution.R_lite[0].rank |= 0 " > out19.json &&
	test_must_fail ${query} -L out19.json -f rv1exec -F rv1_nosched \
		-t R19.out -P first < cmds019
'

test_expect_success 'Misconfigured RV1 is handled correctly 2' '
        cat > cmds020 <<-EOF &&
	quit
	EOF
	flux R encode -r 0-1 -c 0-1 -H fluke[1-0] | \
		jq " .execution.R_lite[0].rank |= \"0^2\" " > out20.json &&
	test_must_fail ${query} -L out20.json -f rv1exec -F rv1_nosched \
		-t R20.out -P first < cmds020
'

test_expect_success 'Jobs with too long of a duration are rejected' '
	cat > cmds021 <<-EOF &&
	match allocate ${duration3600}
	quit
	EOF
	flux R encode -r 0-1 -c 0-1 -H fluke[0-1] | \
		jq ".execution.starttime |= $(date +%s) | \
		.execution.expiration |= $(($(date +%s) + 20))" > out21.json &&
	${query} -L out21.json -f rv1exec -F rv1_nosched \
		-t R21.out -P first < cmds021 &&
	test_cmp R21.out ${exp_dir}/R21.out
'

test_done

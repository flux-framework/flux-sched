#!/bin/sh

test_description='Test emitted resource-set correctness as schema changes'

. $(dirname $0)/sharness.sh

full_job="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/RV/full.yaml"
exp_dir="${SHARNESS_TEST_SRCDIR}/data/resource/expected/RV"
jgf_dir="${SHARNESS_TEST_SRCDIR}/data/resource/jgfs"
jgf_orig="${jgf_dir}/4node.rank0123.nid0123.orig.json"
jgf_mod1="${jgf_dir}/4node.rank0123.nid0123.mod.json"
jgf_mod2="${jgf_dir}/4node.rank1230.nid0123.mod.json"
query="../../resource/utilities/resource-query"

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

test_done

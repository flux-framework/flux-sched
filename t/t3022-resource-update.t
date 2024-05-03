#!/bin/sh

test_description='Test Update Operation on Tiny Machine Configuration'

. $(dirname $0)/sharness.sh

unit_job="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/update/test001.yaml"
job2="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/update/test002.yaml"
job3="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/update/test003.yaml"
job4="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/update/test004.yaml"
job5="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/update/test007.yaml"
job6="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/update/test008.yaml"
job7="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/update/test009.json"
job8="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/update/test010.json"
job9="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/update/test011.json"
job10="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/update/test012.json"
job11="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/update/test013.json"
job12="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/update/test014.json"
exp_dir="${SHARNESS_TEST_SRCDIR}/data/resource/expected/update"
grugs="${SHARNESS_TEST_SRCDIR}/data/resource/grugs/tiny.graphml"
rv1exec_graph="${SHARNESS_TEST_SRCDIR}/data/resource/rv1exec/tiny_rv1exec.json"
rv1exec_graph_2_1="${SHARNESS_TEST_SRCDIR}/data/resource/rv1exec/tiny_rv1exec_2-1.json"
rv1exec_graph_2_1_split="${SHARNESS_TEST_SRCDIR}/data/resource/rv1exec/tiny_rv1exec_2-1_split.json"
query="../../src/resource/utilities/resource-query"

test001_desc="match-allocate/update-allocate result in same output"
test_expect_success "${test001_desc}" '
    cat >cmds001 <<-EOF &&
	match allocate ${unit_job}
	match allocate ${unit_job}
	match allocate ${unit_job}
	match allocate ${unit_job}
	quit
EOF
    ${query} -L ${grugs} -S CA -P high -F jgf -t 001.R.out < cmds001 &&
    cat 001.R.out | grep -v INFO | \
split -l 1 --additional-suffix=".json" - cmds001_ &&
    cat >upd_cmds001 <<-EOF &&
	update allocate jgf cmds001_aa.json 1 0 3600
	update allocate jgf cmds001_ab.json 2 0 3600
	update allocate jgf cmds001_ac.json 3 0 3600
	update allocate jgf cmds001_ad.json 4 0 3600
	quit
EOF
    ${query} -L ${grugs} -S CA -P high -F jgf -t 001.R.out2 < upd_cmds001 &&
    test_cmp 001.R.out2 001.R.out
'

test002_desc="mixed match-allocate/update-allocate works"
test_expect_success "${test002_desc}" '
    cat >cmds002 <<-EOF &&
	match allocate ${unit_job}
	match allocate ${unit_job}
	match allocate ${unit_job}
	match allocate ${unit_job}
	quit
EOF
    ${query} -L ${grugs} -S CA -P high -F jgf -t 002.R.out < cmds002 &&
    cat 002.R.out | grep -v INFO | \
split -l 1 --additional-suffix=".json" - cmds002_ &&
    cat >upd_cmds002 <<-EOF &&
	update allocate jgf cmds002_aa.json 1 0 3600
	match allocate ${unit_job}
	update allocate jgf cmds002_ac.json 3 0 3600
	match allocate ${unit_job}
	quit
EOF
    ${query} -L ${grugs} -S CA -P high -F jgf -t 002.R.out2 < upd_cmds002 &&
    test_cmp 002.R.out2 002.R.out
'

test003_desc="allocate_orelse_reserve/update-reserve result in same output"
test_expect_success "${test003_desc}" '
    cat >cmds003 <<-EOF &&
	match allocate_orelse_reserve ${unit_job}
	match allocate_orelse_reserve ${unit_job}
	match allocate_orelse_reserve ${unit_job}
	match allocate_orelse_reserve ${unit_job}
	match allocate_orelse_reserve ${unit_job}
	match allocate_orelse_reserve ${unit_job}
	match allocate_orelse_reserve ${unit_job}
	match allocate_orelse_reserve ${unit_job}
	match allocate_orelse_reserve ${unit_job}
	match allocate_orelse_reserve ${unit_job}
	quit
EOF
    ${query} -L ${grugs} -S CA -P high -F jgf -t 003.R.out < cmds003 &&
    cat 003.R.out | grep -v INFO | \
split -l 1 --additional-suffix=".json" - cmds003_ &&
    cat >upd_cmds003 <<-EOF &&
	update allocate jgf cmds003_aa.json 1 0 3600
	update allocate jgf cmds003_ab.json 2 0 3600
	update allocate jgf cmds003_ac.json 3 0 3600
	update allocate jgf cmds003_ad.json 4 0 3600
	update reserve jgf cmds003_ae.json 5 3600 3600
	update reserve jgf cmds003_af.json 6 3600 3600
	update reserve jgf cmds003_ag.json 7 3600 3600
	update reserve jgf cmds003_ah.json 8 3600 3600
	update reserve jgf cmds003_ai.json 9 7200 3600
	update reserve jgf cmds003_aj.json 10 7200 3600
	quit
	EOF
    ${query} -L ${grugs} -S CA -P high -F jgf -t 003.R.out2 < upd_cmds003 &&
    test_cmp 003.R.out2 003.R.out
'

test004_desc="mixed allocate_orelse_reserve/update-reserve works"
test_expect_success "${test004_desc}" '
    cat >cmds004 <<-EOF &&
	match allocate_orelse_reserve ${unit_job}
	match allocate_orelse_reserve ${unit_job}
	match allocate_orelse_reserve ${unit_job}
	match allocate_orelse_reserve ${unit_job}
	match allocate_orelse_reserve ${unit_job}
	match allocate_orelse_reserve ${unit_job}
	match allocate_orelse_reserve ${unit_job}
	match allocate_orelse_reserve ${unit_job}
	match allocate_orelse_reserve ${unit_job}
	match allocate_orelse_reserve ${unit_job}
	quit
EOF
    ${query} -L ${grugs} -S CA -P high -F jgf -t 004.R.out < cmds004 &&
    cat 004.R.out | grep -v INFO | \
split -l 1 --additional-suffix=".json" - cmds004_ &&
    cat >upd_cmds004 <<-EOF &&
	update allocate jgf cmds004_aa.json 1 0 3600
	update allocate jgf cmds004_ab.json 2 0 3600
	update allocate jgf cmds004_ac.json 3 0 3600
	match allocate_orelse_reserve ${unit_job}
	update reserve jgf cmds004_ae.json 5 3600 3600
	update reserve jgf cmds004_af.json 6 3600 3600
	match allocate_orelse_reserve ${unit_job}
	update reserve jgf cmds004_ah.json 8 3600 3600
	match allocate_orelse_reserve ${unit_job}
	update reserve jgf cmds004_aj.json 10 7200 3600
	quit
EOF
    ${query} -L ${grugs} -S CA -P high -F jgf -t 004.R.out2 < upd_cmds004 &&
    test_cmp 004.R.out2 004.R.out
'

test005_desc="cancel works with update"
test_expect_success "${test005_desc}" '
    cat >cmds005 <<-EOF &&
	match allocate_orelse_reserve ${unit_job}
	match allocate_orelse_reserve ${unit_job}
	match allocate_orelse_reserve ${unit_job}
	match allocate_orelse_reserve ${unit_job}
	match allocate_orelse_reserve ${unit_job}
	cancel 4
	match allocate_orelse_reserve ${unit_job}
	match allocate_orelse_reserve ${unit_job}
	match allocate_orelse_reserve ${unit_job}
	cancel 6
	match allocate_orelse_reserve ${unit_job}
	match allocate_orelse_reserve ${unit_job}
	quit
EOF
    ${query} -L ${grugs} -S CA -P high -F jgf -t 005.R.out < cmds005 &&
    cat 005.R.out | grep -v INFO | \
split -l 1 --additional-suffix=".json" - cmds005_ &&
    cat >upd_cmds005 <<-EOF &&
	update allocate jgf cmds005_aa.json 1 0 3600
	update allocate jgf cmds005_ab.json 2 0 3600
	update allocate jgf cmds005_ac.json 3 0 3600
	update allocate jgf cmds005_ad.json 4 0 3600
	update reserve jgf cmds005_ae.json 5 3600 3600
	cancel 4
	update reserve jgf cmds005_af.json 6 0 3600
	update reserve jgf cmds005_ag.json 7 3600 3600
	update reserve jgf cmds005_ah.json 8 3600 3600
	cancel 6
	update reserve jgf cmds005_ai.json 9 0 3600
	update reserve jgf cmds005_aj.json 10 3600 3600
	quit
EOF
    ${query} -L ${grugs} -S CA -P high -F jgf -t 005.R.out2 < upd_cmds005 &&
    test_cmp 005.R.out2 005.R.out
'

test006_desc="update with an existing jobid must fail"
test_expect_success "${test006_desc}" '
    cat >cmds006 <<-EOF &&
	match allocate ${unit_job}
	match allocate ${unit_job}
	match allocate ${unit_job}
	match allocate ${unit_job}
	cancel 1
	cancel 3
	match allocate ${unit_job}
	match allocate ${unit_job}
	quit
EOF
    ${query} -L ${grugs} -S CA -P high -F jgf -t 006.R.out < cmds006 &&
    cat 006.R.out | grep -v INFO | \
split -l 1 --additional-suffix=".json" - cmds006_ &&
    cat >upd_cmds006 <<-EOF &&
	match allocate ${unit_job}
	match allocate ${unit_job}
	match allocate ${unit_job}
	match allocate ${unit_job}
	update allocate jgf cmds006_aa.json 1 0 3600
	update allocate jgf cmds006_ab.json 2 0 3600
	update allocate jgf cmds006_ac.json 3 0 3600
	update allocate jgf cmds006_ad.json 4 0 3600
	cancel 1
	cancel 3
	match allocate ${unit_job}
	match allocate ${unit_job}
	quit
EOF
    ${query} -L ${grugs} -S CA -P high -F jgf -t 006.R.out2 < upd_cmds006 &&
    test_cmp 006.R.out2 006.R.out
'

test007_desc="update on already allocated resource sets must fail"
test_expect_success "${test007_desc}" '
    cat >cmds007 <<-EOF &&
	match allocate ${unit_job}
	match allocate ${unit_job}
	match allocate ${unit_job}
	match allocate ${unit_job}
	quit
EOF
    ${query} -L ${grugs} -S CA -P high -F jgf -t 007.R.out < cmds007 &&
    cat 007.R.out | grep -v INFO | \
split -l 1 --additional-suffix=".json" - cmds007_ &&
    cat >upd_cmds007 <<-EOF &&
	match allocate ${unit_job}
	match allocate ${unit_job}
	match allocate ${unit_job}
	match allocate ${unit_job}
	update allocate jgf cmds007_aa.json 5 0 3600
	update allocate jgf cmds007_ab.json 6 0 3600
	update allocate jgf cmds007_ac.json 7 0 3600
	update allocate jgf cmds007_ad.json 8 0 3600
	quit
EOF
    ${query} -L ${grugs} -S CA -P high -F jgf -t 007.R.out2 < upd_cmds007 &&
    cat 007.R.out | grep -v INFO > 007.R.out.filtered &&
    cat 007.R.out2 | grep -v INFO > 007.R.out2.filtered &&
    test_cmp 007.R.out2.filtered 007.R.out.filtered
'

test008_desc="update on partially allocated resources must fail"
test_expect_success "${test008_desc}" '
    cat >cmds008 <<-EOF &&
	match allocate ${unit_job}
	match allocate ${unit_job}
	match allocate ${unit_job}
	match allocate ${unit_job}
	quit
EOF
    ${query} -L ${grugs} -S CA -P high -F jgf -t 008.R.out < cmds008 &&
    cat >cmds008.2 <<-EOF &&
	match allocate ${job2}
	match allocate ${unit_job}
	match allocate ${unit_job}
	quit
EOF
    ${query} -L ${grugs} -S CA -P high -F jgf -t 008.2.R.out < cmds008.2 &&
    cat 008.2.R.out | grep -v INFO | \
split -l 1 --additional-suffix=".json" - cmds008_ &&
    cat >upd_cmds008 <<-EOF &&
	match allocate ${unit_job}
	update allocate jgf cmds008_aa.json 2 0 3600
	match allocate ${unit_job}
	match allocate ${unit_job}
	match allocate ${unit_job}
	quit
EOF
    ${query} -L ${grugs} -S CA -P high -F jgf -t 008.R.out2 < upd_cmds008 &&
    cat 008.R.out | grep -v INFO > 008.R.out.filtered &&
    cat 008.R.out2 | grep -v INFO > 008.R.out2.filtered &&
    test_cmp 008.R.out2.filtered 008.R.out.filtered &&
    cat >upd_cmds008.2 <<-EOF &&
	update allocate jgf cmds008_aa.json 1 0 3600
	match allocate ${unit_job}
	match allocate ${unit_job}
	quit
EOF
    ${query} -L ${grugs} -S CA -P high -F jgf -t 008.2.R.out2 < upd_cmds008.2 &&
    cat 008.2.R.out | grep -v INFO > 008.2.R.out.filtered &&
    cat 008.2.R.out2 | grep -v INFO > 008.2.R.out2.filtered &&
    test_cmp 008.2.R.out2.filtered 008.2.R.out.filtered
'

test009_desc="update shared resource must fail when it is already allocated"
test_expect_success "${test009_desc}" '
    cat >cmds009 <<-EOF &&
	match allocate ${unit_job}
	match allocate ${unit_job}
	match allocate ${unit_job}
	match allocate ${unit_job}
	quit
EOF
    ${query} -L ${grugs} -S CA -P high -F jgf -t 009.R.out < cmds009 &&
    cat 009.R.out | grep -v INFO | \
split -l 1 --additional-suffix=".json" - cmds009_ &&
    cat >cmds009.2 <<-EOF &&
	match allocate ${job3}
	match allocate ${unit_job}
	match allocate ${unit_job}
	match allocate ${unit_job}
	match allocate ${unit_job}
	quit
EOF
    ${query} -L ${grugs} -S CA -P high -F jgf -t 009.2.R.out < cmds009.2 &&
    cat >upd_cmds009 <<-EOF &&
	match allocate ${job3}
	update allocate jgf cmds009_aa.json 2 0 3600
	update allocate jgf cmds009_ab.json 3 0 3600
	update allocate jgf cmds009_ac.json 4 0 3600
	update allocate jgf cmds009_ad.json 5 0 3600
	quit
EOF
    ${query} -L ${grugs} -S CA -P high -F jgf -t 009.R.out2 < upd_cmds009 &&
    cat 009.2.R.out | grep -v INFO > 009.2.R.out.filtered &&
    cat 009.R.out2 | grep -v INFO > 009.R.out2.filtered &&
    test_cmp 009.R.out2.filtered 009.2.R.out.filtered
'

test010_desc="update-allocate work for complex resource shape (cpu+gpu+memory)"
test_expect_success "${test010_desc}" '
    cat >cmds010 <<-EOF &&
	match allocate ${job4}
	match allocate ${job4}
	quit
EOF
    ${query} -L ${grugs} -S CA -P high -F jgf -t 010.R.out < cmds010 &&
    cat 010.R.out | grep -v INFO | \
split -l 1 --additional-suffix=".json" - cmds010_ &&
    cat >upd_cmds010 <<-EOF &&
	update allocate jgf cmds010_aa.json 1 0 3600
	update allocate jgf cmds010_ab.json 2 0 3600
	quit
EOF
    ${query} -L ${grugs} -S CA -P high -F jgf -t 010.R.out2 < upd_cmds010 &&
    test_cmp 010.R.out2 010.R.out
'

test011_desc="match-allocate/update-allocate result in same output in RV1"
test_expect_success "${test011_desc}" '
    cat >cmds011 <<-EOF &&
        match allocate ${unit_job}
        match allocate ${unit_job}
        match allocate ${unit_job}
        match allocate ${unit_job}
        quit
EOF
    ${query} -L ${grugs} -S CA -P high -F rv1 -t 011.R.out < cmds011 &&
    cat 011.R.out | grep -v INFO | \
split -l 1 --additional-suffix=".json" - cmds011_ &&
    cat >upd_cmds011 <<-EOF &&
        update allocate jgf cmds001_aa.json 1 0 3600
        update allocate jgf cmds001_ab.json 2 0 3600
        update allocate jgf cmds001_ac.json 3 0 3600
        update allocate jgf cmds001_ad.json 4 0 3600
        quit
EOF
    ${query} -L ${grugs} -S CA -P high -F rv1 -t 011.R.out2 < upd_cmds011 &&
    test_cmp 011.R.out2 011.R.out
'

test012_desc="match-allocate/update-allocate result in same output for rv1exec"
test_expect_success "${test012_desc}" '
    cat >cmds012 <<-EOF &&
	match allocate ${job5}
	match allocate ${job5}
	match allocate ${job5}
	match allocate ${job5}
	quit
EOF
    ${query} -L ${rv1exec_graph} -f rv1exec -S CA -P high -F rv1_nosched -t 012.R.out < cmds012 &&
    cat 012.R.out | grep -v INFO | \
split -l 1 --additional-suffix=".json" - cmds012_ &&
    cat >upd_cmds012 <<-EOF &&
	update allocate rv1exec cmds012_aa.json 1 0 3600
	update allocate rv1exec cmds012_ab.json 2 0 3600
	update allocate rv1exec cmds012_ac.json 3 0 3600
	update allocate rv1exec cmds012_ad.json 4 0 3600
	quit
EOF
    ${query} -L ${rv1exec_graph} -f rv1exec -S CA -P high -F rv1_nosched -t 012.R.out2 < upd_cmds012 &&
    test_cmp 012.R.out2 012.R.out
'

test013_desc="match-allocate/update-allocate result in same output for rv1exec with gpus"
test_expect_success "${test013_desc}" '
    cat >cmds013 <<-EOF &&
	match allocate ${job6}
	match allocate ${job6}
	match allocate ${job6}
	match allocate ${job6}
	quit
EOF
    ${query} -L ${rv1exec_graph} -f rv1exec -S CA -P high -F rv1_nosched -t 013.R.out < cmds013 &&
    cat 013.R.out | grep -v INFO | \
split -l 1 --additional-suffix=".json" - cmds013_ &&
    cat >upd_cmds013 <<-EOF &&
	update allocate rv1exec cmds013_aa.json 1 0 3600
	update allocate rv1exec cmds013_ab.json 2 0 3600
	update allocate rv1exec cmds013_ac.json 3 0 3600
	update allocate rv1exec cmds013_ad.json 4 0 3600
	quit
EOF
    ${query} -L ${rv1exec_graph} -f rv1exec -S CA -P high -F rv1_nosched -t 013.R.out2 < upd_cmds013 &&
    test_cmp 013.R.out2 013.R.out
'

test014_desc="update-allocate with rv1exec correctly allocates exclusively with 2 ranks/host"
test_expect_success "${test014_desc}" '
    cat >cmds014 <<-EOF &&
	update allocate rv1exec ${job7} 1 0 3600
	update allocate rv1exec ${job8} 2 0 3600
	find sched-now=free
	quit
EOF
    ${query} -L ${rv1exec_graph_2_1} -f rv1exec -S CA -P high -t 014.R.out < cmds014 &&
    test_cmp 014.R.out ${exp_dir}/014.R.out
'

test015_desc="update-allocate with rv1exec correctly allocates exclusively with 2 ranks/host and split resources"
test_expect_success "${test015_desc}" '
    cat >cmds015 <<-EOF &&
	update allocate rv1exec ${job9} 1 0 3600
	find sched-now=free
	quit
EOF
    ${query} -L ${rv1exec_graph_2_1_split} -f rv1exec -S CA -P high -t 015.R.out < cmds015 &&
    test_cmp 015.R.out ${exp_dir}/015.R.out
'

test016_desc="failed update-allocate exhibits correct rollback"
test_expect_success "${test015_desc}" '
    cat >cmds016 <<-EOF &&
	update allocate rv1exec ${job10} 1 0 3600
	update allocate rv1exec ${job11} 2 0 3600
	update allocate rv1exec ${job12} 3 0 3600
	find sched-now=free
	quit
EOF
    ${query} -L ${rv1exec_graph} -f rv1exec -S CA -P high -t 016.R.out < cmds016 &&
    test_cmp 016.R.out ${exp_dir}/016.R.out
'

test_done


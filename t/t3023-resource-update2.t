#!/bin/sh

test_description='Test Update Operation on Tiny Machine Configuration'

. $(dirname $0)/sharness.sh

exp_dir="${SHARNESS_TEST_SRCDIR}/data/resource/expected/update_multi"
unit_job="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/update/test001.yaml"
job5="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/update/test005.yaml"
job6="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/update/test006.yaml"
grugs="${SHARNESS_TEST_SRCDIR}/data/resource/grugs/power.graphml"
query="../../resource/utilities/resource-query"

test001_desc="match-allocate/update-allocate works when CA is selected"
test_expect_success "${test001_desc}" '
    cat >cmds001 <<-EOF &&
	match allocate ${job6}
	match allocate ${job6}
	quit
EOF
    ${query} -L ${grugs} -S CA -P high -F jgf -t 001.R.out < cmds001 &&
    cat 001.R.out | grep -v INFO | \
split -l 1 --additional-suffix=".json" - cmds001_ &&
    cat >upd_cmds001 <<-EOF &&
	update allocate jgf cmds001_aa.json 1 0 3600
	update allocate jgf cmds001_ab.json 2 0 3600
	quit
EOF
    ${query} -L ${grugs} -S CA -P high -F jgf -t 001.R.out2 < upd_cmds001 &&
    test_cmp 001.R.out2 001.R.out
'

test002_desc="match-allocate/update-allocate works when PA is selected"
test_expect_success "${test002_desc}" '
    cat >cmds002 <<-EOF &&
	match allocate ${job5}
	match allocate ${job5}
	quit
EOF
    ${query} -L ${grugs} -S PA -P high -F jgf -t 002.R.out < cmds002 &&
    cat 002.R.out | grep -v INFO | \
split -l 1 --additional-suffix=".json" - cmds002_ &&
    cat >upd_cmds002 <<-EOF &&
	update allocate jgf cmds002_aa.json 1 0 3600
	update allocate jgf cmds002_ab.json 2 0 3600
	quit
EOF
    ${query} -L ${grugs} -S PA -P high -F jgf -t 002.R.out2 < upd_cmds002 &&
    test_cmp 002.R.out2 002.R.out
'

test003_desc="update-allocate works when dom subsystem changes from CA to PA"
test_expect_success "${test003_desc}" '
    cat >cmds003 <<-EOF &&
	match allocate ${job6}
	match allocate ${job6}
	quit
EOF
    ${query} -L ${grugs} -S CA -P high -F jgf -t 003.R.out < cmds003 &&
    cat 003.R.out | grep -v INFO | \
split -l 1 --additional-suffix=".json" - cmds003_ &&
    cat >upd_cmds003 <<-EOF &&
	update allocate jgf cmds003_aa.json 1 0 3600
	match allocate ${job5}
	match allocate ${job5}
	quit
EOF
    ${query} -L ${grugs} -S PA -P high -t 003.R.out2 < upd_cmds003 &&
    test_cmp 003.R.out2 ${exp_dir}/003.R.out
'

test004_desc="cancel works when dom subsystem changes from CA to PA"
test_expect_success "${test004_desc}" '
    cat >cmds004 <<-EOF &&
	match allocate ${job6}
	match allocate ${job6}
	quit
EOF
    ${query} -L ${grugs} -S CA -P high -F jgf -t 004.R.out < cmds004 &&
    cat 004.R.out | grep -v INFO | \
split -l 1 --additional-suffix=".json" - cmds004_ &&
    cat >upd_cmds004 <<-EOF &&
	update allocate jgf cmds004_aa.json 1 0 3600
	match allocate ${job5}
	match allocate ${job5}
	cancel 1
	match allocate ${job5}
	quit
EOF
    ${query} -L ${grugs} -S PA -P high -t 004.R.out2 < upd_cmds004 &&
    test_cmp 004.R.out2 ${exp_dir}/004.R.out
'

test_done


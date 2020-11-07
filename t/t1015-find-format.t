#!/bin/sh

test_description='Test Resource Find and Status with Fluxion Modules'

. `dirname $0`/sharness.sh

hwloc_basepath=`readlink -e ${SHARNESS_TEST_SRCDIR}/data/hwloc-data`
expected_basepath=`readlink -e ${SHARNESS_TEST_SRCDIR}/data/resource/expected/find-format`
excl_1N1B="${hwloc_basepath}/001N/exclusive/01-brokers/"
query="../../resource/utilities/resource-query"

skip_all_unless_have jq

normalize_json() {
    jq 'del (.execution.starttime) | del (.execution.expiration)' |
        grep -v '"rank":'
}

strip_info_lines() {
    grep -v "INFO:"
}

test_under_flux 1

test_expect_success 'find format: find query simple works' '
    cat > find.cmd <<-EOF &&
	find status=up
	quit
EOF
    ${query} -d -L ${excl_1N1B}/0.xml -f hwloc -F simple -S CA -P high \
             -W node,core -t simple.query.raw < find.cmd &&
    strip_info_lines < simple.query.raw > simple.query.out &&
    test_cmp simple.query.out ${expected_basepath}/simple.expected.out
'

test_expect_success 'find format: find query rv1_nosched works' '
    ${query} -d -L ${excl_1N1B}/0.xml -f hwloc -F rv1_nosched -S CA -P high \
             -W node,core -t rv1_nosched.query.raw < find.cmd &&
    strip_info_lines < rv1_nosched.query.raw | \
             normalize_json > rv1_nosched.query.json &&
    test_cmp rv1_nosched.query.json ${expected_basepath}/rv1_nosched.expected.json
'

test_expect_success 'find format: find query rv1 works' '
    ${query} -d -L ${excl_1N1B}/0.xml -f hwloc -F rv1 -S CA -P high \
             -W node,core -t rv1.query.raw < find.cmd &&
    strip_info_lines < rv1.query.raw | normalize_json > rv1.query.json &&
    test_cmp rv1.query.json ${expected_basepath}/rv1.expected.json
'

test_expect_success 'find format: find query jgf works' '
    ${query} -d -L ${excl_1N1B}/0.xml -f hwloc -F jgf -S CA -P high \
             -W node,core -t jgf.query.raw < find.cmd &&
    strip_info_lines < jgf.query.raw | normalize_json > jgf.query.json &&
    test_cmp jgf.query.json ${expected_basepath}/jgf.expected.json
'

test_expect_success 'find/status: reloading resources works' '
    flux module remove -f sched-simple &&
    load_test_resources ${excl_1N1B}
'

test_expect_success 'find/status: loading fluxion modules works' '
    load_resource load-allowlist=cluster,node,core &&
    load_qmanager queue-policy=easy
'

test_expect_success 'find/status: find status=up format=simple works' '
    flux ion-resource find --format=simple status=up \
         > simple.ion.raw &&
    tail -n1 simple.ion.raw | xargs printf > simple.ion.out &&
    test_cmp simple.query.out simple.ion.out
'

test_expect_success 'find/status: find status=up format=rv1_nosched works' '
    flux ion-resource find --format=rv1_nosched status=up \
         > rv1_nosched.raw.raw &&
    flux ion-resource find --format=rv1_nosched status=up \
         | tail -1 > rv1_nosched.raw.json &&
    normalize_json < rv1_nosched.raw.json > rv1_nosched.json &&
    test_cmp rv1_nosched.query.json rv1_nosched.json
'

test_expect_success 'find/status: find status=up format=rv1 works' '
    flux ion-resource find --format=rv1 status=up \
         | tail -1 > rv1.raw.json &&
    normalize_json < rv1.raw.json > rv1.json &&
    test_cmp rv1.query.json rv1.json
'

test_expect_success 'find/status: find status=up format=jgf works' '
    flux ion-resource find --format=jgf status=up \
         | tail -1 > jgf.raw.json &&
    normalize_json < jgf.raw.json > jgf.json &&
    test_cmp jgf.query.json jgf.json
'

test_expect_success 'find/status: removing fluxion modules' '
    remove_qmanager &&
    remove_resource
'

# Reload the core scheduler so that rc3 won't hang waiting for
# queue to become idle after jobs are canceled.
test_expect_success 'find/status: load sched-simple module' '
    flux module load sched-simple
'

test_done


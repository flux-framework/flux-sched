#!/bin/sh

test_description='Test Correctness of Match Emit Format'

. `dirname $0`/sharness.sh

tiny_grug="${SHARNESS_TEST_SRCDIR}/data/resource/grugs/tiny.graphml"
large_grug="${SHARNESS_TEST_SRCDIR}/data/resource/grugs/sierra.graphml"
jobspec="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/basics/test001.yaml"
jobspec2="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/basics/test011.yaml"
schema="${SHARNESS_TEST_SRCDIR}/schemas/json-graph-schema.json"
query="../../resource/utilities/resource-query"

test_expect_success "R emitted with -F jgf validates against schema" '
    echo "match allocate ${jobspec}" > in1.txt &&
    echo "quit" >> in1.txt &&
    ${query} -L ${tiny_grug} -F jgf -d -t o1 -P high < in1.txt &&
    cat o1 | grep -v "INFO:" > o1.json &&
    flux jsonschemalint -v ${schema} o1.json
'

test_expect_success LONGTEST "Large R emitted with -F jgf validates" '
    echo "match allocate ${jobspec2}" > in3.txt &&
    echo "quit" >> in3.txt &&
    ${query} -L ${large_grug} -r 400000 -F jgf -d -t o3 -P high < in3.txt &&
    cat o3 | grep -v "INFO:" > o3.json &&
    flux jsonschemalint -v ${schema} o3.json
'

test_expect_success "--match-format=rv1 works" '
    echo "match allocate ${jobspec}" > in5.txt &&
    echo "quit" >> in5.txt &&
    ${query} -L ${tiny_grug} -F rv1 -d -t o5 -P high < in5.txt &&
    cat o5 | grep -v "INFO:" | jq ".scheduling" > o5.json &&
    flux jsonschemalint -v ${schema} o5.json
'

test_expect_success "--match-format=rv1_nosched and =rlite works" '
    echo "match allocate ${jobspec}" > in7.txt &&
    echo "quit" >> in7.txt &&
    ${query} -L ${tiny_grug} -F rv1_nosched -d -t o7 -P high < in7.txt &&
    ${query} -L ${tiny_grug} -F rlite -d -t o8 -P high < in7.txt &&
    cat o7 | grep -v "INFO:" | jq ".execution.R_lite" > o7.json &&
    cat o8 | grep -v "INFO:" | jq "." > o8.json &&
    diff o7.json o8.json
'

test_expect_success "--match-format=pretty_simple works" '
    echo "match allocate ${jobspec}" > in9.txt &&
    echo "quit" >> in9.txt &&
    ${query} -L ${tiny_grug} -F pretty_simple -d -t o9 -P high < in9.txt &&
    cat o9 | grep -v "INFO:" > o9.simple &&
    test -s o9.simple
'

test_expect_success "rv1 contains starttime and expiration keys" '
    echo "match allocate ${jobspec}" > in10.txt &&
    echo "quit" >> in10.txt &&
    ${query} -L ${tiny_grug} -F rv1_nosched -d -t o10 -P high < in10.txt &&
    starttime=$(cat o10 | grep -v "INFO:" | jq ".execution.starttime") &&
    expiration=$(cat o10 | grep -v "INFO:" | jq ".execution.expiration") &&
    test ${starttime} -eq 0 &&
    test ${expiration} -eq 3600
'

test_expect_success "rv1_nosched contains starttime and expiration keys" '
    echo "match allocate ${jobspec}" > in11.txt &&
    echo "quit" >> in11.txt &&
    ${query} -L ${tiny_grug} -F rv1 -d -t o11 -P high < in11.txt &&
    starttime=$(cat o11 | grep -v "INFO:" | jq ".execution.starttime") &&
    expiration=$(cat o11 | grep -v "INFO:" | jq ".execution.expiration") &&
    test ${starttime} -eq 0 &&
    test ${expiration} -eq 3600
'

test_done

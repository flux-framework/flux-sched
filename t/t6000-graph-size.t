#!/bin/sh

test_description='Test Correctness of Populating Graph Data Store'

. $(dirname $0)/sharness.sh

tiny_grug="${SHARNESS_TEST_SRCDIR}/data/resource/grugs/tiny.graphml"
large_grug="${SHARNESS_TEST_SRCDIR}/data/resource/grugs/sierra.graphml"
query="../../resource/utilities/resource-query"

test_expect_success "vertex/edge counts for a tiny machine are correct" '
    echo "quit" >> input1.txt &&
    ${query} -G ${tiny_grug} -e -S CA -P high < input1.txt > out1.txt &&
    vtx=$(grep "Vertex" out1.txt  | sed "s/INFO: Vertex Count: //") &&
    edg=$(grep "Edge" out1.txt  | sed "s/INFO: Edge Count: //") &&
    test ${vtx} -eq 100 &&
    test ${edg} -eq 198
'

test_expect_success "--reserve-vtx-vec works" '
    echo "quit" >> input2.txt &&
    ${query} -G ${tiny_grug} -e -r 1024 -S CA -P high < input2.txt > out2.txt &&
    vtx=$(grep "Vertex" out2.txt  | sed "s/INFO: Vertex Count: //") &&
    edg=$(grep "Edge" out2.txt  | sed "s/INFO: Edge Count: //") &&
    test ${vtx} -eq 100 &&
    test ${edg} -eq 198
'

test_expect_success LONGTEST "--reserve-vtx-vec improves loading performance" '
    echo "quit" >> input3.txt &&
    ${query} -G ${large_grug} -e -r 400000 < input3.txt > out3A.txt &&
    ${query} -G ${large_grug} -e < input3.txt > out3B.txt &&
    with=$(grep "Graph" out3A.txt  | sed "s/INFO: Graph Load Time: //") &&
    without=$(grep "Graph" out3B.txt  | sed "s/INFO: Graph Load Time: //") &&
    test $(awk "BEGIN{ print $with<$without }") -eq 1
'


test_done

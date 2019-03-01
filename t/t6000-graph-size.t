#!/bin/sh

test_description='Test Correctness of Populating Graph Data Store'

. $(dirname $0)/sharness.sh

tiny_grug="${SHARNESS_TEST_SRCDIR}/data/resource/grugs/tiny.graphml"
query="../../resource/utilities/resource-query"

test_expect_success "vertex/edge counts for a tiny machine are correct" '
    echo "quit" >> input1.txt &&
    ${query} -G ${tiny_grug} -e -S CA -P high < input1.txt > out1.txt &&
    vtx=$(grep "Vertex" out1.txt  | sed "s/INFO: Vertex Count: //") &&
    edg=$(grep "Edge" out1.txt  | sed "s/INFO: Edge Count: //") &&
    test ${vtx} -eq 100 &&
    test ${edg} -eq 198
'

test_done

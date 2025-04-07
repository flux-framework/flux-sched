#!/bin/sh

test_description='Test Correctness of Populating Graph Data Store'

. $(dirname $0)/sharness.sh

tiny_grug="${SHARNESS_TEST_SRCDIR}/data/resource/grugs/tiny.graphml"
tiny_jgf="${SHARNESS_TEST_SRCDIR}/data/resource/jgfs/tiny.json"
exclusive_001N_hwloc="${SHARNESS_TEST_SRCDIR}/data/hwloc-data/001N/exclusive\
/04-brokers/0.xml"
large_grug="${SHARNESS_TEST_SRCDIR}/data/resource/grugs/sierra.graphml"
query="../../resource/utilities/resource-query"

test_expect_success "vertex/edge counts for a tiny machine are correct" '
    echo "quit" > input1.txt &&
    ${query} -L ${tiny_grug} -e -S CA -P high < input1.txt > out1.txt &&
    vtx=$(grep "Vertex" out1.txt  | sed "s/INFO: Vertex Count: //") &&
    edg=$(grep "Edge" out1.txt  | sed "s/INFO: Edge Count: //") &&
    test ${vtx} -eq 100 &&
    test ${edg} -eq 99
'

# Note that by default the rank is -1, meaning that the by_rank map 
# contains the same number of vertices as the overall resource graph. 
test_expect_success "by_type, by_name, by_path, and by_rank map sizes are \
correct for GRUG" '
    echo "quit" > input2.txt &&
    ${query} -L ${tiny_grug} -e -S CA -P high < input2.txt > out2.txt &&
    by_type=$(grep "by_type" out2.txt | sed "s/INFO: by_type Key-Value \
Pairs: //") &&
    by_name=$(grep "by_name" out2.txt | sed "s/INFO: by_name Key-Value \
Pairs: //") &&
    by_path=$(grep "by_path" out2.txt | sed "s/INFO: by_path Key-Value \
Pairs: //") &&
    by_rank=$(grep "number of" out2.txt | sed "s/INFO: number of \
vertices with rank //") &&
    rank=$( echo ${by_rank} | sed "s/:.*//" ) &&
    nvertices=$( echo ${by_rank} | sed "s/[^:]*://" ) &&
    test ${by_type} -eq 7 &&
    test ${by_name} -eq 52 &&
    test ${by_path} -eq 100 &&
    test ${rank} -eq -1 &&
    test ${nvertices} -eq 100
'

# Note that by default the rank is -1, meaning that the by_rank map 
# contains the same number of vertices as the overall resource graph. 
test_expect_success "by_type, by_name, by_path, and by_rank map sizes are \
correct for JGF." '
    echo "quit" > input3.txt &&
    ${query} -L ${tiny_jgf} -e -S CA -P high -f jgf < input3.txt > \
out3.txt &&
    by_type=$(grep "by_type" out3.txt | sed "s/INFO: by_type Key-Value \
Pairs: //") &&
    by_name=$(grep "by_name" out3.txt | sed "s/INFO: by_name Key-Value \
Pairs: //") &&
    by_path=$(grep "by_path" out3.txt | sed "s/INFO: by_path Key-Value \
Pairs: //") &&
    by_rank=$(grep "number of" out3.txt | sed "s/INFO: number of \
vertices with rank //") &&
    rank=$( echo ${by_rank} | sed "s/:.*//" ) &&
    nvertices=$( echo ${by_rank} | sed "s/[^:]*://" ) &&
    test ${by_type} -eq 7 &&
    test ${by_name} -eq 52 &&
    test ${by_path} -eq 100 &&
    test ${rank} -eq -1 &&
    test ${nvertices} -eq 100
'

# Note that by default the rank is -1, meaning that the by_rank map 
# contains the same number of vertices as the overall resource graph. 
test_expect_success "by_type, by_name, by_path, and by_rank map sizes are \
correct for hwloc" '
    echo "quit" > input4.txt &&
    ${query} -L ${exclusive_001N_hwloc} -W core,socket,node -e -S CA -P high \
-f hwloc < input4.txt > out4.txt &&
    by_type=$(grep "by_type" out4.txt | sed "s/INFO: by_type Key-Value \
Pairs: //") &&
    by_name=$(grep "by_name" out4.txt | sed "s/INFO: by_name Key-Value \
Pairs: //") &&
    by_path=$(grep "by_path" out4.txt | sed "s/INFO: by_path Key-Value \
Pairs: //") &&
    by_rank=$(grep "number of" out4.txt | sed "s/INFO: number of \
vertices with rank //") &&
    rank=$( echo ${by_rank} | sed "s/:.*//" ) &&
    nvertices=$( echo ${by_rank} | sed "s/[^:]*://" ) &&
    test ${by_type} -eq 4 &&
    test ${by_name} -eq 7 &&
    test ${by_path} -eq 7 &&
    test ${rank} -eq -1 &&
    test ${nvertices} -eq 7
'

test_expect_success "--reserve-vtx-vec works" '
    echo "quit" > input5.txt &&
    ${query} -L ${tiny_grug} -e -r 1024 -S CA -P high < input5.txt > \
out5.txt &&
    vtx=$(grep "Vertex" out5.txt | sed "s/INFO: Vertex Count: //") &&
    edg=$(grep "Edge" out5.txt | sed "s/INFO: Edge Count: //") &&
    test ${vtx} -eq 100 &&
    test ${edg} -eq 99
'

test_expect_success LONGTEST "--reserve-vtx-vec improves loading performance" '
    echo "quit" > input6.txt &&
    ${query} -L ${large_grug} -e -r 400000 -P high < input6.txt > out6A.txt &&
    ${query} -L ${large_grug} -e -P high < input6.txt > out6B.txt &&
    with=$(grep "Graph" out6A.txt | sed "s/INFO: Graph Load Time: //") &&
    without=$(grep "Graph" out6B.txt | sed "s/INFO: Graph Load Time: //") &&
    test $(awk "BEGIN{ print $with<$without }") -eq 1
'


test_done

#!/bin/sh

test_description='Test matching the storage_node type'

. $(dirname $0)/sharness.sh

query="../../resource/utilities/resource-query"
jgf="${SHARNESS_TEST_SRCDIR}/data/resource/jgfs/rabbit.json"

test_expect_success 'resource-query rejects jobspec asking for a node if only storage_node matches' '
    flux run -N1 -n1 --requires=hosts:hetchy201 --dry hostname | jq . > node_requires_host.json &&
    cat > query_cmd1 <<-EOF &&
m allocate node_requires_host.json
quit
EOF
    ${query} -L ${jgf} -f jgf < query_cmd1 > out1 &&
    grep "No matching resources" out1
'

test_expect_success 'resource-query allocates storage node if jobspec only asks for cores' '
    flux run -n1 --requires=hosts:hetchy201 --dry hostname | jq . \
        > core_requires_host.json &&
    cat > query_cmd2 <<-EOF &&
m allocate core_requires_host.json
quit
EOF
    ${query} -L ${jgf} -f jgf < query_cmd2 > out2 &&
    test_must_fail grep "No matching resources" out2 &&
    grep hetchy201 out2
'

test_expect_success 'resource-query rejects storage node if it fails property constraint' '
    flux run -n1 --requires="hosts:hetchy201 and parrypeak" --dry hostname  | jq . \
        > core_requires_host_and_parrypeak.json &&
    cat > query_cmd3 <<-EOF &&
m allocate core_requires_host_and_parrypeak.json
quit
EOF
    ${query} -L ${jgf} -f jgf < query_cmd3 > out3 &&
    grep "No matching resources" out3
'

test_expect_success 'resource-query accepts storage node with or-rabbit property constraint' '
    flux run -n1 --requires="hosts:hetchy201 and (parrypeak or rabbit)" --dry hostname  | jq .\
        > core_requires_host_and_parrypeak_or_rabbit.json &&
    cat > query_cmd4 <<-EOF &&
m allocate core_requires_host_and_parrypeak_or_rabbit.json
quit
EOF
    ${query} -L ${jgf} -f jgf < query_cmd4 >out4 &&
    test_must_fail grep "no matching resources" out4 &&
    grep hetchy201 out4
'

test_done

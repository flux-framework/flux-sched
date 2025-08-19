#!/bin/sh
#set -x

test_description='Test resource-match using JGF resource information

Ensure that the match (allocate) handler within the resource module works
'

. `dirname $0`/sharness.sh

jobspec_basepath=`readlink -f ${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/`
# slot[1]->core[1]
jobspec_1core="${jobspec_basepath}/basics/test008.yaml"
jgf_4core="${SHARNESS_TEST_SRCDIR}/data/resource/jgfs/hwloc_4core.json"

#
# test_under_flux is under sharness.d/
#
test_under_flux 1

#
# print only with --debug
#
test_debug '
    echo ${jgf_4core}
'

# Test using the full resource matching service
test_expect_success 'loading resource module with a tiny jgf file works' '
    load_resource \
load-file=${jgf_4core} load-format=jgf
'

test_expect_success 'JGF: allocate works with four one-core jobspecs' '
    flux ion-resource match allocate ${jobspec_1core} &&
    flux ion-resource match allocate ${jobspec_1core} &&
    flux ion-resource match allocate ${jobspec_1core} &&
    flux ion-resource match allocate ${jobspec_1core}
'

test_expect_success 'JGF: allocate fails when all resources are allocated' '
    test_expect_code 16 flux ion-resource match allocate ${jobspec_1core} &&
    test_expect_code 16 flux ion-resource match allocate ${jobspec_1core} &&
    test_expect_code 16 flux ion-resource match allocate ${jobspec_1core} &&
    test_expect_code 16 flux ion-resource match allocate ${jobspec_1core}
'

create_bad_jgfs() {
    jq ' .graph.nodes[1].id="99999" ' ${jgf_4core} > bad_id.json
    jq ' .graph.vertices = .graph.nodes | del(.graph.nodes) ' \
${jgf_4core} > bad_nodes.json
    jq ' .graph.edgs = .graph.edges | del(.graph.edges) ' \
${jgf_4core} > bad_edges.json
    jq ' del(.graph.nodes[1].metadata.paths) ' ${jgf_4core} > bad_paths.json
}

test_expect_success 'generate bad JGF files' '
    create_bad_jgfs
'

test_expect_success 'resource detects bad JGF inputs' '
    test_must_fail load_resource \
load-file=bad_id.json load-format=jgf &&
    test_must_fail load_resource \
load-file=bad_nodes.json load-format=jgf &&
    test_must_fail load_resource \
load-file=bad_edges.json load-format=jgf &&
    test_must_fail load_resource \
load-file=bad_paths.json load-format=jgf &&
    test_must_fail load_resource \
load-file=nonexistent.json load-format=jgf &&
    test_must_fail load_resource \
load-file=t4007-match-jgf.t load-format=jgf
'

test_expect_success 'removing resource works' '
    remove_resource
'

test_done

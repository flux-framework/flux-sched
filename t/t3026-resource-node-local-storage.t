#!/bin/sh

test_description='Test node-local storage cases with hwloc reader'

. $(dirname $0)/sharness.sh

cmd_dir="${SHARNESS_TEST_SRCDIR}/data/resource/commands/node_local_storage"
exp_dir="${SHARNESS_TEST_SRCDIR}/data/resource/expected/node_local_storage"
xml_dir="${SHARNESS_TEST_SRCDIR}/data/hwloc-data/001N/node_local_storage/"
query="../../resource/utilities/resource-query"


corona_xml="${xml_dir}/corona/0.xml"
corona_full_cmds="${cmd_dir}/corona-full-drive.in"
corona_full_desc="match allocate with corona hwloc and full-drive jobspec"
test_expect_success "${corona_full_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${corona_full_cmds} \
> corona_full_cmds &&
    ${query} -d -L ${corona_xml} -f hwloc -W node,socket,core,storage \
-S CA -P low -t corona-full-drive.R.out < corona_full_cmds &&
    test_cmp ${exp_dir}/corona-full-drive.R.out corona-full-drive.R.out
'

corona_small_cmds="${cmd_dir}/corona-small-drive.in"
corona_small_desc="match allocate with corona hwloc and small-drive jobspec"
test_expect_success "${corona_small_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${corona_small_cmds} \
> corona_small_cmds &&
    ${query} -d -L ${corona_xml} -f hwloc -W node,socket,core,storage \
-S CA -P low -t corona-small-drive.R.out < corona_small_cmds &&
    test_cmp ${exp_dir}/corona-small-drive.R.out corona-small-drive.R.out
'

lassen_xml="${xml_dir}/lassen/0.xml"
lassen_full_cmds="${cmd_dir}/lassen-full-drive.in"
lassen_full_desc="match allocate with lassen hwloc and full-drive jobspec"
test_expect_success "${lassen_full_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${lassen_full_cmds} \
> lassen_full_cmds &&
    ${query} -d -L ${lassen_xml} -f hwloc -W node,socket,core,gpu,storage \
-S CA -P low -t lassen-full-drive.R.out < lassen_full_cmds &&
    test_cmp ${exp_dir}/lassen-full-drive.R.out lassen-full-drive.R.out
'

lassen_small_cmds="${cmd_dir}/lassen-small-drive.in"
lassen_small_desc="match allocate with lassen hwloc and small-drive jobspec"
test_expect_success "${lassen_small_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${lassen_small_cmds} \
> lassen_small_cmds &&
    ${query} -d -L ${lassen_xml} -f hwloc -W node,socket,core,gpu,storage \
-S CA -P low -t lassen-small-drive.R.out < lassen_small_cmds &&
    test_cmp ${exp_dir}/lassen-small-drive.R.out lassen-small-drive.R.out
'

test_done

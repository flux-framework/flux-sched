#!/bin/sh

test_description='Test flux-tree correctness in real running mode'

ORIG_HOME=${HOME}

. `dirname $0`/sharness.sh

#
# sharness modifies $HOME environment variable, but this interferes
# with python's package search path, in particular its user site package.
#
HOME=${ORIG_HOME}

test_under_flux 1

if test -z "${FLUX_SCHED_TEST_INSTALLED}" || test -z "${FLUX_SCHED_CO_INST}"
 then
     export FLUX_RC_EXTRA="${SHARNESS_TEST_SRCDIR}/rc"
fi

test_expect_success 'flux-tree: prep for testing in real mode works' '
    flux module remove sched-simple &&
    flux module load resource prune-filters=ALL:core \
subsystems=containment policy=low load-whitelist=node,core,gpu &&
    flux module load qmanager
'

test_expect_success 'flux-tree: --leaf in real mode' '
    flux tree --leaf -N 1 -c 1 -J 1 -o p.out2 hostname &&
    test -f p.out2 &&
    lcount=$(wc -l p.out2 | awk "{print \$1}") &&
    test ${lcount} -eq 2 &&
    wcount=$(wc -w p.out2 | awk "{print \$1}") &&
    test ${wcount} -eq 18
'

test_expect_success 'flux-tree: -T1 in real mode' '
    flux tree -T1 -N 1 -c 1 -J 1 -o p.out3 hostname &&
    test -f p.out3 &&
    lcount=$(wc -l p.out3 | awk "{print \$1}") &&
    test ${lcount} -eq 3 &&
    wcount=$(wc -w p.out3 | awk "{print \$1}") &&
    test ${wcount} -eq 27
'

test_expect_success 'flux-tree: -T1x1 in real mode' '
    flux tree -T1x1 -N 1 -c 1 -J 1 -o p.out4 hostname &&
    test -f p.out4 &&
    lcount=$(wc -l p.out4 | awk "{print \$1}") &&
    test ${lcount} -eq 4 &&
    wcount=$(wc -w p.out4 | awk "{print \$1}") &&
    test ${wcount} -eq 36
'

test_expect_success 'flux-tree: -T2 with exit code rollup works' '
    cat >jobscript.sh <<EOF &&
#! /bin/bash
echo \${FLUX_TREE_ID}
if [[ \${FLUX_TREE_ID} = "tree.2" ]]
then
	exit 4
else
	exit 1
fi
EOF

    cat >cmp.01 <<EOF &&
tree.2
flux-tree: warning: ./jobscript.sh: exited with exit code (4)
flux-tree: warning: invocation id: tree.2@index[1]
flux-tree: warning: output displayed above, if any
EOF
    chmod u+x jobscript.sh &&
    test_expect_code 4 flux tree -T2 -N 1 -c 2 -J 2 \
./jobscript.sh > out.01 &&
    test_cmp cmp.01 out.01
'

PERF_FORMAT="{treeid}"
PERF_BLOB='{"treeid":"tree", "perf": {}}'
JOB_NAME="foobar"
test_expect_success 'flux-tree: successfully runs alongside other jobs' '
    flux tree -T 1 -N 1 -c 1 -J 1 -o p.out5 --perf-format="$PERF_FORMAT" \
         --job-name="${JOB_NAME}" -- hostname &&
    flux mini run -N1 -c1 hostname &&
    echo "$PERF_BLOB" | run_timeout 5 flux tree-helper --perf-out=p.out6 \
         --perf-format="$PERF_FORMAT" 1 "tree-perf" "${JOB_NAME}" &&
    test_cmp p.out5 p.out6
'

JOB_NAME="foobar2"
test_expect_success 'flux-tree: successfully runs alongside other flux-trees' '
    run_timeout 20 \
    flux tree -T 1x1 -N 1 -c 1 -J 1 -o p.out6 --perf-format="$PERF_FORMAT" \
         --job-name="${JOB_NAME}" -- hostname &&
    flux tree -T 1 -N 1 -c 1 -J 1 -- hostname &&
    echo "$PERF_BLOB" | run_timeout 5 flux tree-helper --perf-out=p.out7 \
         --perf-format="$PERF_FORMAT" 1 "tree-perf" "${JOB_NAME}" &&
    test_cmp p.out6 p.out7
'

test_expect_success 'flux-tree: works with pre-existing rundir subdirectories' '
    flux tree -T 1 -N 1 -c 1 -r DIR -- hostname &&
    flux tree -T 1 -N 1 -c 1 -r DIR -- hostname
'

test_expect_success 'flux-tree: removing qmanager/resource works' '
     flux module remove resource &&
     flux module remove qmanager
'

test_done

#!/bin/sh

test_description='Test data-staging job shell plugin'

. `dirname $0`/sharness.sh

shell_basepath=`readlink -e ${SHARNESS_TEST_SRCDIR}/data/shell`
shell_plugin_path="$(readlink -e ${SHARNESS_BUILD_DIRECTORY}/src/shell/.libs)"
jobspec_basepath=`readlink -e ${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/`
flux_shell="$(pkg-config --variable=prefix flux-core)/libexec/flux/flux-shell"

test_under_flux 2

test_expect_success 'node-local storage allocation staging rank 0' '
	cat <<-EOT >test-initrc.lua &&
	plugin.load { file = "${shell_plugin_path}/*.so" }
	os.exit(0)
	EOT
    cat ${shell_basepath}/node-local/jobspec.yaml \
        | flux python ${SHARNESS_TEST_SRCDIR}/scripts/y2j.py \
            > node-jobspec.json &&
	${flux_shell} -s -r 0 -R ${shell_basepath}/node-local/R.json \
		-j node-jobspec.json \
		--initrc=test-initrc.lua \
		0 2>&1 | tee shell-rank0-node-local.outerr |
	grep -q "Rank 0 staging from /usr/src/README to /p/ssd0/README"
'

test_expect_success 'node-local storage allocation staging rank 1' '
	${flux_shell} -s -r 1 -R ${shell_basepath}/node-local/R.json \
		-j node-jobspec.json \
		--initrc=test-initrc.lua \
		0 2>&1 | tee shell-rank1-node-local.outerr |
	grep -q "Rank 1 staging from /usr/src/README to /p/ssd1/README"
'

test_expect_success 'cluster-local storage allocation staging rank 0' '
    cat ${shell_basepath}/cluster-local/jobspec.yaml \
        | flux python ${SHARNESS_TEST_SRCDIR}/scripts/y2j.py \
            > cluster-jobspec.json &&
    ${flux_shell} -s -r 0 -R ${shell_basepath}/cluster-local/R.json \
        -j cluster-jobspec.json \
        --initrc=test-initrc.lua \
        0 2>&1 | tee shell-rank0-cluster-local.outerr |
    grep -q "Rank 0 staging from /usr/src/foo to /p/pfs0/foo"
'

test_expect_success 'cluster-local storage allocation staging rank 1' '
    ${flux_shell} -s -r 1 -R ${shell_basepath}/cluster-local/R.json \
        -j cluster-jobspec.json \
        --initrc=test-initrc.lua \
        0 2>&1 | tee shell-rank1-cluster-local.outerr |
    grep -q "Rank 1 skipping staging to a job granularity storage"
'

test_expect_success 'combined storage allocation staging rank 0' '
    cat ${shell_basepath}/combined/jobspec.yaml \
        | flux python ${SHARNESS_TEST_SRCDIR}/scripts/y2j.py \
            > cluster-jobspec.json &&
    ${flux_shell} -s -r 0 -R ${shell_basepath}/combined/R.json \
        -j cluster-jobspec.json \
        --initrc=test-initrc.lua \
        0 2>&1 | tee shell-rank0-combined.outerr &&
    grep -q "Rank 0 staging from /usr/src/README to /p/ssd0/README" shell-rank0-combined.outerr &&
    grep -q "Rank 0 staging from /usr/src/foo to /p/pfs0/foo" shell-rank0-combined.outerr
'

test_expect_success 'combined storage allocation staging rank 1' '
    ${flux_shell} -s -r 1 -R ${shell_basepath}/combined/R.json \
        -j cluster-jobspec.json \
        --initrc=test-initrc.lua \
        0 2>&1 | tee shell-rank1-combined.outerr &&
    grep -q "Rank 1 staging from /usr/src/README to /p/ssd1/README" shell-rank1-combined.outerr &&
    grep -q "Rank 1 skipping staging to a job granularity storage" shell-rank1-combined.outerr
'

test_expect_success 'loading resource and qmanager modules works' '
    flux module remove sched-simple &&
    load_resource load-allowlist=node,core,socket,storage \
prune-filters=ALL:core subsystems=containment policy=low \
load-format=jgf load-file=${shell_basepath}/corona-jgf.json \
match-format=rv1 &&
    load_qmanager
'

test_expect_success 'setting shell plugin searchpath' '
    flux exec flux setattr conf.shell_pluginpath ${shell_plugin_path}
'

test_expect_success 'submitting node-local jobspec succeeded' '
    cat ${shell_basepath}/node-local/jobspec.yaml \
        | flux python ${SHARNESS_TEST_SRCDIR}/scripts/y2j.py \
            > node-jobspec.json &&
        flux job submit -f waitable node-jobspec.json > node-jobid.out &&
        flux job wait $(cat node-jobid.out) &&
        flux job attach $(cat node-jobid.out) 2> node-local-endtoend.err &&
        grep -q "Rank 0 staging from /usr/src/README to /p/ssd0/README" \
            node-local-endtoend.err &&
        grep -q "Rank 1 staging from /usr/src/README to /p/ssd1/README" \
            node-local-endtoend.err
'

test_expect_success 'submitting cluster-local jobspec succeeded' '
    cat ${shell_basepath}/cluster-local/jobspec.yaml \
        | flux python ${SHARNESS_TEST_SRCDIR}/scripts/y2j.py \
            > cluster-jobspec.json &&
        flux job submit -f waitable cluster-jobspec.json > cluster-jobid.out &&
        flux job wait $(cat cluster-jobid.out) &&
        flux job attach $(cat cluster-jobid.out) \
             2> cluster-local-endtoend.err &&
        grep -q "Rank 0 staging from /usr/src/foo to /p/pfs0/foo" \
            cluster-local-endtoend.err &&
        grep -q "Rank 1 skipping staging to a job granularity storage" \
            cluster-local-endtoend.err
'

test_expect_success 'submitting combined storage jobspec succeeded' '
    cat ${shell_basepath}/combined/jobspec.yaml \
        | flux python ${SHARNESS_TEST_SRCDIR}/scripts/y2j.py \
            > combined-jobspec.json &&
        flux job submit -f waitable combined-jobspec.json \
            > combined-jobid.out &&
        flux job wait $(cat combined-jobid.out) &&
        flux job attach $(cat combined-jobid.out) 2> combined-endtoend.err &&
        grep -q "Rank 0 staging from /usr/src/README to /p/ssd0/README" \
            combined-endtoend.err &&
        grep -q "Rank 0 staging from /usr/src/foo to /p/pfs0/foo" \
            combined-endtoend.err &&
        grep -q "Rank 1 staging from /usr/src/README to /p/ssd1/README" \
            combined-endtoend.err &&
        grep -q "Rank 1 skipping staging to a job granularity storage" \
            combined-endtoend.err
'

test_expect_success 'datastaging logging is not overly verbose by default' '
    flux mini run hostname 2> non-verbose.err &&
        test_must_be_empty non-verbose.err
'

test_expect_success 'datastaging logging can be access with verbose option' '
    flux mini run -o verbose=2 hostname 2> verbose-run.err &&
        grep -q " DEBUG: Jobspec does not contain data-staging attributes. "\
"No staging necessary." verbose-run.err
'

test_expect_success 'removing resource and qmanager modules' '
    remove_qmanager &&
    remove_resource
'

test_done

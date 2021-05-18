test_description='Test Fluxion with Excluded Rank Resources'

. `dirname $0`/sharness.sh

hwloc_basepath=`readlink -e ${SHARNESS_TEST_SRCDIR}/data/hwloc-data`
# 4 brokers, each (exclusively) have:
# 1 node, 2 sockets, 44 cores (22 per socket), 4 gpus (2 per socket)
excl_4N4B="${hwloc_basepath}/004N/exclusive/04-brokers-sierra2"

skip_all_unless_have jq

export FLUX_CONF_DIR=$(pwd)

export FLUX_SCHED_MODULE=none
test_under_flux 4

test_expect_success 'exclusion: generate jobspecs' '
    flux mini run --dry-run -N 4 -n 4 -c 44 -g 4 -t 1h sleep 3600 > basic.json &&
    flux mini run --dry-run -N 1 -n 1 -c 44 -g 4 -t 1h sleep 3600 > 1N.json
'

test_expect_success 'exclusion: load config with resource exclusions' '
    cat >resource.toml <<-EOF &&
	[resource]
	exclude = "0-3"
EOF
    flux config reload
'

test_expect_success 'load test resources' '
    load_test_resources ${excl_4N4B}
'

test_expect_success 'exclusion: loading fluxion modules works' '
    load_resource load-allowlist=cluster,node,core,gpu policy=high &&
    load_qmanager
'

test_expect_success 'exclusion: all jobs are rejected on satisfiability' '
    flux dmesg > dmesg.out &&
    jobid1=$(flux job submit basic.json) &&
    jobid2=$(flux job submit 1N.json) &&
    test_must_fail flux job wait-event -t 1 ${jobid1} start &&
    test_must_fail flux job wait-event -t 1 ${jobid2} start &&
    flux job eventlog ${jobid1} | grep unsatisfiable &&
    flux job eventlog ${jobid2} | grep unsatisfiable
'
test_expect_success 'exclusion: removing fluxion modules' '
    remove_qmanager &&
    remove_resource
'
test_expect_success 'exclusion: reloading resource with a config file' '
    cat >resource.toml <<-EOF &&
	[resource]
	exclude = "2-3"
EOF
    flux config reload
'

test_expect_success 'exclusion: loading fluxion modules works' '
    load_resource load-allowlist=cluster,node,core,gpu &&
    load_qmanager
'

test_expect_success 'exclusion: only 1-node jobs are satisfiable' '
    jobid1=$(flux job submit basic.json) &&
    jobid2=$(flux job submit 1N.json) &&
    jobid3=$(flux job submit 1N.json) &&
    test_must_fail flux job wait-event -t 1 ${jobid1} start &&
    flux job wait-event -t 10 ${jobid2} start &&
    flux job wait-event -t 10 ${jobid3} start &&
    flux job eventlog ${jobid1} | grep unsatisfiable
'

test_expect_success 'exclusion: cancel the running job' '
    flux job cancel ${jobid2} && 
    flux job cancel ${jobid3} && 
    flux job wait-event -t 10 ${jobid3} release
'

test_expect_success 'cleanup active jobs' '
    cleanup_active_jobs
'

test_expect_success 'exclusion: removing fluxion modules' '
    remove_qmanager &&
    remove_resource
'

test_done


#!/bin/sh

test_description='Test Slot Exclusive Scheduling'

. $(dirname $0)/sharness.sh

test_under_flux 1

jobspec1="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/exclusive/test011.json"
jobspec2="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/exclusive/test012.json"

is_node_exclusive() {
    ID=${1} &&
    RANKS=${2}
    flux job info ${ID} R > R.${ID} &&
    len1=$(jq ".execution.R_lite | length" R.${ID}) &&
    test ${len1} -eq 1 &&
    ranks=$(jq ".execution.R_lite[0].rank" R.${ID}) &&
    cores=$(jq ".execution.R_lite[0].children.core" R.${ID}) &&
    test ${ranks} = "\"${RANKS}\"" &&
    test ${cores} = ${3}
}

test_expect_success 'load test configuration' '
	flux module remove sched-simple &&
	flux module remove resource &&
	flux config load <<EOF &&
[resource]
noverify = true
norestrict = true
path="${SHARNESS_TEST_SRCDIR}/data/resource/jgfs/resource-exclusivity-test.json"
EOF
	flux module load resource monitor-force-up &&
	flux module load sched-fluxion-resource prune-filters=ALL:core,ALL:socket &&
	flux module load sched-fluxion-qmanager &&
	flux module unload job-list &&
	flux module list &&
	flux queue start --all --quiet &&
	flux resource list &&
	flux resource status
'

test_expect_success 'reload ingest without validator' '
	flux module reload -f job-ingest disable-validator
'

test_expect_success 'qmanager-nodex: submit a job with slot above node (high)' '
    jobid1=$(flux job submit ${jobspec1}) &&
    flux job wait-event -t 10 ${jobid1} start
'

test_expect_success 'allocated node is exclusive (high)' '
    is_node_exclusive ${jobid1} 0 \"0-35\"
'

test_expect_success 'cancel job 1' '
    flux cancel ${jobid1}
'

test_expect_success 'submit a socket-exclusive job (high)' '
    jobid2=$(flux job submit ${jobspec2}) &&
    flux job wait-event -t 10 ${jobid2} start
'

test_expect_success 'allocated node is not exclusive (high)' '
    is_node_exclusive ${jobid2} 0 \"18-35\"
'

test_expect_success 'cancel job 2' '
    flux cancel ${jobid2}
'

test_expect_success 'reload fluxion and resource module' '
	flux module remove sched-fluxion-qmanager &&
	flux module remove sched-fluxion-resource
'

test_done

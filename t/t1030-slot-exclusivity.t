#!/bin/sh

test_description='Test Slot Exclusive Scheduling'

. $(dirname $0)/sharness.sh

jobspec1="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/exclusive/test011.json"
jobspec2="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/exclusive/test012.json"
jobspec3="${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/exclusive/test013.json"
jobspec3="/usr/src/js.out"

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


hwloc_basepath=`readlink -e ${SHARNESS_TEST_SRCDIR}/data/hwloc-data`
# 1 brokers: 1 node, 2 sockets, 44 cores 4 gpus
excl_1N1B="${hwloc_basepath}/001N/exclusive/01-brokers-sierra2"
export FLUX_HWLOC_XMLFILE_NOT_THISSYSTEM=1

export FLUX_SCHED_MODULE=none

#export FLUX_HWLOC_XMLFILE=${hwloc_basepath}/001N/exclusive/01-brokers-sierra2/0.xml
test_under_flux 1 full

echo $FLUX_HWLOC_XMLFILE

test_expect_success 'namespace: load test resources' '
	load_test_resources ${excl_1N1B} &&
	load_resource prune-filters=ALL:core,ALL:node subsystems=containment policy=first &&
    load_qmanager queue-policy=easy
	# flux module list &&
	# flux queue start --all --quiet &&
	# flux resource list &&
	# flux resource status &&
    #flux ion-resource find status=up --format=jgf
'

test_expect_success 'reload ingest without validator' '
	#flux module reload -f job-ingest disable-validator
'

test_expect_success 'qmanager-nodex: submit a test job' '
    flux run -N1 -n2 -c2 -o cpu-affinity=dry-run true
    #flux job wait-event -t 10 ${jobid3} start &&
    #flux job attach ${jobid3} >dry-run-verbose2.out 
'

# test_expect_success 'load test configuration' '
# 	flux config load <<EOF &&
# [resource]
# noverify = true
# norestrict = true
# path="${SHARNESS_TEST_SRCDIR}/data/resource/jgfs/resource-exclusivity-test.json"
# EOF
# 	flux module reload -f resource monitor-force-up &&
# 	flux module load sched-fluxion-resource prune-filters=ALL:core,ALL:socket load-allowlist=cluster,rack,node,socket,core &&
# 	flux module load sched-fluxion-qmanager &&
# 	flux module list &&
# 	flux queue start --all --quiet &&
# 	flux resource list &&
# 	flux resource status &&
#     flux ion-resource find status=up --format=jgf
# '

test_done

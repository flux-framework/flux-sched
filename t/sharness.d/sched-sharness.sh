
#
# project-local sharness code for flux-sched
#
FLUX_SCHED_RC_NOOP=1
FLUX_RESOURCE_RC_NOOP=1
if test -n "$FLUX_SCHED_TEST_INSTALLED"; then
  # Test against installed flux-sched, installed under same prefix as
  #   flux-core.
  # (Assume sched modules installed under PREFIX/lib/flux/modeuls/sched)
  # (We also support testing this when sched is installed
  # another location, but only via make check)
  FLUX_MODULE_PATH_PREPEND=$(which flux | sed -s 's|/bin/flux|/lib/flux/modules/sched|'):${FLUX_MODULE_PATH_PREPEND}
  FLUX_EXEC_PATH_PREPEND=${SHARNESS_TEST_SRCDIR}/scripts:${FLUX_EXEC_PATH_PREPEND}
else
  # Set up environment so that we find flux-sched modules,
  #  commands, and Lua libs from the build directories:
  FLUX_LUA_PATH_PREPEND="${SHARNESS_TEST_SRCDIR}/../rdl/?.lua"
  FLUX_LUA_CPATH_PREPEND="${SHARNESS_BUILD_DIRECTORY}/rdl/?.so"
  FLUX_MODULE_PATH_PREPEND="${SHARNESS_BUILD_DIRECTORY}/sched/.libs:${SHARNESS_BUILD_DIRECTORY}/resource/modules/.libs"
  FLUX_EXEC_PATH_PREPEND="${SHARNESS_BUILD_DIRECTORY}/sched:${SHARNESS_TEST_SRCDIR}/scripts"
fi

## Set up environment using flux(1) in PATH
flux --help >/dev/null 2>&1 || error "Failed to find flux in PATH"
eval $(flux env)

# Return canonicalized path to a file in the *build* tree
sched_build_path () {
    readlink -e "${SHARNESS_BUILD_DIRECTORY}/${1}"
}

# Return canonicalized path to a file in the *src* tree
sched_src_path () {
    readlink -e "${SHARNESS_TEST_SRCDIR}/../${1}"
}

job_kvs_path () {
    if flux wreck help 2>&1 | grep -q kvs-path; then
        flux wreck kvs-path "$1"
    else
        echo "lwj.$1"
    fi
}

export FLUX_EXEC_PATH_PREPEND
export FLUX_SCHED_RC_NOOP
export FLUX_SCHED_RC_PATH
export FLUX_RESOURCE_RC_NOOP
export FLUX_RESOURCE_RC_PATH
export FLUX_SCHED_CO_INST
export FLUX_MODULE_PATH_PREPEND
export FLUX_LUA_CPATH_PREPEND
export FLUX_LUA_PATH_PREPEND
export LUA_PATH
export LUA_CPATH

sched_instance_size=0
sched_test_session=0
sched_start_jobid=0
sched_end_jobid=0

if test "$TEST_LONG" = "t"; then
    test_set_prereq LONGTEST
fi

#
# Internal subroutines
#
sched_debug_to_file () {
    local fn=$1
    local str=$2
cat > $fn <<-HEREDOC
    $str
HEREDOC
}

submit_1N_sleep_jobs () {
    local procs=$1
    local origcores=$1
    local s_amnt=$2
    local opt=$3
    local rc=0

    if [ $procs -lt 1 ]
    then
        return 48
    fi

    for i in `seq $sched_start_jobid $sched_end_jobid`
    do
        flux submit -N 1 -n $procs sleep $s_amnt
        echo "debug: flux submit -N 1 -n $procs sleep $s_amnt" 
        if [ $? -ne 0 ]
        then
            return 48
        fi
        case $opt in
            cons)
                ;;
            decr) 
                procs=$((procs - 1))
                ;;
            fflop) 
                if [ `expr $i % 2` -eq 0 ]
                then
                    procs=$origcores
                else
                    procs=1
                fi
                ;;
            *)     
                procs=0
                ;;
        esac    
        if [ $procs -lt 1 ]
        then
            return 48
        fi
    done
    return 0
}

verify_1N_sleep_jobs () {
    local cores=$1
    local origcores=$1
    local opt=$2
    local rank=0
    for i in `seq $sched_start_jobid $sched_end_jobid`
    do
        $SHARNESS_TEST_SRCDIR/scripts/R_lite.lua ${i} ${rank} core count \
            > ${sched_test_session}.${i}.out
        grep $cores $sched_test_session.$i.out
        if [ $? -ne 0 ]
        then
            return 48
        fi
        case $opt in
            cons)
                ;;
            decr)
                cores=$((cores - 1))
                ;;
            fflop) 
                if [ `expr $i % 2` -eq 0 ]
                then
                    cores=$origcores
                else
                    cores=1
                fi
                ;;
            *)
                cores=0
                ;;
        esac
        if [ $cores -lt 1 ]
        then
            return 48
        fi
        rank=$((rank + 1))
    done
    return 0
}


# PUBLIC:
#   Accessors 
#
get_instance_size () {
    if test $sched_instance_size -eq 0; then
        sched_instance_size=$(flux getattr size)
    fi
    echo $sched_instance_size
}

get_session () {
    echo $sched_test_session
}

get_start_jobid () {
    echo $sched_start_jobid
}

get_end_jobid () {
    echo $sched_end_jobid
}

set_session () {
    sched_test_session=$1
}

set_start_jobid () {
    sched_start_jobid=$1
}

set_end_jobid () {
    sched_end_jobid=$1
}

adjust_session_info () {
    local njobs=$1
    set_session $(($(get_session) + 1))
    set_start_jobid $(($(get_end_jobid) + 1))
    set_end_jobid $(($(get_start_jobid) + $njobs - 1))
    return 0
}

# PUBLIC:
#   Run flux-jstat in background. Wait up to 2 seconds 
#   until flux-jstat gets ready to receive JSC events. 
#   jstat's output will be printed to $1.<session id>
#
timed_run_flux_jstat () {
    local fn=$1
    ofile=${fn}.$sched_test_session
    rm -f ${ofile}
    flux jstat -o ${ofile} notify >/dev/null &
    echo $! &&
    $SHARNESS_TEST_SRCDIR/scripts/waitfile.lua --timeout 2 ${ofile} >&2
}

# PUBLIC:
#   Run flux-waitjob in background. Wait up to $1 seconds
#   until flux-waitjob gets ready to receive JSC events. 
#   waitjob creates wo.st.<session id> to signal its readiness
#   and wo.end.<session id> to indicate the specified job
#   has completed.
#
timed_wait_job () {
    local tout=$1
    local state=$2
    local sfile=wo.st.$sched_test_session
    local cfile=wo.end.$sched_test_session
    rm -f ${sfile} ${cfile}
    if [ "x${state}" = "x" ]; then
        flux waitjob -s ${sfile} -c ${cfile} ${sched_end_jobid} &
    else
        flux waitjob -s ${sfile} -c ${cfile} -j ${state} ${sched_end_jobid} &
    fi
    $SHARNESS_TEST_SRCDIR/scripts/waitfile.lua --timeout ${tout} ${sfile} >&2
}

# PUBLIC:
#   Wait up to $1 seconds until the previously invoked flux-waitjob
#   detects the final job has completed.
#
timed_sync_wait_job () {
    local tout=$1
    local cfile=wo.end.$sched_test_session
    $SHARNESS_TEST_SRCDIR/scripts/waitfile.lua --timeout ${tout} ${cfile} >&2
}

# PUBLIC: 
#   Submit 1-node sleep jobs to flux back to back.
#   Each job sleeps for $2 seconds.
#   The first job is to run at $1 processes and the
#   second job at $1-1 processes and so forth.
#
#   The user script should already have updated
#   sched_start_jobid and sched_end_jobid so that
#   this script submits that many number of jobs.
#
submit_1N_decr_nproc_sleep_jobs () {
    submit_1N_sleep_jobs $1 $2 "decr"
    return $?
}

# PUBLIC:
#   Submit 1-node sleep jobs to flux back to back.
#   Each job sleeps for $2 seconds.
#   Unlike submit_1N_decr_nproc_sleep_jobs, the first
#   job is to run at $1 processes and the second job
#   at 1 process and as such this pattern repeats 
#
#   The user script should already have updated
#   sched_start_jobid and sched_end_jobid so that
#   this script submits that many number of jobs.
#
submit_1N_fflop_nproc_sleep_jobs () {
    submit_1N_sleep_jobs $1 $2 "fflop" 
    return $?
}

# PUBLIC:
#   Submit 1-node sleep jobs at $1 processes
#   to flux back to back.
#
#   The user script should already have updated
#   sched_start_jobid and sched_end_jobid so that
#   this script submits that many number of jobs.
#
submit_1N_nproc_sleep_jobs () {
    submit_1N_sleep_jobs $1 $2 "cons"
    return $?
}

# PUBLIC:
#   Verify that submit_1N_decr_nproc_sleep_jobs
#   successfully targeted the right set of ranks
#   to launch the application processes in the correct
#   locations. 
#
#   Currently, we assume the resources under ranks are
#   scheduled in rank order. This assumption may break
#   when resrc traversing for scheduling change at which
#   point we will need a bit more advanced verification 
#   mechanism.
#
verify_1N_decr_nproc_sleep_jobs () {
    verify_1N_sleep_jobs $1 "decr"
    return $?
}

# PUBLIC:
#   Verify that submit_1N_fflop_nproc_sleep_jobs
#   successfully targeted the right set of ranks.
#
#   Currently, we assume the resources under ranks are
#   scheduled in rank order. This assumption may break
#   when resrc traversing for scheduling change at which
#   point we need a bit more advanced verification 
#   mechanism.
#
verify_1N_fflop_nproc_sleep_jobs () {
    verify_1N_sleep_jobs $1 "fflop"
    return $?
}

# PUBLIC:
#   Verify that submit_1N_nproc_sleep_jobs
#   successfully targeted the right set of ranks.
#
verify_1N_nproc_sleep_jobs () {
    verify_1N_sleep_jobs $1 "cons"
    return $?
}


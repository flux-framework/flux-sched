
#
# project-local sharness code for flux-sched
#
FLUXION_RESOURCE_RC_NOOP=1
FLUXION_QMANAGER_RC_NOOP=1
if test -n "$FLUX_SCHED_TEST_INSTALLED"; then
  # Test against installed flux-sched, installed under same prefix as
  #   flux-core.
  # (Assume sched modules installed under PREFIX/lib/flux/modules)
  # (We also support testing this when sched is installed
  # another location, but only via make check)
  FLUX_EXEC_PATH_PREPEND=${SHARNESS_TEST_SRCDIR}/scripts:${FLUX_EXEC_PATH_PREPEND}
else
  # Set up environment so that we find flux-sched modules,
  #  and commands from the build directories:
  FLUX_MODULE_PATH_PREPEND="${SHARNESS_BUILD_DIRECTORY}/resource/modules/.libs"
  FLUX_MODULE_PATH_PREPEND="${SHARNESS_BUILD_DIRECTORY}/qmanager/modules/.libs":${FLUX_MODULE_PATH_PREPEND}
  FLUX_EXEC_PATH_PREPEND="${SHARNESS_TEST_SRCDIR}/scripts":"${SHARNESS_TEST_SRCDIR}/../src/cmd"
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

export FLUX_EXEC_PATH_PREPEND
export FLUXION_RESOURCE_RC_NOOP
export FLUXION_QMANAGER_RC_NOOP
export FLUXION_RESOURCE_RC_PATH
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

adjust_session_info () {
    local njobs=$1
    set_session $(($(get_session) + 1))
    set_start_jobid $(($(get_end_jobid) + 1))
    set_end_jobid $(($(get_start_jobid) + $njobs - 1))
    return 0
}

load_qmanager () {
    flux module load sched-fluxion-qmanager "$@"
}

load_resource () {
    flux module load sched-fluxion-resource "$@"
}

reload_qmanager () {
    flux module reload -f sched-fluxion-qmanager "$@"
}

reload_resource () {
    flux module reload -f sched-fluxion-resource "$@"
}

remove_qmanager () {
    flux module remove sched-fluxion-qmanager "$@"
}

remove_resource () {
    flux module remove sched-fluxion-resource "$@"
}

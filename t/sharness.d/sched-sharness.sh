
#
# project-local sharness code for flux-sched
#

FLUXION_RESOURCE_RC_NOOP=1
FLUXION_QMANAGER_RC_NOOP=1
FLUX_MODPROBE_DISABLE=sched-fluxion-resource,sched-fluxion-qmanager

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
  FLUX_MODULE_PATH_PREPEND="${SHARNESS_BUILD_DIRECTORY}/resource/modules/:${FLUX_MODULE_PATH_PREPEND}"
  FLUX_MODULE_PATH_PREPEND="${SHARNESS_BUILD_DIRECTORY}/qmanager/modules/:${FLUX_MODULE_PATH_PREPEND}"
  FLUX_EXEC_PATH_PREPEND="${SHARNESS_TEST_SRCDIR}/scripts":"${SHARNESS_TEST_SRCDIR}/../src/cmd"
  export PYTHONPATH="${SHARNESS_TEST_SRCDIR}/../src/python${PYTHONPATH:+:${PYTHONPATH}}"
  export FLUX_MODPROBE_PATH="${SHARNESS_TEST_SRCDIR}/../etc/modprobe"
fi

# Setup FLUX_SOURCE_DIR for use in flux-sharness.sh
FLUX_SOURCE_DIR="$(cd ${SHARNESS_TEST_SRCDIR}/.. && pwd)"
export FLUX_SOURCE_DIR

## Set up environment using flux(1) in PATH
flux --help >/dev/null 2>&1 || error "Failed to find flux in PATH"
eval $(flux env)

export FLUX_EXEC_PATH_PREPEND
export FLUXION_RESOURCE_RC_NOOP
export FLUXION_QMANAGER_RC_NOOP
export FLUX_MODPROBE_DISABLE
export FLUXION_RESOURCE_RC_PATH
export FLUX_SCHED_CO_INST
export FLUX_MODULE_PATH_PREPEND
export FLUX_LUA_CPATH_PREPEND
export FLUX_LUA_PATH_PREPEND
export LUA_PATH
export LUA_CPATH

# Only enable modprobe if this version of flux has working modprobe command
if flux modprobe show sched-fluxion-resource >/dev/null 2>&1; then
    export FLUX_RC_USE_MODPROBE=t
fi

if test "$TEST_LONG" = "t"; then
    test_set_prereq LONGTEST
fi

# PUBLIC:
#   Accessors 
#

test_cmp_json() {
    jq -S "$1" > "$1.tmp"
    jq -S "$2" > "$2.tmp"
    test_cmp "$1.tmp" "$1.tmp"
}

load_qmanager () {
    flux module load sched-fluxion-qmanager "$@"
}

# See reload_qmanager_sync() description
load_qmanager_sync () {
    flux module load sched-fluxion-qmanager "$@" &&
    flux module stats sched-fluxion-qmanager
}

load_resource () {
    flux module load sched-fluxion-resource "$@"
}

load_feasibility () {
    flux module load sched-fluxion-feasibility "$@"
}

reload_qmanager () {
    flux module reload -f sched-fluxion-qmanager "$@"
}

# qmanager calls flux_module_set_running() before it has completed its
# synchronous handshake with fluxion-resource.  This function reloads
# qmanager and returns after it has completed the handshake, so that tests
# may probe fluxion-resource directly and assume it is fully initialized.
reload_qmanager_sync () {
    flux module reload -f sched-fluxion-qmanager "$@" &&
    flux module stats sched-fluxion-qmanager
}

reload_resource () {
    flux module reload -f sched-fluxion-resource "$@"
}

reload_feasibility () {
    flux module reload -f sched-fluxion-feasibility "$@"
}

remove_qmanager () {
    flux module remove sched-fluxion-qmanager "$@"
}

remove_resource () {
    flux module remove sched-fluxion-resource "$@"
}

remove_feasibility () {
    flux module remove sched-fluxion-feasibility "$@"
}

# Usage: load_test_resources hwloc-dir
#   where hwloc-dir contains <rank>.xml files
load_test_resources () {
    flux resource reload --xml $1 &&
        flux kvs get resource.R
}

# N.B. this assumes that a scheduler is loaded
cleanup_active_jobs () {
    flux queue stop --all &&
        flux cancel --all &&
        flux queue idle
}

# Usage: remap_rv1_resource_type file resource_type
#            [rank0_offset rank1_offset rank2_offset rank3_offset id_offset]
#   Remap rank and Id of the given resource type based on rank offsets
#   (corresponding rank - rank#_offset) id_offset.
#   If the rebase refactors are not passed in, don't remap.
#   Print the remapped info into a CSV file with heading:
#   Name, Rank, Id, Id-in-paths
#
remap_rv1_resource_type() {
    local json=${1} &&
    local type=${2} &&
    local id_rebase=${3:-0} &&
    local rank_remap0=${4:-0} &&
    local rank_remap1=${5:-0} &&
    local rank_remap2=${6:-0} &&
    local rank_remap3=${7:-0} &&
    local path_prefix=$(expr 21 + ${#type})

    # jq filter
    local filter=".scheduling.graph.nodes[].metadata | \
select(.type == \"${type}\") | .name |= .[${#type}:] | \
.paths.containment |= .[${path_prefix}:] | \
[ if .rank == 0 then .rank - ${rank_remap0} \
elif .rank == 1 then .rank - ${rank_remap1} \
elif .rank == 2 then .rank - ${rank_remap2} \
else .rank - ${rank_remap3} end, .id - ${id_rebase}, \
(.paths.containment | tonumber - ${id_rebase}) ] | @csv" &&
    echo Name,Rank,Id,Id-in-paths &&
    jq -r " ${filter} " ${json}
}

AM_CFLAGS = $(WARNING_CFLAGS) $(CODE_COVERAGE_CFLAGS)
AM_LDFLAGS = $(CODE_COVERAGE_LIBS)

TEST_EXTENSIONS = .t
T_LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) \
	$(top_srcdir)/config/tap-driver.sh

#
# Set up paths for fluxometer-based Lua tests.
# LUA_PATH is set to find fluxometer.lua itself.
# The PREPEND versions are required so that in-tree versions of
#  modules are found instead of the lua modules in the path
#  of the flux installation we're using for testing. If
#  $FLUX_SCHED_TEST_INSTALLED is set in the current environment,
#  export the $(prefix) PREPEND variables so that they can
#  be picked up from the sched install directiories which
#  may or may not be the same as $(FLUX_PREFIX).
#
# FLUX_SCHED_CO_INST is exported so that runlevel-install
# test can conditionally set FLUX_RC_EXTRA.
#
TESTS_ENVIRONMENT = \
    FLUX_EXEC_PATH_PREPEND="$${FLUX_SCHED_TEST_INSTALLED:+$(prefix)/libexec/flux/cmd}" \
    FLUX_MODULE_PATH_PREPEND="$${FLUX_SCHED_TEST_INSTALLED:+$(prefix)/lib/flux/modules}" \
    FLUX_SCHED_RC_PATH="$${FLUX_SCHED_TEST_INSTALLED:+$(sysconf)/flux}" \
    FLUX_RESOURCE_RC_PATH="$${FLUX_SCHED_TEST_INSTALLED:+$(sysconf)/flux}" \
    FLUX_SCHED_CO_INST=`sh -c 'if [ $(FLUX_PREFIX) = $(prefix) ]; then echo co; fi'` \
    PATH="$(FLUX_PREFIX)/bin:$(PATH)"

TESTS = \
    t0000-sharness.t \
    t0003-basic-install.t \
    t1001-qmanager-basic.t \
    t1002-qmanager-reload.t \
    t1003-qmanager-policy.t \
    t3000-jobspec.t \
    t3001-resource-basic.t \
    t3002-resource-prefix.t \
    t3003-resource-global.t \
    t3004-resource-excl.t \
    t3005-resource-rsv.t \
    t3006-resource-advanced.t \
    t3007-resource-iobw.t \
    t3008-resource-cancel.t \
    t3009-resource-minmax.t \
    t3010-resource-power.t \
    t3011-resource-filt.t \
    t3012-resource-properties.t \
    t3013-resource-unsat.t \
    t4000-match-params.t \
    t4001-match-allocate.t \
    t4002-match-reserve.t \
    t4003-cancel-info.t \
    t4004-match-hwloc.t \
    t4005-match-unsat.t \
    t5000-valgrind.t \
    t6000-graph-size.t \
    t6001-match-formats.t

check_SCRIPTS = $(TESTS)

EXTRA_DIST= \
	$(check_SCRIPTS) \
	data \
	valgrind \
	data/resource/jobspecs/basics \
	scripts \
	sharness.sh \
	sharness.d \
	schemas

clean-local:
	rm -rf *.o test-results trash-directory*


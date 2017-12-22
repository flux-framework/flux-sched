AM_CFLAGS = $(WARNING_CFLAGS) $(CODE_COVERAGE_CFLAGS)
AM_LDFLAGS = $(CODE_COVERAGE_LIBS)

TEST_EXTENSIONS = .t
T_LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) \
	$(top_srcdir)/m4/tap-driver.sh

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
    LUA_PATH="$(FLUX_PREFIX)/share/lua/5.1/?.lua;$(LUA_PATH);;" \
    FLUX_LUA_PATH_PREPEND="$${FLUX_SCHED_TEST_INSTALLED:+$(prefix)/share/lua/5.1/?.lua}" \
    FLUX_LUA_PATH_PREPEND="$${FLUX_LUA_PATH_PREPEND:-$(abs_top_srcdir)/rdl/?.lua}" \
    FLUX_LUA_CPATH_PREPEND="$${FLUX_SCHED_TEST_INSTALLED:+$(prefix)/lib64/lua/5.1/?.so}" \
    FLUX_LUA_CPATH_PREPEND="$${FLUX_LUA_CPATH_PREPEND:-$(abs_top_builddir)/rdl/?.so}" \
    FLUX_EXEC_PATH_PREPEND="$${FLUX_SCHED_TEST_INSTALLED:+$(prefix)/libexec/flux/cmd}" \
    FLUX_MODULE_PATH_PREPEND="$${FLUX_SCHED_TEST_INSTALLED:+$(prefix)/lib/flux/modules}" \
    FLUX_SCHED_RC_PATH="$${FLUX_SCHED_TEST_INSTALLED:+$(sysconf)/flux}" \
    FLUX_SCHED_CO_INST=$(shell if [ $(FLUX_PREFIX) = $(prefix) ]; then echo co; fi) \
    PATH="$(FLUX_PREFIX)/bin:$(PATH)"

TESTS = \
    t0000-sharness.t \
    t0001-basic.t \
    lua/t0001-rdl-basic.t \
    lua/t0002-multilevel.t \
    lua/t0003-default-tags.t \
    lua/t0004-derived-type.t \
    lua/t0005-multi-hierarchy.t \
    t0002-waitjob.t \
    t0003-basic-install.t \
    t0004-rdltool.t \
    t1000-jsc.t \
    t1001-rs2rank-basic.t \
    t1002-rs2rank-64ranks.t \
    t1003-stress.t \
    t1004-module-load.t \
    t1005-sched-params.t \
    t2000-fcfs.t \
    t2001-fcfs-aware.t \
    t2002-easy.t \
    t3001-resource-basic.t \
    t3002-resource-prefix.t \
    t3003-resource-global.t \
    t3004-resource-excl.t

check_SCRIPTS = $(TESTS)

EXTRA_DIST= \
	$(check_SCRIPTS) \
	data \
    data/resource/jobspecs/basics \
	scripts \
	sharness.sh \
	sharness.d

clean-local:
	rm -rf *.o test-results trash-directory*


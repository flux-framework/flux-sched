AM_CFLAGS = @GCCWARN@ $(CODE_COVERAGE_CFLAGS)
AM_LDFLAGS = $(CODE_COVERAGE_LDFLAGS)

TEST_EXTENSIONS = .t
T_LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) \
	$(top_srcdir)/m4/tap-driver.sh

#
# Set up paths for fluxometer-based Lua tests.
# LUA_PATH is set to find fluxometer.lua itself.
# The PREPEND versions are required so that in-tree versions of
#  modules are found instead of the lua modules in the path
#  of the flux installation we're using for testing.
#
TESTS_ENVIRONMENT = \
    LUA_PATH="$(FLUX_PREFIX)/share/lua/5.1/?.lua;$(LUA_PATH);;" \
    FLUX_LUA_PATH_PREPEND="$(abs_top_srcdir)/rdl/?.lua" \
    FLUX_LUA_CPATH_PREPEND="$(abs_top_builddir)/rdl/?.so" \
    PATH="$(FLUX_PREFIX)/bin:$(PATH)"

TESTS = \
	t0000-sharness.t \
	t0001-basic.t \
	lua/t0001-rdl-basic.t \
	lua/t0002-multilevel.t \
	lua/t0003-default-tags.t \
	lua/t0004-derived-type.t \
	t0002-waitjob.t \
	t1000-jsc.t \
	t1001-rs2rank-basic.t \
	t1002-rs2rank-64ranks.t \
	t1003-stress.t \
	t2000-fcfs.t \
	t2001-fcfs-aware.t \
	t2002-easy.t

check_SCRIPTS = $(TESTS)

EXTRA_DIST= \
	$(check_SCRIPTS) \
	data \
	scripts \
	sharness.sh \
	sharness.d

clean-local:
	rm -rf *.o test-results trash-directory*


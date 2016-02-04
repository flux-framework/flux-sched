AM_CFLAGS = @GCCWARN@

TEST_EXTENSIONS = .t
T_LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) \
	$(top_srcdir)/m4/tap-driver.sh

#
# These are required by the lua tests
#
TESTS_ENVIRONMENT = \
    LUA_PATH="$(abs_top_srcdir)/rdl/?.lua;$(FLUX_PREFIX)/share/lua/5.1/?.lua;$(LUA_PATH);;" \
    LUA_CPATH="$(abs_top_builddir)/rdl/.libs/?.so;$(FLUX_PREFIX)/lib64/lua/5.1/?.so;$(LUA_CPATH);;" \
    FLUXOMETER_LUA_PATH="$(FLUX_PREFIX)/share/lua/5.1/?.lua"

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
	sharness.d

clean-local:
	rm -rf *.o test-results trash-directory*


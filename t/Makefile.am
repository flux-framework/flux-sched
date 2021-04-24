AM_CFLAGS = $(WARNING_CFLAGS) $(CODE_COVERAGE_CFLAGS)
AM_LDFLAGS = $(CODE_COVERAGE_LIBS)

TEST_EXTENSIONS = .t
T_LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) \
	$(top_srcdir)/config/tap-driver.sh

#
#  If $FLUX_SCHED_TEST_INSTALLED is set in the current environment,
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
    FLUX_SCHED_CO_INST=`sh -c 'if [ $(FLUX_PREFIX) = $(prefix) ]; then echo co; fi'` \
    PATH="$(FLUX_PREFIX)/bin:$(PATH)"

TESTS = \
    t0000-sharness.t \
    t1001-qmanager-basic.t \
    t1002-qmanager-reload.t \
    t1003-qmanager-policy.t \
    t1004-qmanager-optimize.t \
    t1005-qmanager-conf.t \
    t1006-qmanager-multiqueue.t \
    t1007-recovery-full.t \
    t1008-recovery-none.t \
    t1009-recovery-multiqueue.t \
    t1010-sync-modules.t \
    t1011-dynstate-change.t \
    t1012-find-status.t \
    t1013-exclusion.t \
    t1013-qmanager-priority.t \
    t1014-annotation.t \
    t1015-find-format.t \
    t1016-nest-namespace.t \
    t1017-rv1-bootstrap.t \
    t1018-rv1-bootstrap2.t \
    t2000-tree-basic.t \
    t2001-tree-real.t \
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
    t3014-resource-var-aware.t \
    t3015-resource-basic-jgf.t \
    t3016-resource-power-jgf.t \
    t3017-resource-granule.t \
    t3018-resource-mtl0.t \
    t3019-resource-mtl1.t \
    t3020-resource-mtl2.t \
    t3021-resource-mtl3.t \
    t3022-resource-update.t \
    t3023-resource-update2.t \
    t3024-resource-status.t \
    t3025-resource-find.t \
    t3026-resource-node-local-storage.t \
    t3026-resource-sibling.t \
    t3027-resource-RV.t \
    t3028-resource-grow.t \
    t3029-resource-prune.t \
    t3030-resource-multi.t \
    t4000-match-params.t \
    t4001-match-allocate.t \
    t4002-match-reserve.t \
    t4003-cancel-info.t \
    t4004-match-hwloc.t \
    t4005-match-unsat.t \
    t4006-properties.t \
    t4007-match-var-aware.t \
    t4008-match-jgf.t \
    t4009-match-update.t \
    t5000-valgrind.t \
    t6000-graph-size.t \
    t6001-match-formats.t \
    t6002-graph-hwloc.t \
    t7000-shell-datastaging.t

check_SCRIPTS = $(TESTS)

EXTRA_DIST= \
	$(check_SCRIPTS) \
	data \
	valgrind \
	data/resource/jobspecs/basics \
	scripts \
	sharness.sh \
	sharness.d \
	schemas \
	conf.d \
	rc

clean-local:
	rm -rf *.o test-results trash-directory*


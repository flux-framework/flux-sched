# NOTE: skipping tree tests, since they're long broken
#  t2000-tree-basic.t
#  t2001-tree-real.t

set(ALL_TESTS
  t0000-sharness.t
  t1001-qmanager-basic.t
  t1002-qmanager-reload.t
  t1003-qmanager-policy.t
  t1004-qmanager-optimize.t
  t1005-qmanager-conf.t
  t1006-qmanager-multiqueue.t
  t1007-recovery-full.t
  t1009-recovery-multiqueue.t
  t1010-sync-modules.t
  t1011-dynstate-change.t
  t1012-find-status.t
  t1013-qmanager-priority.t
  t1014-annotation.t
  t1015-find-format.t
  t1016-nest-namespace.t
  t1017-rv1-bootstrap.t
  t1018-rv1-bootstrap2.t
  t1019-qmanager-async.t
  t1020-qmanager-feasibility.t
  t1021-qmanager-nodex.t
  t1022-property-constraints.t
  t1023-multiqueue-constraints.t
  t1024-alloc-check.t
  t1025-rv1-reload.t
  t1026-rv1-partial-release.t
  t1027-rv1-partial-release-brokerless-resources.t
  t2317-resource-shrink.t
  t3000-jobspec.t
  t3001-resource-basic.t
  t3002-resource-prefix.t
  t3003-resource-global.t
  t3004-resource-excl.t
  t3005-resource-rsv.t
  t3006-resource-advanced.t
  t3007-resource-iobw.t
  t3008-resource-cancel.t
  t3009-resource-minmax.t
  t3010-resource-power.t
  t3011-resource-filt.t
  t3012-resource-properties.t
  t3013-resource-unsat.t
  t3014-resource-var-aware.t
  t3015-resource-basic-jgf.t
  t3016-resource-power-jgf.t
  t3017-resource-granule.t
  t3018-resource-mtl0.t
  t3019-resource-mtl1.t
  t3020-resource-mtl2.t
  t3021-resource-mtl3.t
  t3022-resource-update.t
  t3023-resource-update2.t
  t3024-resource-status.t
  t3025-resource-find.t
  t3026-resource-node-local-storage.t
  t3026-resource-sibling.t
  t3027-resource-RV.t
  t3028-resource-grow.t
  t3029-resource-prune.t
  t3030-resource-multi.t
  t3031-resource-minmax2.t
  t3033-resource-nodex.t
  t3034-resource-pconstraints.t
  t3035-resource-remove.t
  t3037-resource-custom-policy.t
  t3300-system-dontblock.t
  t3301-system-latestart.t
  t4000-match-params.t
  t4001-match-allocate.t
  t4002-match-reserve.t
  t4003-cancel-info.t
  t4004-match-hwloc.t
  t4005-match-unsat.t
  t4006-properties.t
  t4007-match-var-aware.t
  t4008-match-jgf.t
  t4009-match-update.t
  t4010-match-conf.t
  t4011-match-duration.t
  t4012-set-status.t
  t4013-unreservable.sh
  t5000-valgrind.t
  t5100-issues-test-driver.t
  t6000-graph-size.t
  t6001-match-formats.t
  t6002-graph-hwloc.t
  t8001-util-ion-R.t
  t9001-golang-basic.t
  python/t10001-resourcegraph.py
  )
foreach(test ${ALL_TESTS})
  flux_add_test(NAME ${test}
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/${test} -v
    )
endforeach()

add_custom_target(recheck
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  COMMAND ${CMAKE_CTEST_COMMAND} --rerun-failed --output-on-failure)

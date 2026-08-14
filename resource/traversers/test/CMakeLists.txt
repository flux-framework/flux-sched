add_executable(flexible_split_test
  ${CMAKE_CURRENT_SOURCE_DIR}/flexible_split_test.cpp
  )
target_link_libraries(flexible_split_test PRIVATE resource Catch2::Catch2WithMain)
target_include_directories(flexible_split_test PRIVATE
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/resource
  )
target_compile_definitions(flexible_split_test PRIVATE
  CATCH_CONFIG_FAST_COMPILE=1
  FLEXIBLE_JOBSPEC_DIR="${CMAKE_SOURCE_DIR}/t/data/resource/jobspecs/flexible"
  )
add_sanitizers(flexible_split_test)
catch_discover_tests(flexible_split_test
  TEST_PREFIX "resource traversers "
  )

add_executable(jobmeta_test
  ${CMAKE_CURRENT_SOURCE_DIR}/jobmeta_test.cpp
  )
target_link_libraries(jobmeta_test PRIVATE resource Catch2::Catch2WithMain)
target_include_directories(jobmeta_test PRIVATE
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/resource
  )
target_compile_definitions(jobmeta_test PRIVATE
  CATCH_CONFIG_FAST_COMPILE=1
  )
add_sanitizers(jobmeta_test)
# PRE_TEST discovery is required so the trailing space in TEST_PREFIX
# survives: the default POST_BUILD mode forwards the prefix via `cmake -D`,
# which strips trailing whitespace and glues the prefix onto the case name.
# Embedding the target name in the prefix lets `ctest -R jobmeta_test` work.
catch_discover_tests(jobmeta_test
  TEST_PREFIX "resource traversers jobmeta_test: "
  DISCOVERY_MODE PRE_TEST
  )

add_executable(by_jobid_index_test
  ${CMAKE_CURRENT_SOURCE_DIR}/by_jobid_index_test.cpp
  )
target_link_libraries(by_jobid_index_test PRIVATE resource Catch2::Catch2WithMain)
target_include_directories(by_jobid_index_test PRIVATE
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/resource
  )
target_compile_definitions(by_jobid_index_test PRIVATE
  CATCH_CONFIG_FAST_COMPILE=1
  )
add_sanitizers(by_jobid_index_test)
# See the jobmeta_test note above: PRE_TEST discovery is required so the
# trailing space in TEST_PREFIX survives and `ctest -R by_jobid_index_test`
# works.
catch_discover_tests(by_jobid_index_test
  TEST_PREFIX "resource traversers by_jobid_index_test: "
  DISCOVERY_MODE PRE_TEST
  )

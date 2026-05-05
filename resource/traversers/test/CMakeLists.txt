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

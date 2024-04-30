add_executable(matcher_policy_factory_test
  ${CMAKE_CURRENT_SOURCE_DIR}/matcher_policy_factory_test02.cpp
  )
target_link_libraries(matcher_policy_factory_test PRIVATE libtap resource)
add_sanitizers(matcher_policy_factory_test)
flux_add_test(NAME matcher_policy_factory_test COMMAND matcher_policy_factory_test)
add_executable(matcher_util_api_test
  ${CMAKE_CURRENT_SOURCE_DIR}/matcher_util_api_test01.cpp
  )
target_link_libraries(matcher_util_api_test PRIVATE libtap resource
  PkgConfig::JANSSON
  )
add_sanitizers(matcher_util_api_test)
flux_add_test(NAME matcher_util_api_test COMMAND matcher_util_api_test)

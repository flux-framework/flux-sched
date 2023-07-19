add_executable(matcher_util_api_test
  ${CMAKE_CURRENT_SOURCE_DIR}/matcher_util_api_test01.cpp
  )
target_link_libraries(matcher_util_api_test PRIVATE libtap resource
  PkgConfig::JANSSON
  )
flux_add_test(NAME matcher_util_api_test COMMAND matcher_util_api_test)

add_executable(matcher_util_api_test
        matcher_util_api_test01.cpp
  )
target_link_libraries(matcher_util_api_test PRIVATE libtap resource
  PkgConfig::JANSSON
  )
add_sanitizers(matcher_util_api_test)
flux_add_test(NAME matcher_util_api_test COMMAND matcher_util_api_test)

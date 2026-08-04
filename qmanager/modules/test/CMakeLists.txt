add_executable(qmanager_opts_test
  ${CMAKE_CURRENT_SOURCE_DIR}/qmanager_opts_test.cpp
  ${CMAKE_SOURCE_DIR}/qmanager/modules/qmanager_opts.cpp
  )
target_include_directories(qmanager_opts_test PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(qmanager_opts_test
  PRIVATE Catch2::Catch2WithMain PkgConfig::JANSSON)
add_sanitizers(qmanager_opts_test)
flux_add_test(NAME qmanager_opts_test COMMAND qmanager_opts_test)

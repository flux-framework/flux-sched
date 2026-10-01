# catch2 is managed in cmake/config_testing.cmake to ensure its scope
# covers all test builds
add_library(boost_scope INTERFACE)
if (Boost_VERSION VERSION_LESS 1.85.0)
  message(STATUS "Older boost version found, using vendored scope")
  target_include_directories(boost_scope INTERFACE scope/include)
else ()
  message(STATUS "Using main boost to provide scope")
  target_link_libraries(boost_scope INTERFACE Boost::boost)
endif ()

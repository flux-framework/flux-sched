add_executable(grug2dot
  grug2dot.cpp
  )
add_sanitizers(grug2dot)
target_link_libraries(grug2dot
  resource
  Boost::headers
  )
add_executable(resource-query
  resource-query.cpp
  command.cpp
  command.hpp
  )
add_sanitizers(resource-query)
target_link_libraries(resource-query
  resource
  PkgConfig::LIBEDIT
  PkgConfig::JANSSON
  PkgConfig::UUID
  Boost::headers
  )

add_subdirectory(test)


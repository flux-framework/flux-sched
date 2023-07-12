add_executable(grug2dot
  grug2dot.cpp
  )
target_link_libraries(grug2dot
  resource
  )
add_executable(resource-query
  resource-query.cpp
  command.cpp
  command.hpp
  )
target_link_libraries(resource-query
  PRIVATE
  resource
  PkgConfig::LIBEDIT
  PkgConfig::JANSSON
  PkgConfig::UUID
  )

install(
  TARGETS
  grug2dot
  resource-query
  )

add_subdirectory(test)


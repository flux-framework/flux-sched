set(libplanner_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/planner.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/scheduled_point_tree.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/mintime_resource_tree.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/planner_multi.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/planner.h
	${CMAKE_CURRENT_SOURCE_DIR}/mintime_resource_tree.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/planner_internal_tree.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/scheduled_point_tree.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/planner_multi.h
)
add_library(planner STATIC ${libplanner_SOURCES} )
target_link_libraries(planner yggdrasil)

add_subdirectory(test)

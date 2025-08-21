add_library(planner_cxx STATIC 
	./planner.cpp
	./scheduled_point_tree.cpp
	./mintime_resource_tree.cpp
	./planner_multi.cpp
	./planner_internal_tree.cpp
	./mintime_resource_tree.hpp
	./planner_internal_tree.hpp
	./scheduled_point_tree.hpp
 )
add_sanitizers(planner_cxx) 
target_link_libraries(planner_cxx yggdrasil intern)



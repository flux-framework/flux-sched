add_library(planner STATIC
  ./planner_c_interface.cpp
  ./planner_multi_c_interface.cpp
	./planner.h
	./planner_multi.h
)
add_sanitizers(planner)
target_include_directories(planner PUBLIC ./)
target_link_libraries(planner PUBLIC planner_cxx)


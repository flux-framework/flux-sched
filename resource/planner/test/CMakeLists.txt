add_executable(planner_test1 planner_test01.cpp)
target_link_libraries(planner_test1 planner libtap)
flux_add_test(NAME planner_test1 COMMAND planner_test1)
set_property(TEST planner_test1 PROPERTY ENVIRONMENT "TESTRESRC_INPUT_FILE=$(CMAKE_SOURCE_DIR)/conf/hype.lua")

add_executable(planner_test2 planner_test02.cpp)
target_link_libraries(planner_test2 planner libtap)
flux_add_test(NAME planner_test2 COMMAND planner_test1)
set_property(TEST planner_test2 PROPERTY ENVIRONMENT "TESTRESRC_INPUT_FILE=$(CMAKE_SOURCE_DIR)/conf/hype.lua")

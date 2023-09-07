add_executable(schema_test01 ./schema_test01.cpp)
target_link_libraries(schema_test01 resource libtap)
target_include_directories(schema_test01 PUBLIC ../)

add_executable(schema_test02 ./schema_test02.cpp)
target_link_libraries(schema_test02 resource libtap)
target_include_directories(schema_test02 PUBLIC ../)

add_library(cppwrappers INTERFACE)
target_include_directories(cppwrappers INTERFACE .)
add_subdirectory( test )

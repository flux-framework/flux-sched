add_library(cppwrappers INTERFACE
        eh_wrapper.hpp
        jansson.hpp)
target_include_directories(cppwrappers INTERFACE .)
add_subdirectory(test)

add_library(intern STATIC
        interner.cpp)

add_executable(interned_string_test test/interned_string_test.cpp)
add_sanitizers(interned_string_test)
target_link_libraries(interned_string_test intern Catch2::Catch2WithMain)
catch_discover_tests (interned_string_test)
target_include_directories(interned_string_test PUBLIC .)

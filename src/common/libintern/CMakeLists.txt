add_library(intern SHARED
        interner.cpp)
install(TARGETS intern
        LIBRARY)

add_executable(interned_string_test test/interned_string_test.cpp)
target_link_libraries(interned_string_test intern Catch2::Catch2WithMain)
catch_discover_tests (interned_string_test)
target_include_directories(interned_string_test PUBLIC .)

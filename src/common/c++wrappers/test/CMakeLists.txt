add_executable(exception_safe_wrapper_test01 exception_safe_wrapper_test01.cpp)
add_sanitizers(exception_safe_wrapper_test01)
target_link_libraries(exception_safe_wrapper_test01 PRIVATE cppwrappers
  Catch2::Catch2WithMain)

target_compile_definitions(exception_safe_wrapper_test01 PRIVATE
  CATCH_CONFIG_FAST_COMPILE=1)
catch_discover_tests(exception_safe_wrapper_test01
  TEST_PREFIX "exception_safe_wrapper "
  PROPERTIES ENVIRONMENT "${TEST_ENV}")

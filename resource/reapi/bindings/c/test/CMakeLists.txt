add_executable(reapi_cli_test reapi_cli_test.c)
add_sanitizers(reapi_cli_test)
target_link_libraries(reapi_cli_test PRIVATE reapi_cli libtap PkgConfig::JANSSON)
flux_add_test(NAME reapi_cli_test COMMAND reapi_cli_test)

flux_add_test(NAME run_sanity_check COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/run_sanity_check.sh)
set_property(TEST run_sanity_check PROPERTY ENVIRONMENT "srcdir=${CMAKE_CURRENT_SOURCE_DIR}")

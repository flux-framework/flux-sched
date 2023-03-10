set(SRC_PATH "${CMAKE_CURRENT_SOURCE_DIR}")
configure_file(run_sanity_check.sh.in run_sanity_check.sh)
unset(SRC_PATH)
add_test(NAME run_sanity_check COMMAND bash run_sanity_check.sh)

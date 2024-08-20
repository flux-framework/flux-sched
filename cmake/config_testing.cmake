if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  include(CTest)
endif()

# Try to use installed catch2 to save build time, require at least version 3
find_package(Catch2 3 CONFIG)
if (NOT Catch2_FOUND)
    add_subdirectory(external/catch2)
endif ()
include(Catch)

set(TEST_ENV)
set(FLUX_CO_INST "")
if(FLUX_PREFIX EQUAL CMAKE_INSTALL_PREFIX)
  set(FLUX_CO_INST "co")
endif()
list(APPEND TEST_ENV "FLUX_SCHED_CO_INST=${FLUX_CO_INST}")
list(APPEND TEST_ENV "PATH=${FLUX_PREFIX}/bin:$ENV{PATH}")
list(APPEND TEST_ENV "SHARNESS_TEST_DIRECTORY=${CMAKE_BINARY_DIR}/t")
list(APPEND TEST_ENV "SHARNESS_TEST_SRCDIR=${CMAKE_SOURCE_DIR}/t")
list(APPEND TEST_ENV "SHARNESS_BUILD_DIRECTORY=${CMAKE_BINARY_DIR}")
list(APPEND TEST_ENV "FLUX_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}")
list(APPEND TEST_ENV "FLUX_SCHED_MOD_DIR=${FLUX_MOD_DIR}")
list(APPEND TEST_ENV "FLUX_SCHED_EXEC_DIR=${FLUX_CMD_DIR}")
list(APPEND TEST_ENV "FLUX_SCHED_PYTHON_SITELIB=${PYTHON_INSTALL_SITELIB}")

# add maybe-installtest to the start
function(flux_config_test)
  set(options "")
  set(oneValueArgs NAME COMMAND)
  set(multiValueArgs "")

  cmake_parse_arguments(PARSE_ARGV 0 ARG
    "${options}" "${oneValueArgs}" "${multiValueArgs}")
  set_property(TEST ${ARG_NAME} PROPERTY ENVIRONMENT "${TEST_ENV}")
endfunction()
find_program(TEST_SHELL NAMES bash zsh sh)
function(flux_add_test)
  # This odd construction is as close as we can get to
  # generic argument forwarding
  set(options "")
  set(oneValueArgs NAME COMMAND)
  set(multiValueArgs "")

  cmake_parse_arguments(PARSE_ARGV 0 ARG
    "${options}" "${oneValueArgs}" "${multiValueArgs}")

  set(__argsQuoted)
  foreach(__item IN LISTS ARG_UNPARSED_ARGUMENTS)
    string(APPEND __argsQuoted " [==[${__item}]==]")
  endforeach()
  if(TARGET ${ARG_COMMAND})
      # if it's a target, pull the path from the target
      set(ARG_COMMAND "$<TARGET_FILE:${ARG_COMMAND}>")
  endif()
  cmake_language(EVAL CODE "
  add_test(
      NAME ${ARG_NAME}
      COMMAND ${TEST_SHELL} ${CMAKE_SOURCE_DIR}/t/scripts/maybe-installtest ${ARG_COMMAND}
      ${__argsQuoted}
    ) ")
    flux_config_test(NAME ${ARG_NAME})
endfunction()


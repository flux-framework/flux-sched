cmake_minimum_required(VERSION 3.18)
set(VER_FILE ${CMAKE_SOURCE_DIR}/flux-sched.ver)
if(DEFINED FLUX_SCHED_VER)
  # do nothing, user said so
elseif(DEFINED ENV{FLUX_SCHED_VERSION})
  set(FLUX_SCHED_VER "$ENV{FLUX_SCHED_VERSION}")
elseif(EXISTS ${VER_FILE})
  file(READ ${VER_FILE} FLUX_SCHED_VER)
  string(STRIP ${FLUX_SCHED_VER} FLUX_SCHED_VER)
else()
  execute_process(COMMAND sh -c [=[git describe --always | awk '/.*/ {sub(/^v/, ""); printf "%s",$1; exit}']=] OUTPUT_VARIABLE FLUX_SCHED_VER)
endif()
message(STATUS "VER ${FLUX_SCHED_VER}")
string(REGEX REPLACE "-.*$" "" FLUX_SCHED_VER_NOGIT "${FLUX_SCHED_VER}")
# I took this regex from the CMake source code. If the version fails to
# match it, CMake will error out. We can help the end-user get past this
# with a few tips.
string(REGEX MATCH "(^([0-9]+(\.[0-9]+(\.[0-9]+(\.[0-9]+)?)?)?)?$)" CHECK_FLUX_SCHED_VER "${FLUX_SCHED_VER_NOGIT}")
string(LENGTH "${CHECK_FLUX_SCHED_VER}" LENGTH_FLUX_SCHED_VER)
if (LENGTH_FLUX_SCHED_VER STREQUAL "0")
    message(WARNING 
    "CMake may generate an error that says \"VERSION \"${FLUX_SCHED_VER}\" format invalid.\"\n"
    "If this happens, try the following:\n"
    "    1. Set the variable manually, with `cmake -DFLUX_SCHED_VER=<version> ...`\n"
    "       or FLUX_SCHED_VERSION=<version> in your environment.\n"
    "    2. If you are running in a CI environment, run `git fetch --tags`\n"
    "       before building. Versions in flux-sched are derived from \n"
    "       `git describe` which uses the most recent tag.\n"
    "    3. If you are running remote CI in a fork of the main repository,\n"
    "       try pushing the upstream tags to your fork with\n"
    "       `git push --tags <your_remote>` to make sure tags are\n"
    "       synchronized in your fork.\n"
    "    4. If you are running outside of a repo (such as in a buildfarm),\n"
    "       add a file to the source directory called flux-sched.ver and\n"
    "       place a valid version string in that file."
    )
endif()
project(flux-sched VERSION ${FLUX_SCHED_VER_NOGIT} LANGUAGES CXX C)
message(STATUS "Building flux-sched version ${FLUX_SCHED_VER}")

if(POLICY CMP0140)
  cmake_policy(SET CMP0140 NEW)
endif()

set(CMAKE_CXX_STANDARD 23 CACHE STRING "The C++ standard to use")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# check compiler versions
if(NOT FLUX_IGNORE_COMPILER_SUPPORT_AND_FIX_THIS_MYSELF)
  set(CLANG_VER "15")
  set(GCC_VER "12")
  set(COMPILER_MESSAGE "\
  Insufficient compiler version, gcc@${GCC_VER} or clang@${CLANG_VER}
  or higher are the supported compilers.  To ignore these requirements
  (at your own peril) set FLUX_IGNORE_COMPILER_SUPPORT_AND_FIX_THIS_MYSELF=On")
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS ${GCC_VER})
      message(FATAL_ERROR ${COMPILER_MESSAGE})
    endif()
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS ${CLANG_VER})
      message(FATAL_ERROR ${COMPILER_MESSAGE})
    endif()
  endif()
endif()

configure_file(config.h.in config.h)

include( GNUInstallDirs ) # convenience names for gnu-style directories
# RPATH setup
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH ON)
set( CMAKE_BUILD_RPATH
  ${CMAKE_BINARY_DIR}/resource:${CMAKE_BINARY_DIR}/src/common/libtap )
# the RPATH to be used when installing, but only if it's not a system directory
list(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/lib" isSystemDir)
if("${isSystemDir}" STREQUAL "-1")
  set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib64:${CMAKE_INSTALL_PREFIX}/lib")
endif("${isSystemDir}" STREQUAL "-1")

# Setting the include directory for the application to find config.h
include_directories( ${CMAKE_BINARY_DIR} )
# Since we have created a config.h add a global define for it
add_definitions( "-DHAVE_CONFIG_H" )
add_definitions( "-DPACKAGE_VERSION=\"${FLUX_SCHED_VER}\"" )

# We build a lot of shared libs, build them all with PIC
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/sanitizers-cmake/cmake)
list(APPEND CMAKE_PREFIX_PATH ${CMAKE_INSTALL_PREFIX})

include(FindSanitizers)

# external dependencies
find_package(PkgConfig REQUIRED)
set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:${CMAKE_INSTALL_PREFIX}/lib/pkgconfig")
find_package(FluxCore REQUIRED)
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  message(STATUS "flux core found and no specific prefix specified, using ${FLUX_CORE_PREFIX}")
  set(CMAKE_INSTALL_PREFIX ${FLUX_CORE_PREFIX} CACHE PATH "..." FORCE)
endif()
if("${FLUX_CORE_LIBDIR}" MATCHES "^${FLUX_CORE_PREFIX}/(.+)$")
  set(FLUX_CORE_LIBEXT "${CMAKE_MATCH_1}")
else()
  set(FLUX_CORE_LIBEXT "${CMAKE_INSTALL_LIBDIR}")
endif()
set(CMAKE_INSTALL_LIBDIR ${FLUX_CORE_LIBEXT} CACHE PATH "..." FORCE)
message(STATUS "Forcing libdir selection to match flux-core: prefix subdir (${FLUX_CORE_LIBEXT})")
message(STATUS "    flux-core library path: (${FLUX_CORE_LIBDIR})")
message(STATUS "    flux-sched library path: (${CMAKE_INSTALL_PREFIX}/${FLUX_CORE_LIBEXT})")
if(DEFINED ENV{PYTHON})
  set(Python_EXECUTABLE $ENV{PYTHON} CACHE FILEPATH "")
endif()
find_package(Python 3.6 COMPONENTS Interpreter Development REQUIRED) # 3.6 or higher
execute_process(
  COMMAND "${Python_EXECUTABLE}" -c "import sysconfig as sc; print(sc.get_path('purelib', vars={'base':'${CMAKE_INSTALL_PREFIX}'}))"
  OUTPUT_VARIABLE PYTHON_INSTALL_SITELIB
  OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(
  COMMAND "${Python_EXECUTABLE}" -c "import sysconfig as sc; print(sc.get_path('platlib', vars={'platbase':'${CMAKE_INSTALL_PREFIX}'}))"
  OUTPUT_VARIABLE PYTHON_INSTALL_SITEARCH
  OUTPUT_STRIP_TRAILING_WHITESPACE)
pkg_check_modules(yaml-cpp REQUIRED IMPORTED_TARGET yaml-cpp)
pkg_check_modules(LIBEDIT REQUIRED IMPORTED_TARGET libedit)
pkg_check_modules(HWLOC REQUIRED IMPORTED_TARGET hwloc>=2)
pkg_check_modules(JANSSON REQUIRED IMPORTED_TARGET jansson>=2.10)
pkg_check_modules(UUID REQUIRED IMPORTED_TARGET uuid)

set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost CONFIG 1.66 COMPONENTS
  graph
)
# if the upstream boost cmake config is not found, fall back
# TODO: remove this when we require 1.70+
if (NOT Boost_FOUND)
  find_package(Boost 1.66 REQUIRED COMPONENTS
    graph
    )
endif ()
message(STATUS "Boost version: ${Boost_VERSION}")

# Install paths
set(FLUX_CMD_DIR "${CMAKE_INSTALL_LIBEXECDIR}/flux/cmd")
set(FLUX_LIB_DIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/flux")
set(FLUX_MOD_DIR "${FLUX_LIB_DIR}/modules")
set(FLUX_SHELL_PLUGIN_DIR "${FLUX_LIB_DIR}/shell/plugins")
set(FLUX_RC1_DIR "${CMAKE_INSTALL_SYSCONFDIR}/flux/rc1.d")
set(FLUX_RC3_DIR "${CMAKE_INSTALL_SYSCONFDIR}/flux/rc3.d")

include(config_testing)
### HELPERS
function(flux_find_python_module Name Version)
  execute_process(
    COMMAND ${Python_EXECUTABLE} -c "import ${Name}; print(${Name}.__version__)"
    RESULT_VARIABLE EXIT_CODE
    OUTPUT_VARIABLE MOD_VER
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    # COMMAND_ECHO STDOUT
    )
  if(MOD_VER VERSION_GREATER_EQUAL Version)
    set(${Name}_MOD $MOD_VER PARENT_SCOPE)
  else()
    set(${Name}_MOD NOTFOUND PARENT_SCOPE)
    message(FATAL_ERROR "Python module ${Name} not found or version too low, need ${Version} got ${MOD_VER}")
  endif()
endfunction()
function(flux_add_plugin TargetName PluginType)
  # This odd construction is as close as we can get to
  # generic argument forwarding
  set(options "NOINSTALL")
  set(oneValueArgs "")
  set(multiValueArgs "")

  if(PluginType STREQUAL "MODULE")
    set(LinkerOpts "LINKER:--no-undefined")
    set(InstallDest ${FLUX_MOD_DIR})
  elseif(PluginType STREQUAL "SHELL")
    set(InstallDest ${FLUX_SHELL_PLUGIN_DIR})
  endif()

  cmake_parse_arguments(PARSE_ARGV 2 ARG
    "${options}" "${oneValueArgs}" "${multiValueArgs}")

  set(__argsQuoted)
  foreach(__item IN LISTS ARG_UNPARSED_ARGUMENTS)
    string(APPEND __argsQuoted " [==[${__item}]==]")
  endforeach()
  cmake_language(EVAL CODE "
  add_library(
    ${TargetName}
    ${LibFlags}
    MODULE
    # List these last so they can override things we set above
    ${__argsQuoted}
    )"
    )
  target_link_options(${TargetName} PRIVATE
    "LINKER:--version-script=${CMAKE_SOURCE_DIR}/flux-plugin.map" ${LinkerOpts})
  add_sanitizers(${TargetName})
  target_link_libraries(${TargetName} PRIVATE flux::core)
  if (NOT ARG_NOINSTALL)
    install(TARGETS ${TargetName}
      LIBRARY DESTINATION ${InstallDest})
  endif()
  set_property(TARGET ${TargetName} PROPERTY PREFIX "")
endfunction()
### END HELPERS
flux_find_python_module(yaml 3.10)
flux_find_python_module(jsonschema 2.3.0)

if(ENABLE_COVERAGE)
  include(CodeCoverage)

  SET(COVERAGE_FLAGS "--coverage -g -fprofile-arcs -ftest-coverage")
  SET( CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} ${COVERAGE_FLAGS}" )
  SET( CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${COVERAGE_FLAGS}" )
  SET( CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} ${COVERAGE_FLAGS}" )
endif()

if(DEFINED ENV{WITH_GO})
  message(STATUS "WITH_GO detected in main CMakeLists.txt to build go bindings")
  include(GolangSimple)
endif()

include(FindValgrind) # Search for Valgrind

include_directories(.)
add_subdirectory( etc )
add_subdirectory( src )
add_subdirectory( resource )
add_subdirectory( qmanager )
add_subdirectory( doc )
add_subdirectory( t )

if(ENABLE_COVERAGE)
  blt_add_code_coverage_target(NAME flux-sched-coverage
    DEPENDS check
    RUNNER ${CMAKE_CTEST_COMMAND}
    REMOVE "*/include/boost/*" "/nix/store/*" "*/include/boost/*" "*/src/common/libutil/*" "*/src/common/libtap/*" "*/src/common/yggdrasil/*")
  add_custom_target(check-code-coverage
    DEPENDS flux-sched-coverage)
endif()
set(CTEST_COMMON_FLAGS --output-on-failure --output-junit
  ${CMAKE_CURRENT_BINARY_DIR}/test-results.xml)
add_custom_target(check
  DEPENDS ${check_targets}
  COMMAND ${CMAKE_CTEST_COMMAND} ${CTEST_COMMON_FLAGS}
  )
add_custom_target(installcheck
  COMMAND env FLUX_SCHED_TEST_INSTALLED=1 ${CMAKE_CTEST_COMMAND} ${CTEST_COMMON_FLAGS})
add_custom_target(dist
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  COMMAND echo ${FLUX_SCHED_VER} > ${CMAKE_BINARY_DIR}/flux-sched.ver
  COMMAND git archive --format=tar.gz
  --prefix=flux-sched-${FLUX_SCHED_VER}/
  --add-file=${CMAKE_BINARY_DIR}/flux-sched.ver
  --output=${CMAKE_BINARY_DIR}/flux-sched-${FLUX_SCHED_VER}.tar.gz
  HEAD .
  COMMAND rm ${CMAKE_BINARY_DIR}/flux-sched.ver
  COMMENT "Generated flux-sched-${FLUX_SCHED_VER}.tar.gz"
  )
# run installcheck, if it passes then write out version information and pack up
# a tarball with the result
add_custom_target(distcheck
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target all
  COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target check
  COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target dist
  )
add_custom_target(deb
  COMMAND ${CMAKE_SOURCE_DIR}/scripts/debbuild.sh ${CMAKE_SOURCE_DIR}
  )

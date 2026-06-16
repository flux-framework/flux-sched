# Find Python 3
find_package(Python3 COMPONENTS Interpreter Development REQUIRED)

# Find Cython  with python -m cython
execute_process(
    COMMAND ${Python3_EXECUTABLE} -m cython --version
    RESULT_VARIABLE CYTHON_CHECK_RESULT
    OUTPUT_QUIET
    ERROR_QUIET
)

if(NOT CYTHON_CHECK_RESULT EQUAL 0)
    message(FATAL_ERROR "Cython not found. Please install it: ${Python3_EXECUTABLE} -m pip install Cython")
endif()

set(CYTHON_COMMAND ${Python3_EXECUTABLE} -m cython)

# Set up Include Directories
# We need the top level for config.h and the bindings directory for headers
include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/resource/reapi/bindings
)

# ==============================================================================
# Build flux_sched.reapi_cli
# ==============================================================================

# Step A: Generate C++ from Cython
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/reapi_cli.cpp
    COMMAND ${CYTHON_COMMAND} --cplus -3
            -I ${CMAKE_CURRENT_SOURCE_DIR}
            -o ${CMAKE_CURRENT_BINARY_DIR}/reapi_cli.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/flux_sched/reapi_cli.pyx
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/flux_sched/reapi_cli.pyx
            ${CMAKE_CURRENT_SOURCE_DIR}/flux_sched/c_reapi.pxd
    COMMENT "Cythonizing flux_sched/reapi_cli.pyx"
)

# Create the Python Extension Module
add_library(python_reapi_cli MODULE ${CMAKE_CURRENT_BINARY_DIR}/reapi_cli.cpp)

# Link against the existing C++ library 'reapi_cli'
# This pulls in 'resource', 'jobspec_conv', and 'flux::core' automatically
target_link_libraries(python_reapi_cli PRIVATE
    reapi_cli
    Python3::Python
)

# Set output name to 'flux_sched/reapi_cli.so'
set_target_properties(python_reapi_cli PROPERTIES
    PREFIX ""
    SUFFIX ".so"  # Force .so extension for Python consistency
    OUTPUT_NAME "flux_sched/reapi_cli"
)

# ==============================================================================
# Build flux_sched.reapi_module
# ==============================================================================

# Generate C++ from Cython
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/reapi_module.cpp
    COMMAND ${CYTHON_COMMAND} --cplus -3
            -I ${CMAKE_CURRENT_SOURCE_DIR}
            -o ${CMAKE_CURRENT_BINARY_DIR}/reapi_module.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/flux_sched/reapi_module.pyx
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/flux_sched/reapi_module.pyx
            ${CMAKE_CURRENT_SOURCE_DIR}/flux_sched/c_reapi.pxd
    COMMENT "Cythonizing flux_sched/reapi_module.pyx"
)

# Create the Python Extension Module
add_library(python_reapi_module MODULE ${CMAKE_CURRENT_BINARY_DIR}/reapi_module.cpp)

# Link against the static library 'reapi_module'
target_link_libraries(python_reapi_module PRIVATE
    reapi_module
    Python3::Python
)

# Step D: Set output name to 'flux_sched/reapi_module.so'
set_target_properties(python_reapi_module PROPERTIES
    PREFIX ""
    SUFFIX ".so"
    OUTPUT_NAME "flux_sched/reapi_module"
)

# ==============================================================================
# Installation
# ==============================================================================

# Determine the install path for site-packages
if(NOT DEFINED PYTHON_SITE_PACKAGES)
    set(PYTHON_SITE_PACKAGES lib/python${Python3_VERSION_MAJOR}.${Python3_VERSION_MINOR}/site-packages)
endif()

# Install the python source files (__init__.py, etc)
install(DIRECTORY flux_sched
    DESTINATION ${PYTHON_SITE_PACKAGES}
    FILES_MATCHING PATTERN "*.py" PATTERN "*.pxd"
)

# Install the compiled extensions (.so files)
install(TARGETS python_reapi_cli python_reapi_module
    DESTINATION ${PYTHON_SITE_PACKAGES}/flux_sched
)

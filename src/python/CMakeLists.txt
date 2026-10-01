
# Build CFFI extension module for reapi_cli bindings
set(REAPI_CLI_BUILD_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/_fluxion/_reapi_cli_build.py")
set(REAPI_CLI_C_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/_reapi_cli.c")

# Generate C source from CFFI build script
add_custom_command(
  OUTPUT ${REAPI_CLI_C_SOURCE}
  COMMAND ${Python_EXECUTABLE} ${REAPI_CLI_BUILD_SCRIPT}
  DEPENDS ${REAPI_CLI_BUILD_SCRIPT}
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMENT "Generating _reapi_cli.c from CFFI build script"
)

# Build the Python extension module
Python_add_library(_reapi_cli MODULE ${REAPI_CLI_C_SOURCE})
target_link_libraries(_reapi_cli PRIVATE reapi_cli)
target_include_directories(_reapi_cli PRIVATE
  ${CMAKE_SOURCE_DIR}/resource/reapi/bindings/c
)
# Set output name without lib prefix and configure RPATH
set_target_properties(_reapi_cli PROPERTIES
  PREFIX ""
  OUTPUT_NAME "_reapi_cli"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/_fluxion"
  INSTALL_RPATH "${FLUX_LIB_DIR}"
)

# Copy Python source files to build tree for build-tree testing
set(FLUXION_PY_FILES
  fluxion/__init__.py
  fluxion/resourcegraph/V1.py
  fluxion/resourcegraph/__init__.py
  fluxion/jsongraph/objects/edge.py
  fluxion/jsongraph/objects/graph.py
  fluxion/jsongraph/objects/__init__.py
  fluxion/jsongraph/objects/multigraph.py
  fluxion/jsongraph/objects/node.py
  fluxion/reapi/__init__.py
  fluxion/reapi/cli.py
  _fluxion/__init__.py
  fluxion/pool/jgf.py
  fluxion/pool/rv1exec.py
  fluxion/pool/__init__.py
)
foreach(py_file ${FLUXION_PY_FILES})
  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/${py_file}
    ${CMAKE_CURRENT_BINARY_DIR}/${py_file}
    COPYONLY
  )
endforeach()

# Install the extension module
install(TARGETS _reapi_cli
  LIBRARY DESTINATION "${PYTHON_INSTALL_SITELIB}/_fluxion"
)

install(DIRECTORY fluxion
  DESTINATION "${PYTHON_INSTALL_SITELIB}"
  FILES_MATCHING PATTERN "*.py")

# Byte-compile installed Python files
install(CODE "
  message(STATUS \"Byte-compiling Python modules...\")
  execute_process(
    COMMAND ${Python_EXECUTABLE} -m compileall -q
      \$ENV{DESTDIR}${PYTHON_INSTALL_SITELIB}/fluxion
      \$ENV{DESTDIR}${PYTHON_INSTALL_SITELIB}/_fluxion
  )
  execute_process(
    COMMAND ${Python_EXECUTABLE} -O -m compileall -q
      \$ENV{DESTDIR}${PYTHON_INSTALL_SITELIB}/fluxion
      \$ENV{DESTDIR}${PYTHON_INSTALL_SITELIB}/_fluxion
  )
  execute_process(
    COMMAND ${Python_EXECUTABLE} -OO -m compileall -q
      \$ENV{DESTDIR}${PYTHON_INSTALL_SITELIB}/fluxion
      \$ENV{DESTDIR}${PYTHON_INSTALL_SITELIB}/_fluxion
  )
")

install(DIRECTORY _fluxion
  DESTINATION "${PYTHON_INSTALL_SITELIB}"
  FILES_MATCHING PATTERN "*.py"
  PATTERN "*_build.py" EXCLUDE)

set(PYNAME "python${Python_VERSION_MAJOR}.${Python_VERSION_MINOR}")
set(PYDIR "${CMAKE_INSTALL_FULL_LIBDIR}/flux/${PYNAME}")
set(LINKTARGET_FLUXION "${PYTHON_INSTALL_SITELIB}/fluxion")
set(LINKTARGET_FLUXION_EXT "${PYTHON_INSTALL_SITELIB}/_fluxion")

INSTALL(CODE "\
  execute_process( \
  COMMAND \
    mkdir -p \$ENV{DESTDIR}/${PYDIR} \
  )
  execute_process( \
  COMMAND \
    rm -f \$ENV{DESTDIR}/${PYDIR}/fluxion \
  )
  MESSAGE (STATUS \
    \"linking ${PYDIR}/fluxion -> ${PYTHON_INSTALL_SITELIB}/fluxion\")
  execute_process( \
  COMMAND \
    ln -s ${LINKTARGET_FLUXION} \$ENV{DESTDIR}/${PYDIR}/fluxion \
  )
  execute_process( \
  COMMAND \
    rm -f \$ENV{DESTDIR}/${PYDIR}/_fluxion \
  )
  MESSAGE (STATUS \
    \"linking ${PYDIR}/_fluxion -> ${PYTHON_INSTALL_SITELIB}/_fluxion\")
  execute_process( \
  COMMAND \
    ln -s ${LINKTARGET_FLUXION_EXT} \$ENV{DESTDIR}/${PYDIR}/_fluxion \
  )")

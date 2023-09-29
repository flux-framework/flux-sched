
set(FLUXION_PY_FILES
  fluxion/resourcegraph/V1.py
  fluxion/resourcegraph/__init__.py
  fluxion/jsongraph/objects/edge.py
  fluxion/jsongraph/objects/graph.py
  fluxion/jsongraph/objects/__init__.py
  fluxion/jsongraph/objects/multigraph.py
  fluxion/jsongraph/objects/node.py
  )
install(DIRECTORY fluxion
  DESTINATION "${PYTHON_INSTALL_SITELIB}"
  FILES_MATCHING PATTERN "*.py")

set(PYNAME "python${Python_VERSION_MAJOR}.${Python_VERSION_MINOR}")
set(PYDIR "${CMAKE_INSTALL_FULL_LIBDIR}/flux/${PYNAME}")
set(LINKTARGET "${PYTHON_INSTALL_SITELIB}/fluxion")

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
    ln -s ${LINKTARGET} \$ENV{DESTDIR}/${PYDIR}/fluxion \
  )")

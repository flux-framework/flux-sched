
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


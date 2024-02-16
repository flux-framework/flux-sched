set(TARGET main)
set(SRCS main.go)
set(CGO_CFLAGS "-I${CMAKE_BINARY_DIR}")

# This is currently passed but not used because when passed into add_custom_command the spaces are escaped
set(CGO_LIBRARY_FLAGS -Wl,-R
    -lflux-idset
    -lstdc++
    -lczmq
    -ljansson
    -lhwloc
    -lboost_system
    -lflux-hostlist
    -lboost_graph
    -lyaml-cpp
    -L${CMAKE_BINARY_DIR}/resource/reapi/bindings -lreapi_cli
    )

# This ensures the main binary is cleaned up
set_directory_properties(
    PROPERTIES
    ADDITIONAL_CLEAN_FILES "${CMAKE_CURRENT_BINARY_DIR}/main"
  )

# We add the dependencies at the end so the build can run in parallel and we don't try to build this before they are ready!
BUILD_GO_PROGRAM(${TARGET} ${SRCS} "${CGO_CFLAGS}" "${CGO_LIBRARY_FLAGS}" reapi_cli)

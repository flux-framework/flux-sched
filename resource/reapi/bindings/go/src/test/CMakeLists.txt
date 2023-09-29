set(TARGET main)
set(SRCS main.go)
set(CGO_CFLAGS "-I${CMAKE_BINARY_DIR}/resource/reapi/bindings/c")

# This is currently passed but not used because when passed into add_custom_command the spaces are escaped
set(CGO_LIBRARY_FLAGS "-Wl,-R ${CMAKE_BINARY_DIR}/resource -L${CMAKE_BINARY_DIR}/resource/reapi/bindings -L${CMAKE_BINARY_DIR}/resource/libjobspec -ljobspec_conv -lreapi_cli -L${CMAKE_BINARY_DIR}/resource -lresource -lflux-idset -lstdc++ -lczmq -ljansson -lhwloc -lboost_system -lflux-hostlist -lboost_graph -lyaml-cpp")

# This ensures the main binary is cleaned up
set_directory_properties(
    PROPERTIES
      ADDITIONAL_CLEAN_FILES "${CMAKE_CURRENT_BINARY_DIR}/main"
  )

# GO_GET(go_redis github.com/hoisie/redis)
#                          main      main.go  
# We add the dependencies at the end so the build can run in parallel and we don't try to build this before they are ready!
BUILD_GO_PROGRAM(${TARGET} ${SRCS} "${CGO_CFLAGS}" "${CGO_LIBRARY_FLAGS}" jobspec_conv reapi_module reapi_cli resource flux::core)

set(CUSTOM_GO_PATH "${CMAKE_SOURCE_DIR}/resource/reapi/bindings/go")
set(GOPATH "${CMAKE_CURRENT_BINARY_DIR}/go")

# This probably isn't necessary, although if we could build fluxcli into it maybe
file(MAKE_DIRECTORY ${GOPATH})

# ADD_GO_INSTALLABLE_PROGRAM builds a custom go program (primarily for testing)
function(BUILD_GO_PROGRAM NAME MAIN_SRC CGO_CFLAGS CGO_LIBRARY_FLAGS)
  message(STATUS "GOPATH: ${GOPATH}")
  message(STATUS "CGO_LDFLAGS: ${CGO_LIBRARY_FLAGS}")
  get_filename_component(MAIN_SRC_ABS ${MAIN_SRC} ABSOLUTE)
  add_custom_target(${NAME})

  # IMPORTANT: the trick to getting *spaces* to render in COMMAND is to convert them to ";"
  # string(REPLACE <match-string> <replace-string> <out-var> <input>...)
  STRING(REPLACE " " ";" CGO_LIBRARY_FLAGS ${CGO_LIBRARY_FLAGS})  
  add_custom_command(TARGET ${NAME}
                    COMMAND GOPATH=${GOPATH}:${CUSTOM_GO_PATH} GOOS=linux G0111MODULE=off CGO_CFLAGS="${CGO_CFLAGS}" CGO_LDFLAGS='${CGO_LIBRARY_FLAGS}' go build -ldflags '-w'
                    -o "${CMAKE_CURRENT_SOURCE_DIR}/${NAME}"
                    ${CMAKE_GO_FLAGS} ${MAIN_SRC}
                    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
                    DEPENDS ${MAIN_SRC_ABS}
                    COMMENT "Building Go library")
  foreach(DEP ${ARGN})
    add_dependencies(${NAME} ${DEP})
  endforeach()

  add_custom_target(${NAME}_all ALL DEPENDS ${NAME})
  install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/${NAME} DESTINATION bin)
endfunction(BUILD_GO_PROGRAM)

add_library ( reapi_cli SHARED
    c++/reapi.hpp
    c++/reapi_cli.hpp
    c++/reapi_cli_impl.hpp
    c/reapi_cli.cpp
    c/reapi_cli.h
    )
target_link_libraries( reapi_cli PRIVATE
    resource
    jobspec_conv
    flux::core
    )
install(TARGETS reapi_cli LIBRARY DESTINATION "${FLUX_LIB_DIR}")

add_library (reapi_module STATIC
    c++/reapi.hpp
    c++/reapi_module.hpp
    c++/reapi_module_impl.hpp
    c/reapi_module.cpp
    c/reapi_module.h
    )
target_link_libraries( reapi_module PRIVATE
    flux::core
    )

if(DEFINED ENV{WITH_GO})
    message(STATUS "WITH_GO is set to build go bindings")
    add_subdirectory( go )
endif()

if(DEFINED ENV{WITH_PYTHON_BINDINGS})
  message(STATUS "WITH_PYTHON_BINDINGS detected to build python bindings")
  add_subdirectory( python )
endif()

add_library ( reapi_cli STATIC
    c++/reapi.hpp
    c++/reapi_cli.hpp
    c++/reapi_cli_impl.hpp
    c/reapi_cli.cpp
    c/reapi_cli.h
    )
target_link_libraries(reapi_cli PRIVATE
    flux::core
    )
add_library ( reapi_module STATIC
    c++/reapi.hpp
    c++/reapi_module.hpp
    c++/reapi_module_impl.hpp
    c/reapi_module.cpp
    c/reapi_module.h
    )
target_link_libraries(reapi_module PRIVATE
    flux::core
    )

flux_add_module ( sched-fluxion-resource
    ${CMAKE_CURRENT_SOURCE_DIR}/resource_match.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resource_match_opts.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resource_match_opts.hpp
    )
target_link_libraries (sched-fluxion-resource PRIVATE
    resource
    PkgConfig::JANSSON
    PkgConfig::UUID
    )

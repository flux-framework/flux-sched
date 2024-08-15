flux_add_plugin ( sched-fluxion-resource MODULE
    ${CMAKE_CURRENT_SOURCE_DIR}/resource_match.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resource_match.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resource_match_opts.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resource_match_opts.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resource.cpp
    )
flux_add_plugin ( sched-fluxion-feasibility MODULE
    ${CMAKE_CURRENT_SOURCE_DIR}/resource_match.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resource_match.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resource_match_opts.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resource_match_opts.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/feasibility.cpp
    )
target_link_libraries (sched-fluxion-resource PRIVATE
    resource
    PkgConfig::JANSSON
    PkgConfig::UUID
    intern
    cppwrappers
    )
target_link_libraries (sched-fluxion-feasibility PRIVATE
    resource
    PkgConfig::JANSSON
    PkgConfig::UUID
    intern
    cppwrappers
    )

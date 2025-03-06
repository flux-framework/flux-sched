add_library(sched-fluxion-resource-module SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/resource_match.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resource_match.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resource_match_opts.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resource_match_opts.hpp
    )
target_link_libraries (sched-fluxion-resource-module PUBLIC
    resource
    PkgConfig::JANSSON
    PkgConfig::UUID
    cppwrappers
    )
install(TARGETS sched-fluxion-resource-module LIBRARY)

flux_add_plugin ( sched-fluxion-resource MODULE
    ${CMAKE_CURRENT_SOURCE_DIR}/resource.cpp
    )
flux_add_plugin ( sched-fluxion-feasibility MODULE
    ${CMAKE_CURRENT_SOURCE_DIR}/feasibility.cpp
    )

target_link_libraries (sched-fluxion-resource PRIVATE
	sched-fluxion-resource-module
    )
target_link_libraries (sched-fluxion-feasibility PRIVATE
	sched-fluxion-resource-module
    )

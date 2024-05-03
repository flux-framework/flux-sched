flux_add_plugin ( sched-fluxion-resource MODULE
        resource_match.cpp
        resource_match_opts.cpp
        resource_match_opts.hpp
    )
target_link_libraries (sched-fluxion-resource PRIVATE
    resource
    PkgConfig::JANSSON
    PkgConfig::UUID
    )

flux_add_module ( sched-fluxion-qmanager
    qmanager.cpp
    qmanager_callbacks.cpp
    qmanager_callbacks.hpp
    qmanager_opts.cpp
    qmanager_opts.hpp
    )
target_link_libraries(sched-fluxion-qmanager PRIVATE
    jobspec_conv
    flux::core
    flux::schedutil
    PkgConfig::JANSSON
    )

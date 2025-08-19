add_library(fluxion-data SHARED schema/data_std.cpp)
target_link_libraries(fluxion-data PUBLIC
    intern
    )
set_target_properties(fluxion-data PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    )
install(TARGETS fluxion-data LIBRARY NAMELINK_SKIP)
add_subdirectory(libjobspec)
add_subdirectory(planner)
set(RESOURCE_HEADERS 
    utilities/command.hpp
    policies/dfu_match_high_id_first.hpp
    policies/dfu_match_low_id_first.hpp
    policies/dfu_match_multilevel_id.hpp
    policies/dfu_match_multilevel_id_impl.hpp
    policies/dfu_match_locality.hpp
    policies/dfu_match_var_aware.hpp
    policies/dfu_match_policy_factory.hpp
    jobinfo/jobinfo.hpp
    schema/resource_graph.hpp
    schema/infra_data.hpp
    schema/sched_data.hpp
    schema/resource_base.hpp
    schema/resource_data.hpp
    schema/perf_data.hpp
    schema/color.hpp
    schema/ephemeral.hpp
    traversers/dfu.hpp
    traversers/dfu_impl.hpp
    policies/base/dfu_match_cb.hpp
    policies/base/matcher.hpp
    readers/resource_namespace_remapper.hpp
    readers/resource_reader_base.hpp
    readers/resource_spec_grug.hpp
    readers/resource_reader_grug.hpp
    readers/resource_reader_hwloc.hpp
    readers/resource_reader_jgf.hpp
    readers/resource_reader_rv1exec.hpp
    readers/resource_reader_factory.hpp
    evaluators/scoring_api.hpp
    evaluators/edge_eval_api.hpp
    evaluators/fold.hpp
    evaluators/expr_eval_api.hpp
    evaluators/expr_eval_target.hpp
    evaluators/expr_eval_vtx_target.hpp
    config/system_defaults.hpp
    store/resource_graph_store.hpp
)

add_library(resource STATIC
    policies/dfu_match_locality.cpp
    policies/dfu_match_var_aware.cpp
    policies/dfu_match_policy_factory.cpp
    jobinfo/jobinfo.cpp
    schema/resource_data.cpp
    schema/perf_data.cpp
    schema/infra_data.cpp
    schema/sched_data.cpp
    schema/color.cpp
    schema/ephemeral.cpp
    traversers/dfu.cpp
    traversers/dfu_impl.cpp
    traversers/dfu_impl_update.cpp
    policies/base/dfu_match_cb.cpp
    policies/base/matcher.cpp
    readers/resource_namespace_remapper.cpp
    readers/resource_reader_base.cpp
    readers/resource_spec_grug.cpp
    readers/resource_reader_grug.cpp
    readers/resource_reader_hwloc.cpp
    readers/resource_reader_jgf.cpp
    readers/resource_reader_rv1exec.cpp
    readers/resource_reader_factory.cpp
    writers/match_writers.cpp
    evaluators/scoring_api.cpp
    evaluators/edge_eval_api.cpp
    evaluators/expr_eval_api.cpp
    evaluators/expr_eval_vtx_target.cpp
    store/resource_graph_store.cpp
    ${RESOURCE_HEADERS}
    )
target_include_directories(resource PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(resource PRIVATE
    planner
    )
target_link_libraries(resource PUBLIC
    flux::hostlist
    flux::idset
    PkgConfig::JANSSON
    PkgConfig::HWLOC
    PkgConfig::UUID
    Boost::graph
    jobspec_conv
    )
add_sanitizers(resource)

add_subdirectory(modules)
add_subdirectory(reapi)
add_subdirectory(evaluators)
add_subdirectory(policies)
add_subdirectory(schema)

# programs that depend on libresource
add_subdirectory(utilities)

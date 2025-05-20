set(MODULE_SRCFILES
    ${CMAKE_CURRENT_SOURCE_DIR}/resource_match.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resource_match_opts.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resource_match_opts.hpp
)

foreach(SRCFILE ${MODULE_SRCFILES})
   set_source_files_properties(SRCFILE COMPILE_FLAGS "-Xclang -load -Xclang ${PERFFLOWASPECT_LIB_DIR}/libWeavePass.so")
endforeach()

flux_add_plugin ( sched-fluxion-resource MODULE
	${MODULE_SRCFILES})
target_link_libraries (sched-fluxion-resource PRIVATE
    resource
    PkgConfig::JANSSON
    PkgConfig::UUID
    cppwrappers
    )

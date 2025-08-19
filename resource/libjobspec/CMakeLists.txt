add_library ( jobspec_conv STATIC
    constraint.cpp
    constraint.hpp
    hostlist_constraint.cpp
    hostlist_constraint.hpp
    jobspec.cpp
    jobspec.hpp
    parse_error.hpp
    rank_constraint.cpp
    rank_constraint.hpp
    )
add_sanitizers(jobspec_conv)
target_link_libraries(jobspec_conv PUBLIC
    flux::hostlist
    flux::idset
    flux::core
    PkgConfig::JANSSON
    PkgConfig::yaml-cpp
    )
target_link_libraries(jobspec_conv PRIVATE
    fluxion-data
    PkgConfig::UUID
    intern
    )

add_executable(flux-jobspec-validate
    ${CMAKE_CURRENT_SOURCE_DIR}/flux-jobspec-validate.cpp
    )
add_sanitizers(flux-jobspec-validate)

target_link_libraries(flux-jobspec-validate PRIVATE
    jobspec_conv
    intern
    )

add_executable(test_constraint.t test/constraint.cpp)
add_sanitizers(test_constraint.t)
target_link_libraries(test_constraint.t jobspec_conv libtap intern)
flux_add_test(NAME test_constraint COMMAND test_constraint.t)

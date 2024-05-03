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
target_link_libraries(jobspec_conv PUBLIC flux::hostlist flux::idset
    flux::core yaml-cpp PkgConfig::JANSSON 
    PkgConfig::UUID
    )

add_executable(flux-jobspec-validate
        flux-jobspec-validate.cpp
    )

target_link_libraries(flux-jobspec-validate PRIVATE jobspec_conv)

add_executable(test_constraint.t test/constraint.cpp)
target_link_libraries(test_constraint.t jobspec_conv libtap)
flux_add_test(NAME test_constraint COMMAND test_constraint.t)

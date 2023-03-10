add_library ( jobspec_conv STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/constraint.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/constraint.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hostlist_constraint.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hostlist_constraint.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/jobspec.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/jobspec.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/parse_error.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rank_constraint.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rank_constraint.hpp
    )
target_link_libraries(jobspec_conv PUBLIC flux::hostlist flux::idset
    flux::core yaml-cpp)

add_executable(flux-jobspec-validate
    ${CMAKE_CURRENT_SOURCE_DIR}/flux-jobspec-validate.cpp
    )

target_link_libraries(flux-jobspec-validate PRIVATE jobspec_conv)

add_executable(test_constraint.t test/constraint.cpp)
target_link_libraries(test_constraint.t jobspec_conv libtap)
add_test(NAME test_constraint COMMAND test_constraint.t)

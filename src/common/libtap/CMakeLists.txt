# Generating the library libtap.la
list(APPEND tap_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/tap.c
    ${CMAKE_CURRENT_SOURCE_DIR}/tap.h
)
add_library (libtap SHARED ${tap_SOURCES} )
target_include_directories(libtap PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../)

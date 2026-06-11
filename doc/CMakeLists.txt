set(3VALUE_OPTS On Off Try)
set(ENABLE_DOCS "Try" CACHE STRING "Enables documentation")

if(ENABLE_DOCS AND NOT ENABLE_DOCS STREQUAL "Try") # only matches On
    set(REQ_DOCS REQUIRED)
endif()
find_program(SPHINX NAMES sphinx-build sphinx-build2 ${REQ_DOCS})
find_package(Doxygen)

# Check if breathe Python module is available for API docs
if(DOXYGEN_FOUND AND Python_EXECUTABLE)
    execute_process(
        COMMAND ${Python_EXECUTABLE} -c "import breathe"
        RESULT_VARIABLE BREATHE_IMPORT_RESULT
        OUTPUT_QUIET ERROR_QUIET
    )
    if(BREATHE_IMPORT_RESULT EQUAL 0)
        set(BREATHE_FOUND TRUE)
        message(STATUS "Found breathe Python module")
    else()
        set(BREATHE_FOUND FALSE)
        message(STATUS "breathe Python module not found - API documentation will be disabled")
    endif()
endif()

if(DOXYGEN_FOUND AND BREATHE_FOUND AND SPHINX AND ENABLE_DOCS)
    # Configure Doxyfile with current project settings
    set(DOXYGEN_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/doxygen)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in
                   ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)

    # Add target to generate Doxygen XML output
    add_custom_command(
        OUTPUT ${DOXYGEN_OUTPUT_DIR}/xml/index.xml ${DOXYGEN_OUTPUT_DIR}/doxygen-warnings.log
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        COMMENT "Generating Doxygen XML documentation (warnings in ${DOXYGEN_OUTPUT_DIR}/doxygen-warnings.log)"
        VERBATIM
    )
    add_custom_target(doxygen DEPENDS ${DOXYGEN_OUTPUT_DIR}/xml/index.xml)
endif()

if(SPHINX AND ENABLE_DOCS)

    set(man5_inputs
        man5/flux-config-sched-fluxion-qmanager.rst
        man5/flux-config-sched-fluxion-resource.rst
        )
    foreach(file ${man5_inputs})
        string(REPLACE ".rst" ".5" out ${file})
        list(APPEND man5_outputs ${CMAKE_CURRENT_BINARY_DIR}/${out})
        string(REPLACE ".rst" ".html" out ${file})
        list(APPEND man5_html_outputs ${CMAKE_CURRENT_BINARY_DIR}/html/${out})
    endforeach()

    set(sphinx_deps man5/index.rst ${man5_inputs})
    if(DOXYGEN_FOUND)
        list(APPEND sphinx_deps ${DOXYGEN_OUTPUT_DIR}/xml/index.xml)
    endif()

    add_custom_command(
        OUTPUT ${man5_outputs}
        DEPENDS ${sphinx_deps}
        COMMAND
        PYTHONPATH=$ENV{PYTHONPATH}:${CMAKE_CURRENT_SOURCE_DIR}
        SPHINX_BUILDDIR=${CMAKE_CURRENT_BINARY_DIR}
        ${Python_EXECUTABLE} -m sphinx -q -b man ${CMAKE_CURRENT_SOURCE_DIR} ./man5
        )
    add_custom_target(manpages DEPENDS ${man5_outputs})
    if(DOXYGEN_FOUND)
        add_dependencies(manpages doxygen)
    endif()
    install(
        DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/man5
        TYPE MAN
        FILES_MATCHING PATTERN "*.5" PATTERN ".doctrees" EXCLUDE
        )
    add_custom_command(
        OUTPUT ${man5_html_outputs}
        DEPENDS ${sphinx_deps}
        COMMAND
        PYTHONPATH=$ENV{PYTHONPATH}:${CMAKE_CURRENT_SOURCE_DIR}
        SPHINX_BUILDDIR=${CMAKE_CURRENT_BINARY_DIR}
        ${Python_EXECUTABLE} -m sphinx -q -b html ${CMAKE_CURRENT_SOURCE_DIR} ./html
        )
    add_custom_target(html DEPENDS ${man5_html_outputs})
    if(DOXYGEN_FOUND)
        add_dependencies(html doxygen)
    endif()
    add_custom_target(docs
        DEPENDS manpages)
endif()

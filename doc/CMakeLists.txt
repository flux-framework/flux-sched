set(3VALUE_OPTS On Off Try)
set(ENABLE_DOCS "Try" CACHE STRING "Enables documentation")

if(ENABLE_DOCS AND NOT ENABLE_DOCS STREQUAL "Try") # only matches On
    set(REQ_DOCS REQUIRED)
endif()
find_program(SPHINX NAMES sphinx-build sphinx-build2 ${REQ_DOCS})

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

    add_custom_command(
        OUTPUT ${man5_outputs}
        ALL
        DEPENDS
        man5/index.rst
        ${man5_inputs}
        COMMAND
        PYTHONPATH=$ENV{PYTHONPATH}:${CMAKE_CURRENT_SOURCE_DIR}
        SPHINX_BUILDDIR=${CMAKE_CURRENT_BINARY_DIR}
        ${Python_EXECUTABLE} -m sphinx -b man ${CMAKE_CURRENT_SOURCE_DIR} ./man5
        )
    add_custom_target(manpages DEPENDS ${man5_outputs})
    install(
        DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/man5
        TYPE MAN
        FILES_MATCHING PATTERN "*.5" PATTERN ".doctrees" EXCLUDE
        )
    add_custom_command(
        OUTPUT ${man5_html_outputs}
        DEPENDS
        man5/index.rst
        ${man5_inputs}
        COMMAND
        PYTHONPATH=$ENV{PYTHONPATH}:${CMAKE_CURRENT_SOURCE_DIR}
        SPHINX_BUILDDIR=${CMAKE_CURRENT_BINARY_DIR}
        ${Python_EXECUTABLE} -m sphinx -b html ${CMAKE_CURRENT_SOURCE_DIR} ./html
        )
    add_custom_target(html DEPENDS ${man5_html_outputs})
    add_custom_target(docs ALL
        DEPENDS manpages)
endif()

if(DEFINED FLUX_CORE_PREFIX)
    set(ENV{PKG_CONFIG_PATH} "${FLUX_CORE_PREFIX}/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
else()
    find_program(FLUX flux ENV PATH)
    if(FLUX)
        get_filename_component(EXTRA_FLUX_CORE_PREFIX_BIN ${FLUX} DIRECTORY)
        get_filename_component(EXTRA_FLUX_CORE_PREFIX ${EXTRA_FLUX_CORE_PREFIX_BIN} DIRECTORY)
        set(ENV{PKG_CONFIG_PATH} "${EXTRA_FLUX_CORE_PREFIX}/lib64/pkgconfig:${EXTRA_FLUX_CORE_PREFIX}/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
	message(STATUS "found ${FLUX} in PATH with prefix=${EXTRA_FLUX_CORE_PREFIX}")
    endif()
endif()

pkg_check_modules(FLUX_CORE REQUIRED IMPORTED_TARGET flux-core>=0.77)
set(FLUX_PREFIX ${FLUX_CORE_PREFIX})
set(LIBFLUX_VERSION ${FLUX_CORE_VERSION})

find_program(FLUX flux
    PATHS ${FLUX_PREFIX}/bin ENV PATH)

execute_process(COMMAND $FLUX python -c "import sys; print(\".\".join(map(str, sys.version_info[[:2]])))"
    OUTPUT_VARIABLE FLUX_PYTHON_VERSION)

pkg_check_modules(FLUX_HOSTLIST REQUIRED IMPORTED_TARGET flux-hostlist )
pkg_check_modules(FLUX_IDSET REQUIRED IMPORTED_TARGET flux-idset )
pkg_check_modules(FLUX_OPTPARSE REQUIRED IMPORTED_TARGET flux-optparse )
pkg_check_modules(FLUX_SCHEDUTIL REQUIRED IMPORTED_TARGET flux-schedutil )
pkg_check_modules(FLUX_TASKMAP REQUIRED IMPORTED_TARGET flux-taskmap )

add_library(flux::core ALIAS PkgConfig::FLUX_CORE)
add_library(flux::hostlist ALIAS PkgConfig::FLUX_HOSTLIST)
add_library(flux::idset ALIAS PkgConfig::FLUX_IDSET)
add_library(flux::optparse ALIAS PkgConfig::FLUX_OPTPARSE)
add_library(flux::schedutil ALIAS PkgConfig::FLUX_SCHEDUTIL)
add_library(flux::taskmap ALIAS PkgConfig::FLUX_TASKMAP)

# all but PMI
add_library(flux-all INTERFACE)
target_link_libraries(flux-all INTERFACE flux::core flux::hostlist flux::idset flux::optparse flux::schedutil flux::taskmap)
add_library(flux::all ALIAS flux-all)


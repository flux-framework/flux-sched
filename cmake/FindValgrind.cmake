# Look for Valgrind headers and binary.
#
# Variables defined by this module:
# 	Valgrind_FOUND System has valgrind
# 	Valgrind_INCLUDE_DIR where to find valgrind/memcheck.h, etc.
# 	Valgrind_EXECUTABLE the valgrind executable.
# This module appends to config.h so t5000-valgrind.t succeeds.
# We may need to change this behavior once remaining autotools 
# files are removed.

find_path(Valgrind_INCLUDE_DIR valgrind HINTS ${Valgrind_INCLUDE_PATH})
find_program(Valgrind_EXECUTABLE NAMES valgrind PATH ${Valgrind_BINARY_PATH})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Valgrind DEFAULT_MSG Valgrind_INCLUDE_DIR Valgrind_EXECUTABLE)

if(Valgrind_FOUND)
	file(APPEND config.h "#define HAVE_VALGRIND 1\n")
endif()

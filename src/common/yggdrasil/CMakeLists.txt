if (${CMAKE_VERSION} VERSION_GREATER_EQUAL 3.19.0)
set(YGG_SOURCES 
    util.hpp
    tree_iterator.hpp
    tree_iterator.cpp
    size_holder.hpp
    rbtree.hpp
    rbtree.cpp
    options.hpp
    debug.hpp
    bst.hpp
    bst.cpp
  )
else()
set(YGG_SOURCES )
endif()
# NOTE: despite the .cpp files this is a header-only library
add_library( yggdrasil INTERFACE ${YGG_SOURCES})
target_include_directories(yggdrasil INTERFACE .)

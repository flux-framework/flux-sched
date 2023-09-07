# NOTE: despite the .cpp files this is a header-only library
add_library(
  yggdrasil
  INTERFACE
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

//
// Created by Lukas Barth on 09.08.17.
//

#ifndef YGG_OPTIONS_HPP
#define YGG_OPTIONS_HPP

#include "util.hpp"

namespace ygg {

// DHA: Heavily modifiy TreeOptions for Planner's use: 
// 1) A bit better self-containment as the original "Options"
//    includes options for *all* of the supported data structures
//    beyond the augmented rbtree;
// 2) A bit better readability; and
// 3) Mostly importantly so that we can more easily compile
//    RBtree at the C++11 level. Option template parameter
//    packing and etc use some of the more modern features that
//    are only available C++14 or 17, which are pretty difficult
//    to backport to C++11. 

class TreeOptions {
public:
	static constexpr bool multiple = true;
	static constexpr bool order_queries = false;
	static constexpr bool constant_time_size = true;
	static constexpr bool compress_color = false;
	static constexpr bool stl_erase = false;
private:
	TreeOptions(); // Instantiation not allowed
};

using DefaultOptions = TreeOptions;

} // namespace ygg

#endif // YGG_OPTIONS_HPP

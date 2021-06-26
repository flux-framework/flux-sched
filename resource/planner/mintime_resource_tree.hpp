/*****************************************************************************\
 *  Copyright (c) 2021 Lawrence Livermore National Security, LLC.  Produced at
 *  the Lawrence Livermore National Laboratory (cf, AUTHORS, DISCLAIMER.LLNS).
 *  LLNL-CODE-658032 All rights reserved.
 *
 *  This file is part of the Flux resource manager framework.
 *  For details, see https://github.com/flux-framework.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the license, or (at your option)
 *  any later version.
 *
 *  Flux is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the terms and conditions of the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *  See also:  http://www.gnu.org/licenses/
\*****************************************************************************/

#ifndef MINTIME_RESOURCE_TREE_HPP
#define MINTIME_RESOURCE_TREE_HPP

#include <cstdint>
#include "src/common/yggdrasil/rbtree.hpp"

struct scheduled_point_t;
struct rb_node_base_t;

struct mt_resource_rb_node_t
        : public rb_node_base_t,
          public ygg::RBTreeNodeBase<mt_resource_rb_node_t> {
    int64_t at;
    int64_t subtree_min;
    int64_t remaining;
    bool operator< (const mt_resource_rb_node_t &other) const;
};

template <class mt_resource_rb_node_t, class NodeTraits>
class mt_resource_node_traits : public NodeTraits {
public:
    template <class BaseTree>
    static void leaf_inserted (mt_resource_rb_node_t &node, BaseTree &t);

    template <class BaseTree>
    static void rotated_left (mt_resource_rb_node_t &node, BaseTree &t);

    template <class BaseTree>
    static void rotated_right (mt_resource_rb_node_t &node, BaseTree &t);

    template <class BaseTree>
    static void deleted_below (mt_resource_rb_node_t &node, BaseTree &t);

    template <class BaseTree>
    static void swapped (mt_resource_rb_node_t &n1,
                         mt_resource_rb_node_t &n2, BaseTree &t);

private:
    static void fix (mt_resource_rb_node_t *node);
};

using mt_resource_rb_tree_t = ygg::RBTree<mt_resource_rb_node_t,
                                          mt_resource_node_traits<
                                              mt_resource_rb_node_t,
                                              ygg::RBDefaultNodeTraits>>;


class mintime_resource_tree_t {
public:
    int insert (scheduled_point_t *point);
    int remove (scheduled_point_t *point);
    scheduled_point_t *get_mintime (int64_t request);

private:
    int64_t right_branch_mintime (mt_resource_rb_node_t *n);
    scheduled_point_t *find_mintime_point (mt_resource_rb_node_t *anchor,
                                           int64_t min_time);
    int64_t find_mintime_anchor (int64_t request,
                                 mt_resource_rb_node_t **anchor_p);
    mt_resource_rb_tree_t m_tree;
};

#endif // MINTIME_RESOURCE_TREE_HPP

/*
 * vi: ts=4 sw=4 expandtab
 */

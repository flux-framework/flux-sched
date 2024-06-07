/*****************************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef MINTIME_RESOURCE_TREE_HPP
#define MINTIME_RESOURCE_TREE_HPP

#include <cstdint>
#include "src/common/yggdrasil/rbtree.hpp"

struct scheduled_point_t;
class rb_node_base_t;

struct mt_resource_rb_node_t : public rb_node_base_t,
                               public ygg::RBTreeNodeBase<mt_resource_rb_node_t> {
    int64_t at;
    int64_t subtree_min;
    int64_t remaining;
    bool operator< (const mt_resource_rb_node_t &other) const;
    bool operator== (const mt_resource_rb_node_t &other) const;
    bool operator!= (const mt_resource_rb_node_t &other) const;
};

template<class mt_resource_rb_node_t, class NodeTraits>
class mt_resource_node_traits : public NodeTraits {
   public:
    template<class BaseTree>
    static void leaf_inserted (mt_resource_rb_node_t &node, BaseTree &t);

    template<class BaseTree>
    static void rotated_left (mt_resource_rb_node_t &node, BaseTree &t);

    template<class BaseTree>
    static void rotated_right (mt_resource_rb_node_t &node, BaseTree &t);

    template<class BaseTree>
    static void deleted_below (mt_resource_rb_node_t &node, BaseTree &t);

    template<class BaseTree>
    static void swapped (mt_resource_rb_node_t &n1, mt_resource_rb_node_t &n2, BaseTree &t);

   private:
    static void fix (mt_resource_rb_node_t *node);
};

using mt_resource_rb_tree_t =
    ygg::RBTree<mt_resource_rb_node_t,
                mt_resource_node_traits<mt_resource_rb_node_t, ygg::RBDefaultNodeTraits>>;

class mintime_resource_tree_t {
   public:
    int insert (scheduled_point_t *point);
    int remove (scheduled_point_t *point);
    scheduled_point_t *get_mintime (int64_t request) const;
    void clear ();

   private:
    int64_t right_branch_mintime (mt_resource_rb_node_t *n) const;
    scheduled_point_t *find_mintime_point (mt_resource_rb_node_t *anchor, int64_t min_time) const;
    int64_t find_mintime_anchor (int64_t request, mt_resource_rb_node_t **anchor_p) const;

    mt_resource_rb_tree_t m_tree;
};

#endif  // MINTIME_RESOURCE_TREE_HPP

/*
 * vi: ts=4 sw=4 expandtab
 */

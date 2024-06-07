/*****************************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef SCHEDULED_POINT_TREE_HPP
#define SCHEDULED_POINT_TREE_HPP

#include <cstdint>

#if defined(__clang__) || (__GNUC__ <= 11)
#include "src/common/yggdrasil/rbtree.hpp"
#elif (__GNUC__ > 11)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-template-keyword"
#include "src/common/yggdrasil/rbtree.hpp"
#pragma GCC diagnostic pop
#endif

struct scheduled_point_t;

class rb_node_base_t {
   public:
    void set_point (scheduled_point_t *p)
    {
        m_point = p;
    }
    scheduled_point_t *get_point ()
    {
        return m_point;
    }
    scheduled_point_t *get_point () const
    {
        return m_point;
    }

   private:
    scheduled_point_t *m_point = nullptr;
};

struct scheduled_point_rb_node_t : public rb_node_base_t,
                                   public ygg::RBTreeNodeBase<scheduled_point_rb_node_t> {
    bool operator< (const scheduled_point_rb_node_t &other) const;
    bool operator== (const scheduled_point_rb_node_t &other) const;
    bool operator!= (const scheduled_point_rb_node_t &other) const;
};

using scheduled_point_rb_tree_t = ygg::RBTree<scheduled_point_rb_node_t, ygg::RBDefaultNodeTraits>;

class scheduled_point_tree_t {
   public:
    ~scheduled_point_tree_t ();
    scheduled_point_t *next (scheduled_point_t *point) const;
    scheduled_point_t *next (scheduled_point_t *point);
    scheduled_point_t *search (int64_t tm);
    scheduled_point_t *get_state (int64_t at) const;
    int insert (scheduled_point_t *point);
    int remove (scheduled_point_t *point);
    void destroy ();
    bool empty () const;
    size_t get_size () const;

   private:
    scheduled_point_t *get_recent_state (scheduled_point_t *new_point,
                                         scheduled_point_t *old_point) const;
    void destroy (scheduled_point_rb_node_t *node);
    scheduled_point_rb_tree_t m_tree;
};

#endif  // SCHEDULED_POINT_TREE_HPP

/*
 * vi: ts=4 sw=4 expandtab
 */

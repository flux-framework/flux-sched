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

#ifndef SCHEDULED_POINT_TREE_HPP
#define SCHEDULED_POINT_TREE_HPP

#include <cstdint>
#include "src/common/yggdrasil/rbtree.hpp"

struct scheduled_point_t;

class rb_node_base_t {
public:
    void set_point (scheduled_point_t *p) {
        m_point = p;
    }
    scheduled_point_t *get_point () {
        return m_point;
    }
    const scheduled_point_t *get_point () const {
        return m_point;
    }
private:
    scheduled_point_t *m_point = nullptr;
};

struct scheduled_point_rb_node_t
           : public rb_node_base_t,
             public ygg::RBTreeNodeBase<scheduled_point_rb_node_t> {
    bool operator< (const scheduled_point_rb_node_t &other) const;
};

using scheduled_point_rb_tree_t = ygg::RBTree<scheduled_point_rb_node_t,
                                              ygg::RBDefaultNodeTraits>;

class scheduled_point_tree_t {
public:
    ~scheduled_point_tree_t ();
    scheduled_point_t *next (scheduled_point_t *point);
    scheduled_point_t *search (int64_t tm);
    scheduled_point_t *get_state (int64_t at);
    int insert (scheduled_point_t *point);
    int remove (scheduled_point_t *point);
    void destroy ();

private:
    scheduled_point_t *get_recent_state (scheduled_point_t *new_point,
                                         scheduled_point_t *old_point);
    void destroy (scheduled_point_rb_node_t *node);
    scheduled_point_rb_tree_t m_tree;
};

#endif // SCHEDULED_POINT_TREE_HPP

/*
 * vi: ts=4 sw=4 expandtab
 */

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

extern "C" {
#if HAVE_CONFIG_H
#include "config.h"
#endif
}

#include "planner_internal_tree.hpp"


/*******************************************************************************
 *                                                                             *
 *               Private Scheduled Point Search Tree Methods                   *
 *                                                                             *
 *******************************************************************************/

scheduled_point_t *scheduled_point_tree_t::get_recent_state (
                       scheduled_point_t *new_point,
                       scheduled_point_t *old_point)
{
    if (!old_point)
        return new_point;
    return (new_point->at > old_point->at)? new_point : old_point;
}

void scheduled_point_tree_t::destroy (rb_node *node)
{
    if (node->rb_left)
        destroy (node->rb_left);
    if (node->rb_right)
        destroy (node->rb_right);
    scheduled_point_t *data = container_of (node, scheduled_point_t, point_rb);
    delete data;
}


/*******************************************************************************
 *                                                                             *
 *                 Public Scheduled Point Search Tree Methods                  *
 *                                                                             *
 *******************************************************************************/

scheduled_point_tree_t::~scheduled_point_tree_t ()
{
    if (m_tree.rb_node) {
        destroy (m_tree.rb_node);
        m_tree.rb_node = nullptr;
    }
}

void scheduled_point_tree_t::destroy ()
{
    if (m_tree.rb_node) {
        destroy (m_tree.rb_node);
        m_tree.rb_node = nullptr;
    }
}

scheduled_point_t *scheduled_point_tree_t::next (scheduled_point_t *point)
{
    rb_node *n = rb_next (&(point->point_rb));
    return rb_entry (n, scheduled_point_t, point_rb);
}

scheduled_point_t *scheduled_point_tree_t::search (int64_t tm)
{
    rb_node *node = m_tree.rb_node;
    while (node) {
        scheduled_point_t *this_data = nullptr;
        this_data = container_of (node, scheduled_point_t, point_rb);
        int64_t result = tm - this_data->at;
        if (result < 0)
            node = node->rb_left;
        else if (result > 0)
            node = node->rb_right;
        else
            return this_data;
    }
    return nullptr;
}

/*! While scheduled_point_search returns the exact match scheduled_point_state
 *  returns the most recent scheduled point, representing the accurate resource
 *  state at the time t.
 */
scheduled_point_t *scheduled_point_tree_t::get_state (int64_t at)
{
    scheduled_point_t *last_state = nullptr;
    rb_node *node = m_tree.rb_node;
    while (node) {
        scheduled_point_t *this_data = nullptr;
        this_data = container_of (node, scheduled_point_t, point_rb);
        int64_t result = at - this_data->at;
        if (result < 0) {
            node = node->rb_left;
        } else if (result > 0) {
            last_state = get_recent_state (this_data, last_state);
            node = node->rb_right;
        } else {
            return this_data;
        }
    }
    return last_state;
}

int scheduled_point_tree_t::insert (scheduled_point_t *point)
{
    rb_node **link = &(m_tree.rb_node);
    rb_node *parent = nullptr;
    while (*link) {
        scheduled_point_t *this_data = nullptr;
        this_data  = container_of (*link, scheduled_point_t, point_rb);
        int64_t result = point->at - this_data->at;
        parent = *link;
        if (result < 0)
            link = &((*link)->rb_left);
        else if (result > 0)
            link = &((*link)->rb_right);
        else
            return -1;
    }
    rb_link_node (&(point->point_rb), parent, link);
    rb_insert_color (&(point->point_rb), &m_tree);
    return 0;
}

int scheduled_point_tree_t::remove (scheduled_point_t *point)
{
    int rc = -1;
    scheduled_point_t *n = search (point->at);
    if (n) {
        rb_erase (&(n->point_rb), &m_tree);
        // Note: this must only remove the node from the scheduled point tree:
        // DO NOT free memory allocated to the node
        rc = 0;
    }
    return rc;
}

/*
 * vi: ts=4 sw=4 expandtab
 */

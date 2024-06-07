/*****************************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

extern "C" {
#if HAVE_CONFIG_H
#include "config.h"
#endif
}

#include "planner_internal_tree.hpp"

////////////////////////////////////////////////////////////////////////////////
// Private Scheduled Point Search Tree Methods
////////////////////////////////////////////////////////////////////////////////

scheduled_point_t *scheduled_point_tree_t::get_recent_state (scheduled_point_t *new_point,
                                                             scheduled_point_t *old_point) const
{
    if (!old_point)
        return new_point;
    return (new_point->at > old_point->at) ? new_point : old_point;
}

void scheduled_point_tree_t::destroy (scheduled_point_rb_node_t *node)
{
    if (node->get_left ())
        destroy (node->get_left ());
    if (node->get_right ())
        destroy (node->get_right ());
    scheduled_point_t *data = node->get_point ();
    delete (data);
    data = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// Public Scheduled Point RBTree Node Methods
////////////////////////////////////////////////////////////////////////////////

bool scheduled_point_rb_node_t::operator< (const scheduled_point_rb_node_t &other) const
{
    return this->get_point ()->at < other.get_point ()->at;
}

bool operator< (const scheduled_point_rb_node_t &lhs, const int64_t rhs)
{
    return lhs.get_point ()->at < rhs;
}

bool operator< (const int64_t lhs, const scheduled_point_rb_node_t &rhs)
{
    return lhs < rhs.get_point ()->at;
}

bool scheduled_point_rb_node_t::operator== (const scheduled_point_rb_node_t &other) const
{
    return this->get_point ()->at == other.get_point ()->at;
}

bool scheduled_point_rb_node_t::operator!= (const scheduled_point_rb_node_t &other) const
{
    return !operator== (other);
}

////////////////////////////////////////////////////////////////////////////////
// Public Scheduled Point Search Tree Methods
////////////////////////////////////////////////////////////////////////////////

scheduled_point_tree_t::~scheduled_point_tree_t ()
{
    if (!m_tree.empty ()) {
        destroy (m_tree.get_root ());
        m_tree.clear ();
    }
}

void scheduled_point_tree_t::destroy ()
{
    if (!m_tree.empty ()) {
        destroy (m_tree.get_root ());
        m_tree.clear ();
    }
}

scheduled_point_t *scheduled_point_tree_t::next (scheduled_point_t *point)
{
    scheduled_point_t *next_point = nullptr;
    auto iter = m_tree.iterator_to (point->point_rb);
    if (iter != m_tree.end ()) {
        iter++;
        if (iter != m_tree.end ())
            next_point = iter->get_point ();
    }
    return next_point;
}

scheduled_point_t *scheduled_point_tree_t::next (scheduled_point_t *point) const
{
    scheduled_point_t *next_point = nullptr;
    auto iter = m_tree.iterator_to (point->point_rb);
    if (iter != m_tree.end ()) {
        iter++;
        if (iter != m_tree.end ())
            next_point = iter->get_point ();
    }
    return next_point;
}

scheduled_point_t *scheduled_point_tree_t::search (int64_t tm)
{
    auto iter = m_tree.find (tm);
    return (iter != m_tree.end ()) ? iter->get_point () : nullptr;
}

/*! While scheduled_point_search returns the exact match scheduled_point_state
 *  returns the most recent scheduled point, representing the accurate resource
 *  state at the time t.
 */
scheduled_point_t *scheduled_point_tree_t::get_state (int64_t at) const
{
    scheduled_point_t *last_state = nullptr;
    scheduled_point_rb_node_t *node = m_tree.get_root ();
    while (node) {
        scheduled_point_t *this_data = nullptr;
        this_data = node->get_point ();
        int64_t result = at - this_data->at;
        if (result < 0) {
            node = node->get_left ();
        } else if (result > 0) {
            last_state = get_recent_state (this_data, last_state);
            node = node->get_right ();
        } else {
            return this_data;
        }
    }
    return last_state;
}

int scheduled_point_tree_t::insert (scheduled_point_t *point)
{
    if (!point) {
        errno = EINVAL;
        return -1;
    }
    point->point_rb.set_point (point);
    m_tree.insert (point->point_rb);
    return 0;
}

int scheduled_point_tree_t::remove (scheduled_point_t *point)
{
    if (!point) {
        errno = EINVAL;
        return -1;
    }
    m_tree.remove (point->point_rb);
    return 0;
}

bool scheduled_point_tree_t::empty () const
{
    return m_tree.empty ();
}

size_t scheduled_point_tree_t::get_size () const
{
    return m_tree.size ();
}

/*
 * vi: ts=4 sw=4 expandtab
 */

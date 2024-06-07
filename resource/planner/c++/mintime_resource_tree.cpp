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

#include <limits>
#include <cerrno>

#include "planner_internal_tree.hpp"

////////////////////////////////////////////////////////////////////////////////
// Private Mintime Resource Tree Methods
////////////////////////////////////////////////////////////////////////////////

int64_t mintime_resource_tree_t::right_branch_mintime (mt_resource_rb_node_t *n) const
{
    int64_t min_time = std::numeric_limits<int64_t>::max ();
    mt_resource_rb_node_t *right = n->get_right ();
    if (right)
        min_time = right->subtree_min;
    return (n->at < min_time) ? n->at : min_time;
}

scheduled_point_t *mintime_resource_tree_t::find_mintime_point (mt_resource_rb_node_t *anchor,
                                                                int64_t min_time) const
{
    if (!anchor)
        return nullptr;

    if (anchor->at == min_time)
        return anchor->get_point ();

    mt_resource_rb_node_t *node = anchor->get_right ();
    while (node) {
        if (node->at == min_time)
            return node->get_point ();
        if (node->get_left () && (node->get_left ()->subtree_min == min_time))
            node = node->get_left ();
        else
            node = node->get_right ();
    }

    // Error condition: when an anchor was found, there must be
    // a point that meets the requirements.
    errno = ENOTSUP;
    return nullptr;
}

int64_t mintime_resource_tree_t::find_mintime_anchor (int64_t request,
                                                      mt_resource_rb_node_t **anchor_p) const
{
    mt_resource_rb_node_t *node = m_tree.get_root ();
    int64_t min_time = std::numeric_limits<int64_t>::max ();
    int64_t right_min_time = std::numeric_limits<int64_t>::max ();
    while (node) {
        if (request <= node->remaining) {
            // visiting node satisfies the resource requirements. This means all
            // nodes at its subtree also satisfy the requirements. Thus,
            // right_min_time is the best min time.
            right_min_time = right_branch_mintime (node);
            if (right_min_time < min_time) {
                min_time = right_min_time;
                *anchor_p = node;
            }
            // next, we should search the left subtree for potentially better
            // then current min_time;
            node = node->get_left ();
        } else {
            // visiting node does not satisfy the resource requirements. This
            // means that nothing in its left branch will meet these requirements:
            // time to search the right subtree.
            node = node->get_right ();
        }
    }
    return min_time;
}

////////////////////////////////////////////////////////////////////////////////
// Private Mintime Resource RBTree Node Methods
////////////////////////////////////////////////////////////////////////////////

template<class mt_resource_rb_node_t, class NodeTraits>
void mt_resource_node_traits<mt_resource_rb_node_t, NodeTraits>::fix (mt_resource_rb_node_t *node)
{
    mt_resource_rb_node_t *trav = node;
    while (trav) {
        int64_t old = trav->subtree_min;
        trav->subtree_min = trav->at;
        mt_resource_rb_node_t *left = trav->get_left ();
        if (left) {
            if (trav->subtree_min > left->subtree_min)
                trav->subtree_min = left->subtree_min;
        }
        mt_resource_rb_node_t *right = trav->get_right ();
        if (right) {
            if (trav->subtree_min > right->subtree_min)
                trav->subtree_min = right->subtree_min;
        }
        // no parent propagation is needed if new subtree_min == old,
        if (trav->subtree_min == old)
            break;

        mt_resource_rb_node_t *parent = trav->get_parent ();
        // if parent's subtree_min is already smaller than
        // new subtree_min, no parent propagation is necessary.
        // However, if parent's subtree_min == old, propagation is
        // needed, as it could (or in fact is likely to)
        // come from the traversing child node and thus needs an update.
        if (parent && parent->subtree_min != old && parent->subtree_min < trav->subtree_min)
            break;
        trav = parent;
    }
}

////////////////////////////////////////////////////////////////////////////////
// Public Mintime Resource RBTree Node Methods
////////////////////////////////////////////////////////////////////////////////

template<class mt_resource_rb_node_t, class NodeTraits>
template<class BaseTree>
void mt_resource_node_traits<mt_resource_rb_node_t, NodeTraits>::leaf_inserted (
    mt_resource_rb_node_t &node,
    BaseTree &tree)
{
    node.subtree_min = node.at;
    mt_resource_rb_node_t *n = &node;
    while ((n = n->get_parent ()) != nullptr && n->subtree_min > node.subtree_min)
        n->subtree_min = node.subtree_min;
}

template<class mt_resource_rb_node_t, class NodeTraits>
template<class BaseTree>
void mt_resource_node_traits<mt_resource_rb_node_t, NodeTraits>::rotated_left (
    mt_resource_rb_node_t &node,
    BaseTree &tree)
{
    fix (&node);
    if (node.get_parent ())
        fix (node.get_parent ());
}

template<class mt_resource_rb_node_t, class NodeTraits>
template<class BaseTree>
void mt_resource_node_traits<mt_resource_rb_node_t, NodeTraits>::rotated_right (
    mt_resource_rb_node_t &node,
    BaseTree &tree)
{
    fix (&node);
    if (node.get_parent ())
        fix (node.get_parent ());
}

template<class mt_resource_rb_node_t, class NodeTraits>
template<class BaseTree>
void mt_resource_node_traits<mt_resource_rb_node_t, NodeTraits>::deleted_below (
    mt_resource_rb_node_t &node,
    BaseTree &tree)
{
    fix (&node);
}

template<class mt_resource_rb_node_t, class NodeTraits>
template<class BaseTree>
void mt_resource_node_traits<mt_resource_rb_node_t, NodeTraits>::swapped (
    mt_resource_rb_node_t &node1,
    mt_resource_rb_node_t &node2,
    BaseTree &t)
{
    fix (&node1);
    if (node1.get_parent ())
        fix (node1.get_parent ());
    fix (&node2);
    if (node2.get_parent ())
        fix (node2.get_parent ());
}

bool operator< (const mt_resource_rb_node_t &lhs, const int64_t rhs)
{
    return lhs.remaining < rhs;
}

bool operator< (const int64_t lhs, const mt_resource_rb_node_t &rhs)
{
    return lhs < rhs.remaining;
}

bool mt_resource_rb_node_t::operator< (const mt_resource_rb_node_t &other) const
{
    return this->remaining < other.remaining;
}

bool mt_resource_rb_node_t::operator== (const mt_resource_rb_node_t &other) const
{
    return this->remaining == other.remaining;
}

bool mt_resource_rb_node_t::operator!= (const mt_resource_rb_node_t &other) const
{
    return !operator== (other);
}

////////////////////////////////////////////////////////////////////////////////
// Public Minimum Time Resource Tree API
////////////////////////////////////////////////////////////////////////////////

int mintime_resource_tree_t::insert (scheduled_point_t *point)
{
    if (!point) {
        errno = EINVAL;
        return -1;
    }
    point->resource_rb.set_point (point);
    point->resource_rb.at = point->at;
    point->resource_rb.remaining = point->remaining;
    m_tree.insert (point->resource_rb);
    point->in_mt_resource_tree = 1;
    return 0;
}

int mintime_resource_tree_t::remove (scheduled_point_t *point)
{
    if (!point) {
        errno = EINVAL;
        return -1;
    }
    m_tree.remove (point->resource_rb);
    point->in_mt_resource_tree = 0;
    return 0;
}

scheduled_point_t *mintime_resource_tree_t::get_mintime (int64_t request) const
{
    mt_resource_rb_node_t *anchor = nullptr;
    int64_t min_time = find_mintime_anchor (request, &anchor);
    return find_mintime_point (anchor, min_time);
}

void mintime_resource_tree_t::clear ()
{
    m_tree.clear ();
}

/*
 * vi: ts=4 sw=4 expandtab
 */

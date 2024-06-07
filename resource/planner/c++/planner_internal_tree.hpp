/*****************************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef PLANNER_INTERNAL_TREE_HPP
#define PLANNER_INTERNAL_TREE_HPP

#include "scheduled_point_tree.hpp"
#include "mintime_resource_tree.hpp"

/*! Scheduled point: time at which resource state changes.  Each point's resource
 *  requirements are tracked as a node in a min-time resource (MTR) binary search
 *  tree.
 */
struct scheduled_point_t {
    bool operator== (const scheduled_point_t &o) const;
    bool operator!= (const scheduled_point_t &o) const;

    scheduled_point_rb_node_t point_rb; /* BST node for scheduled point tree */
    mt_resource_rb_node_t resource_rb;  /* BST node for min-time resource tree */
    int64_t at;                         /* Resource-state changing time */
    int in_mt_resource_tree;            /* 1 when inserted in min-time resource tree */
    int new_point;                      /* 1 when this point is newly created */
    int ref_count;                      /* reference counter */
    int64_t scheduled;                  /* scheduled quantity at this point */
    int64_t remaining;                  /* remaining resources (available) */
};

#endif  // PLANNER_INTERNAL_TREE_HPP

/*
 * vi: ts=4 sw=4 expandtab
 */

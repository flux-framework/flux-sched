/*****************************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef SCHED_DATA_H
#define SCHED_DATA_H

#include <map>
#include <cstdint>
#include <stdexcept>
#include "resource/planner/c/planner.h"

namespace Flux {
namespace resource_model {

//! Type to keep track of current schedule state
struct schedule_t {
    schedule_t ();
    schedule_t (const schedule_t &o);
    schedule_t &operator= (const schedule_t &o);
    bool operator== (const schedule_t &o) const;
    ~schedule_t ();

    std::map<int64_t, int64_t> allocations;
    std::map<int64_t, int64_t> reservations;
    planner_t *plans = nullptr;
    //! Invariant: the sum of allocations.size () over this vertex and all
    //! of its descendants in the containment subsystem. The functions in
    //! resource_graph.hpp that change allocations keep it exact. A vertex
    //! that a shrink cut from the graph keeps the value it had, and no
    //! match examines it.
    int64_t subtree_allocations = 0;
};

}  // namespace resource_model
}  // namespace Flux

#endif  // SCHED_DATA_H

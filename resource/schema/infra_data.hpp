/*****************************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef INFRA_DATA_HPP
#define INFRA_DATA_HPP

#include <map>
#include <cstdint>
#include <limits>
#include "resource/schema/data_std.hpp"
#include "resource/schema/ephemeral.hpp"
#include "resource/planner/c/planner_multi.h"

namespace Flux {
namespace resource_model {

/*! Base type to organize the data supporting scheduling infrastructure's
 * operations (e.g., graph organization, coloring and edge evaluation).
 */
struct infra_base_t {
    infra_base_t ();
    infra_base_t (const infra_base_t &o);
    infra_base_t &operator= (const infra_base_t &o);
    virtual ~infra_base_t ();
    virtual void scrub () = 0;

    intern::interned_key_vec<subsystem_t, bool> member_of;
};

struct pool_infra_t : public infra_base_t {
    pool_infra_t ();
    pool_infra_t (const pool_infra_t &o);
    pool_infra_t &operator= (const pool_infra_t &o);
    bool operator== (const pool_infra_t &o) const;
    virtual ~pool_infra_t ();
    virtual void scrub ();

    std::map<int64_t, int64_t> tags;
    std::map<int64_t, int64_t> x_spans;
    std::map<int64_t, int64_t> job2span;
    planner_t *x_checker = NULL;
    subsystem_key_vec<planner_multi_t *> subplans;
    subsystem_key_vec<uint64_t> colors;
    ephemeral_t ephemeral;
};

class relation_infra_t : public infra_base_t {
   public:
    relation_infra_t ();
    relation_infra_t (const relation_infra_t &o);
    relation_infra_t &operator= (const relation_infra_t &o);
    virtual ~relation_infra_t ();
    virtual void scrub ();

    void set_for_trav_update (uint64_t needs, int exclusive, uint64_t trav_token);

    uint64_t get_needs () const;
    int get_exclusive () const;
    uint64_t get_trav_token () const;
    uint64_t get_weight () const;
    void set_weight (uint64_t);

   private:
    uint64_t m_needs = 0;
    uint64_t m_trav_token = 0;
    uint64_t m_weight = std::numeric_limits<uint64_t>::max ();
    int m_exclusive = 0;
};

}  // namespace resource_model
}  // namespace Flux

#endif  // INFRA_DATA_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

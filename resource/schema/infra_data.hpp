/*****************************************************************************\
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

#ifndef INFRA_DATA_HPP
#define INFRA_DATA_HPP

#include <map>
#include <cstdint>
#include "schema/data_std.hpp"
#include "planner/planner_multi.h"

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

    multi_subsystems_t member_of;
};

struct pool_infra_t : public infra_base_t {
    pool_infra_t ();
    pool_infra_t (const pool_infra_t &o);
    pool_infra_t &operator= (const pool_infra_t &o);
    virtual ~pool_infra_t ();
    virtual void scrub ();

    std::map<int64_t, int64_t> job2span;
    std::map<subsystem_t, planner_multi_t *> subplans;
    std::map<subsystem_t, uint64_t> colors;
};

struct relation_infra_t : public infra_base_t {
    relation_infra_t ();
    relation_infra_t (const relation_infra_t &o);
    relation_infra_t &operator= (const relation_infra_t &o);
    virtual ~relation_infra_t ();
    virtual void scrub ();

    uint64_t needs = 0;
    uint64_t best_k_cnt = 0;
    int exclusive = 0;
};

} // Flux::resource_model
} // Flux

#endif // INFRA_DATA_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

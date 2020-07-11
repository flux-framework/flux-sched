/*****************************************************************************\
 *  Copyright (c) 2020 Lawrence Livermore National Security, LLC.  Produced at
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

#include <algorithm>
#include <cctype>
#include "resource/evaluators/expr_eval_vtx_target.hpp"

namespace Flux {
namespace resource_model {

void vtx_predicates_override_t::set (bool sd, bool sna, bool sfr)
{
   if (sd)
       status_down = true;
   if (sna)
       sched_now_allocated = true;
   if (sfr)
       sched_future_reserved = true;
}

int expr_eval_vtx_target_t::validate (const std::string &p,
                                      const std::string &x) const
{
    int rc = -1;
    std::string lcx = x;

    if (!m_initialized) {
        errno = EINVAL;
        goto done;
    }
    std::transform (x.begin(), x.end(), lcx.begin(), ::tolower);
    if (p == "status")
        rc = (lcx == "down" || lcx == "up")? 0 : -1;
    else if (p == "sched-now")
        rc = (lcx == "allocated" || lcx == "free")? 0 : -1;
    else if (p == "sched-future")
        rc = (lcx == "reserved" || lcx == "free")? 0 : -1;
    else
        errno = EINVAL;
done:
    return rc;
}

int expr_eval_vtx_target_t::evaluate (const std::string &p,
                                      const std::string &x, bool &result) const
{
    int rc = 0;
    std::string lcx = x;

    if ( (rc = validate (p, x)) < 0)
        goto done;
    std::transform (x.begin(), x.end(), lcx.begin(), ::tolower);
    if (p == "status") {
        if (lcx == "down") {
            result = m_overridden.status_down
                     || ((*m_g)[m_u].status == resource_pool_t::status_t::DOWN);
        } else if (lcx == "up") {
            result = !m_overridden.status_down
                     && (*m_g)[m_u].status == resource_pool_t::status_t::UP;
        }
    } else if (p == "sched-now") {
        if (lcx == "allocated") {
            result = m_overridden.sched_now_allocated
                     || !(*m_g)[m_u].schedule.allocations.empty ();
        } else if (lcx == "free") {
            result = !m_overridden.sched_now_allocated
                     && (*m_g)[m_u].schedule.allocations.empty ();
        }
    } else if (p == "sched-future") {
        if (lcx == "reserved") {
            result = m_overridden.sched_future_reserved
                     || !(*m_g)[m_u].schedule.reservations.empty ();
        } else if (lcx == "free") {
            result = !m_overridden.sched_future_reserved
                     && (*m_g)[m_u].schedule.reservations.empty ();
        }
    } else {
        rc = -1;
        errno = EINVAL;
    }
done:
    return rc;
}

void expr_eval_vtx_target_t::initialize (const vtx_predicates_override_t &p,
                                         const std::shared_ptr<
                                             const f_resource_graph_t> g,
                                         vtx_t u)
{
    m_initialized = true;
    m_overridden = p;
    m_g = g;
    m_u = u;
}

} // Flux::resource_model
} // Flux

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

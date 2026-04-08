/*****************************************************************************\
 * Copyright 2020 Lawrence Livermore National Security, LLC
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

#include <algorithm>
#include <cctype>
#include <flux/hostlist.h>
#include <flux/core/job.h>
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

int expr_eval_vtx_target_t::validate (const std::string &p, const std::string &x) const
{
    int rc = -1;
    std::string lcx = x;  // lowercase version of x
    struct hostlist *hlist;
    flux_jobid_t id;

    if (!m_initialized) {
        errno = EINVAL;
        goto done;
    }
    std::transform (x.begin (), x.end (), lcx.begin (), ::tolower);
    if (p == "status")
        rc = (lcx == "down" || lcx == "up") ? 0 : -1;
    else if (p == "sched-now")
        rc = (lcx == "allocated" || lcx == "free") ? 0 : -1;
    else if (p == "sched-future")
        rc = (lcx == "reserved" || lcx == "free") ? 0 : -1;
    else if (p == "jobid-alloc" || p == "jobid-span" || p == "jobid-tag" || p == "jobid-reserved") {
        rc = flux_job_id_parse (x.c_str (), &id);  // must use non-lowercased version
    } else if (p == "agfilter") {
        rc = (lcx == "true" || lcx == "t" || lcx == "false" || lcx == "f") ? 0 : -1;
    } else if (p == "names") {
        hlist = hostlist_decode (lcx.c_str ());
        if (!hlist) {
            rc = -1;
        } else {
            rc = 0;
            hostlist_destroy (hlist);
        }
    } else if (p == "property") {
        rc = (lcx.length () > 0) ? 0 : -1;
    } else
        errno = EINVAL;
done:
    return rc;
}

int expr_eval_vtx_target_t::evaluate (const std::string &p,
                                      const std::string &x,
                                      bool &result) const
{
    int rc = 0;
    std::string lcx = x;
    flux_jobid_t jobid = 0;
    struct hostlist *hlist;

    result = false;
    if ((rc = validate (p, x)) < 0)
        goto done;
    std::transform (x.begin (), x.end (), lcx.begin (), ::tolower);
    if (p == "status") {
        if (lcx == "down") {
            result =
                m_overridden.status_down || ((*m_g)[m_u].status == resource_pool_t::status_t::DOWN);
        } else if (lcx == "up") {
            result =
                !m_overridden.status_down && (*m_g)[m_u].status == resource_pool_t::status_t::UP;
        }
    } else if (p == "sched-now") {
        if (lcx == "allocated") {
            result = m_overridden.sched_now_allocated || !(*m_g)[m_u].schedule.allocations.empty ();
        } else if (lcx == "free") {
            result = !m_overridden.sched_now_allocated && (*m_g)[m_u].schedule.allocations.empty ();
        }
    } else if (p == "sched-future") {
        if (lcx == "reserved") {
            result =
                m_overridden.sched_future_reserved || !(*m_g)[m_u].schedule.reservations.empty ();
        } else if (lcx == "free") {
            result =
                !m_overridden.sched_future_reserved && (*m_g)[m_u].schedule.reservations.empty ();
        }
    } else if (p == "jobid-alloc") {
        result = (flux_job_id_parse (x.c_str (), &jobid) == 0  // must use non-lowercased version
                  && (*m_g)[m_u].schedule.allocations.contains (jobid));
    } else if (p == "jobid-reserved") {
        result = (flux_job_id_parse (x.c_str (), &jobid) == 0  // must use non-lowercased version
                  && (*m_g)[m_u].schedule.reservations.contains (jobid));
    } else if (p == "jobid-span") {
        if ((flux_job_id_parse (x.c_str (), &jobid) != 0  // must use non-lowercased version
             || !(*m_g)[m_u].idata.job2span.contains (jobid))) {
            goto done;
        }
        result = true;
    } else if (p == "jobid-tag") {
        if ((flux_job_id_parse (x.c_str (), &jobid) != 0  // must use non-lowercased version
             || !(*m_g)[m_u].idata.tags.contains (jobid))) {
            goto done;
        }
        result = true;
    } else if (p == "agfilter") {
        if (lcx == "true" || lcx == "t") {
            result = true;
        } else {
            result = false;
        }
    } else if (p == "names") {
        if (!(hlist = hostlist_decode (lcx.c_str ()))) {
            rc = -1;
            goto done;
        }
        if (hostlist_find (hlist, (*m_g)[m_u].name.c_str ()) >= 0) {
            result = true;
        } else {
            result = false;
        }
        hostlist_destroy (hlist);
    } else if (p == "property") {
        result = (*m_g)[m_u].properties.contains (lcx);
    } else {
        rc = -1;
        errno = EINVAL;
    }
done:
    return rc;
}

int expr_eval_vtx_target_t::extract (
    const std::string &p,
    const std::string &x,
    std::vector<std::pair<std::string, std::string>> &predicates) const
{
    int rc = 0;
    std::string lcx = x;

    if (!m_initialized) {
        errno = EINVAL;
        rc = -1;
        goto done;
    }
    std::transform (x.begin (), x.end (), lcx.begin (), ::tolower);
    predicates.push_back ({p, lcx});
done:
    return rc;
}

void expr_eval_vtx_target_t::initialize (const vtx_predicates_override_t &p,
                                         const resource_graph_t *g,
                                         vtx_t u)
{
    m_initialized = true;
    m_overridden = p;
    m_g = g;
    m_u = u;
}

}  // namespace resource_model
}  // namespace Flux

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

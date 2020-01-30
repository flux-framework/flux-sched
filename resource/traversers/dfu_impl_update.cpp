/*****************************************************************************\
 *  Copyright (c) 2019 Lawrence Livermore National Security, LLC.  Produced at
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

#include "resource/traversers/dfu_impl.hpp"

extern "C" {
#if HAVE_CONFIG_H
#include "config.h"
#endif
}

using namespace Flux::Jobspec;
using namespace Flux::resource_model;
using namespace Flux::resource_model::detail;


/****************************************************************************
 *                                                                          *
 *         DFU Traverser Implementation Private Update API                  *
 *                                                                          *
 ****************************************************************************/

int dfu_impl_t::emit_vtx (vtx_t u, std::shared_ptr<match_writers_t> &w,
                          unsigned int needs, bool exclusive)
{
    return w->emit_vtx (level (), (*m_graph), u, needs, exclusive);
}

int dfu_impl_t::emit_edg (edg_t e, std::shared_ptr<match_writers_t> &w)
{
     return w->emit_edg (level (), (*m_graph), e);
}

int dfu_impl_t::upd_txfilter (vtx_t u, const jobmeta_t &jobmeta,
                              const std::map<std::string, int64_t> &dfu)
{
    // idata tag and exclusive checker update
    int64_t span = -1;
    planner_t *x_checker = NULL;

    // Tag on a vertex with exclusive access or all of its ancestors
    (*m_graph)[u].idata.tags[jobmeta.jobid] = jobmeta.jobid;
    // Update x_checker used for quick exclusivity check during matching
    if ( (x_checker = (*m_graph)[u].idata.x_checker) == NULL) {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": x_checker not installed.\n";
        return -1;
    }
    if ( (span = planner_add_span (x_checker, jobmeta.at,
                                   jobmeta.duration, 1)) == -1) {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": planner_add_span returned -1.\n";
        m_err_msg += strerror (errno);
        m_err_msg += "\n";
        return -1;
    }
    (*m_graph)[u].idata.x_spans[jobmeta.jobid] = span;
    return 0;
}

int dfu_impl_t::upd_agfilter (vtx_t u, const subsystem_t &s,
                              const jobmeta_t &jobmeta,
                              const std::map<std::string, int64_t> &dfu)
{
    // idata subtree aggregate prunning filter
    planner_multi_t *subtree_plan = (*m_graph)[u].idata.subplans[s];
    if (subtree_plan) {
        int64_t span = -1;
        std::vector<uint64_t> aggregate;
        // Update the subtree aggregate pruning filter of this vertex
        // using the new aggregates passed by dfu.
        count_relevant_types (subtree_plan, dfu, aggregate);
        span = planner_multi_add_span (subtree_plan,
                                       jobmeta.at, jobmeta.duration,
                                       &(aggregate[0]), aggregate.size ());
        if (span == -1) {
            m_err_msg += __FUNCTION__;
            m_err_msg += ": planner_multi_add_span returned -1.\n";
            m_err_msg += strerror (errno);
            m_err_msg += "\n";
            return -1;
        }
        (*m_graph)[u].idata.job2span[jobmeta.jobid] = span;
    }
    return 0;
}

int dfu_impl_t::upd_idata (vtx_t u, const subsystem_t &s,
                          const jobmeta_t &jobmeta,
                          const std::map<std::string, int64_t> &dfu)
{
    int rc = 0;
    if ( (rc = upd_txfilter (u, jobmeta, dfu)) != 0)
        goto done;
    if ( (rc = upd_agfilter (u, s, jobmeta, dfu)) != 0)
        goto done;
done:
    return rc;
}

int dfu_impl_t::upd_plan (vtx_t u, const subsystem_t &s, unsigned int needs,
                          bool excl,  const jobmeta_t &jobmeta, bool full,
                          int &n)
{
    if (excl) {

        n++;
	if (!full) {
            // If not full mode, plan has already been updated, thus return.
            return 0;
        }

        int64_t span = -1;
        planner_t *plans = NULL;

        if ( (plans = (*m_graph)[u].schedule.plans) == NULL) {
            m_err_msg += __FUNCTION__;
            m_err_msg += ": plans not installed.\n";
        }
        if ( (span = planner_add_span (plans, jobmeta.at, jobmeta.duration,
                                       (const uint64_t)needs)) == -1) {
            m_err_msg += __FUNCTION__;
            m_err_msg += ": planner_add_span returned -1.\n";
            if (errno != 0) {
                m_err_msg += strerror (errno);
                m_err_msg += "\n";
            }
            return -1;
        }
        if (jobmeta.allocate) {
            (*m_graph)[u].schedule.allocations.spantype[jobmeta.jobid].span = span;
            (*m_graph)[u].schedule.allocations.spantype[jobmeta.jobid].jobtype = jobmeta.jobtype;

            if (jobmeta.jobtype == "elastic")
                ++(*m_graph)[u].schedule.allocations.elasticcount;
       }
        else {
            (*m_graph)[u].schedule.reservations.spantype[jobmeta.jobid].span = span;
            (*m_graph)[u].schedule.reservations.spantype[jobmeta.jobid].jobtype = jobmeta.jobtype;
        }
    }
    return 0;
}

int dfu_impl_t::accum_to_parent (vtx_t u, const subsystem_t &subsystem,
                                 unsigned int needs, bool excl,
                                 const std::map<std::string, int64_t> &dfu,
                                 std::map<std::string, int64_t> &to_parent)
{
    // Build up the new aggregates that will be used by subtree
    // aggregate pruning filter. If exclusive, none of the vertex's resource
    // is available (0). If not, all will be available (needs).
    if (excl)
        accum_if (subsystem, (*m_graph)[u].type, 0, to_parent);
    else
        accum_if (subsystem, (*m_graph)[u].type, needs, to_parent);

    // Pass up the new subtree aggregates collected so far to the parent.
    for (auto &kv : dfu)
        accum_if (subsystem, kv.first, kv.second, to_parent);

    return 0;
}

int dfu_impl_t::upd_meta (vtx_t u, const subsystem_t &s, unsigned int needs,
                          bool excl, int n, const jobmeta_t &jobmeta,
                          const std::map<std::string, int64_t> &dfu,
                          std::map<std::string, int64_t> &to_parent)
{
    int rc = 0;
    if (n == 0)
        goto done;
    if ( (rc = upd_idata (u, s, jobmeta, dfu)) == -1)
        goto done;
    if ( (rc = accum_to_parent (u, s, needs, excl, dfu, to_parent)) == -1)
        goto done;
done:
    return rc;
}

int dfu_impl_t::upd_sched (vtx_t u, std::shared_ptr<match_writers_t> &writers,
                           const subsystem_t &s, unsigned int needs, bool excl,
                           int n, const jobmeta_t &jobmeta, bool full,
                           const std::map<std::string, int64_t> &dfu,
                           std::map<std::string, int64_t> &to_parent)
{
    int rc = -1;
    if ( (rc = upd_plan (u, s, needs, excl, jobmeta, full, n)) == -1)
        goto done;
    if ( (rc = upd_meta (u, s, needs, excl, n, jobmeta, dfu, to_parent)) == -1) {
        goto done;
    }
    if (n > 0) {
        if ( (rc = emit_vtx (u, writers, needs, excl)) == -1) {
            m_err_msg += __FUNCTION__;
            m_err_msg += ": emit_vtx returned -1.\n";
        }
    }
    m_trav_level--;

done:
    return n;
}

int dfu_impl_t::upd_upv (vtx_t u, std::shared_ptr<match_writers_t> &writers,
                         const subsystem_t &subsystem,
                         unsigned int needs, bool excl,
                         const jobmeta_t &jobmeta, bool full,
                         std::map<std::string, int64_t> &to_parent)
{
    //NYI: update resources on the UPV direction
    return 0;
}

int dfu_impl_t::upd_dfv (vtx_t u, std::shared_ptr<match_writers_t> &writers,
                         unsigned int needs, bool excl,
                         const jobmeta_t &jobmeta, bool full,
                         std::map<std::string, int64_t> &to_parent)
{
    int n_plans = 0;
    std::map<std::string, int64_t> dfu;
    const std::string &dom = m_match->dom_subsystem ();
    f_out_edg_iterator_t ei, ei_end;
    m_trav_level++;
    for (auto &subsystem : m_match->subsystems ()) {
        for (tie (ei, ei_end) = out_edges (u, *m_graph); ei != ei_end; ++ei) {
            if (!in_subsystem (*ei, subsystem) || stop_explore (*ei, subsystem))
                continue;
            if ((*m_graph)[*ei].idata.get_trav_token () != m_best_k_cnt)
                continue;

            int n_plan_sub = 0;
            bool x = (*m_graph)[*ei].idata.get_exclusive ();
            unsigned int needs = (*m_graph)[*ei].idata.get_needs ();
            vtx_t tgt = target (*ei, *m_graph);
            if (subsystem == dom) {
                n_plan_sub += upd_dfv (tgt, writers,
                                       needs, x, jobmeta, full, dfu);
            } else {
                n_plan_sub += upd_upv (tgt, writers, subsystem,
                                       needs, x, jobmeta, full, dfu);
            }

            if (n_plan_sub > 0) {
                if (emit_edg (*ei, writers) == -1) {
                    m_err_msg += __FUNCTION__;
                    m_err_msg += ": emit_edg returned -1.\n";
                }
                n_plans += n_plan_sub;
            }
        }
    }
    (*m_graph)[u].idata.colors[dom] = m_color.black ();
    return upd_sched (u, writers, dom, needs,
                      excl, n_plans, jobmeta, full, dfu, to_parent);
}

int dfu_impl_t::rem_txfilter (vtx_t u, int64_t jobid, bool &stop)
{
    int rc = -1;
    int64_t span = -1;
    planner_t *x_checker = NULL;
    auto &x_spans = (*m_graph)[u].idata.x_spans;
    auto &tags = (*m_graph)[u].idata.tags;

    if (tags.find (jobid) == tags.end ()) {
        stop = true;
        rc = 0;
        goto done;
    }
    if (x_spans.find (jobid) == x_spans.end ()) {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": jobid isn't found in x_spans table.\n ";
        goto done;
    }

    x_checker = (*m_graph)[u].idata.x_checker;
    (*m_graph)[u].idata.tags.erase (jobid);
    span = (*m_graph)[u].idata.x_spans[jobid];
    (*m_graph)[u].idata.x_spans.erase (jobid);
    if ( (rc = planner_rem_span (x_checker, span)) == -1) {
        m_err_msg += __FUNCTION__;
        m_err_msg += "planner_rem_span returned -1.\n";
        m_err_msg += (*m_graph)[u].name + ".\n";
        m_err_msg += strerror (errno);
        m_err_msg += ".\n";
    }

done:
    return rc;
}

int dfu_impl_t::rem_agfilter (vtx_t u, int64_t jobid,
                              const std::string &subsystem)
{
    int rc = 0;
    int span = -1;
    planner_multi_t *subtree_plan = NULL;
    auto &job2span = (*m_graph)[u].idata.job2span;

    if ((subtree_plan = (*m_graph)[u].idata.subplans[subsystem]) == NULL)
        goto done;
    if (job2span.find (jobid) == job2span.end ())
        goto done;
    if ((span = job2span[jobid]) == -1) {
        rc = -1;
        goto done;
    }
    if ((rc = planner_multi_rem_span (subtree_plan, span)) != 0) {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": planner_multi_rem_span returned -1.\n";
        m_err_msg += (*m_graph)[u].name + ".\n";
        m_err_msg += strerror (errno);
        m_err_msg += "\n";
    }

done:
    return rc;
}

int dfu_impl_t::rem_idata (vtx_t u, int64_t jobid,
                           const std::string &subsystem, bool &stop)
{
    int rc = -1;

    if ( (rc = rem_txfilter (u, jobid, stop)) != 0 || stop)
        goto done;
    if ( (rc = rem_agfilter (u, jobid, subsystem)) != 0)
        goto done;

done:
    return rc;
}

int dfu_impl_t::rem_plan (vtx_t u, int64_t jobid)
{
    int rc = 0;
    int64_t span = -1;
    planner_t *plans = NULL;

    if ((*m_graph)[u].schedule.allocations.spantype.find (jobid)
        != (*m_graph)[u].schedule.allocations.spantype.end ()) {
        span = (*m_graph)[u].schedule.allocations.spantype[jobid].span;
        (*m_graph)[u].schedule.allocations.erase (jobid);
    } else if ((*m_graph)[u].schedule.reservations.spantype.find (jobid)
               != (*m_graph)[u].schedule.reservations.spantype.end ()) {
        span = (*m_graph)[u].schedule.reservations.spantype[jobid].span;
        (*m_graph)[u].schedule.reservations.erase (jobid);
    } else {
        goto done;
    }

    plans = (*m_graph)[u].schedule.plans;
    if ( (rc = planner_rem_span (plans, span)) == -1) {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": planner_rem_span returned -1.\n";
        m_err_msg += (*m_graph)[u].name + ".\n";
        m_err_msg += strerror (errno);
        m_err_msg += ".\n";
    }

done:
    return rc;
}

int dfu_impl_t::rem_upv (vtx_t u, int64_t jobid)
{
    // NYI: remove schedule data for upwalk
    return 0;
}

int dfu_impl_t::rem_dfv (vtx_t u, int64_t jobid)
{
    int rc = 0;
    bool stop = false;
    const std::string &dom = m_match->dom_subsystem ();
    f_out_edg_iterator_t ei, ei_end;

    if ( (rc = rem_idata (u, jobid, dom, stop)) != 0 || stop)
        goto done;
    if ( (rc = rem_plan (u, jobid)) != 0)
        goto done;
    for (auto &subsystem : m_match->subsystems ()) {
        for (tie (ei, ei_end) = out_edges (u, *m_graph); ei != ei_end; ++ei) {
            if (!in_subsystem (*ei, subsystem) || stop_explore (*ei, subsystem))
                continue;
            vtx_t tgt = target (*ei, *m_graph);
            if (subsystem == dom)
                rc += rem_dfv (tgt, jobid);
            else
                rc += rem_upv (tgt, jobid);
        }
    }
done:
    return rc;
}

int dfu_impl_t::rem_exv (int64_t jobid)
{
    int rc = -1;
    int64_t span = -1;
    vtx_iterator_t vi, v_end;
    edg_iterator_t ei, e_end;
    resource_graph_t &g = m_graph_db->resource_graph;

    // Exhausitive visit (exv) is required when jobid came from an allocation
    // created by a traverser different from this traverser, for example, one
    // that uses a different dominant subsystem. If this code is used
    // in Flux's resource module, this condition can arise when this module
    // is reloaded with a different match policy and the job-manager wants
    // to reconstruct a previously allocated job.
    // In this case, you can't find allocated resources from an accelerated
    // depth first visit (dfv). There won't be no idata for that allocation.
    for (boost::tie (vi, v_end) = boost::vertices (g); vi != v_end; ++vi) {
        if (g[*vi].schedule.allocations.spantype.find (jobid)
            != g[*vi].schedule.allocations.spantype.end ()) {
            span = g[*vi].schedule.allocations.spantype[jobid].span;
            g[*vi].schedule.allocations.erase (jobid);
        } else if (g[*vi].schedule.reservations.spantype.find (jobid)
                   != g[*vi].schedule.reservations.spantype.end ()) {
            span = g[*vi].schedule.reservations.spantype[jobid].span;
            g[*vi].schedule.reservations.erase (jobid);
        } else {
            continue;
        }

        if ( (rc += planner_rem_span (g[*vi].schedule.plans, span)) == -1) {
            m_err_msg += __FUNCTION__;
            m_err_msg += ": planner_rem_span returned -1.\n";
            m_err_msg += "name=" + g[*vi].name + "uniq_id=";
            m_err_msg + std::to_string (g[*vi].uniq_id) + ".\n";
            m_err_msg += strerror (errno);
            m_err_msg += ".\n";
        }
    }

    return (!rc)? 0 : -1;
}


/****************************************************************************
 *                                                                          *
 *              DFU Traverser Implementation Update API                     *
 *                                                                          *
 ****************************************************************************/

int dfu_impl_t::update (vtx_t root, std::shared_ptr<match_writers_t> &writers,
                        jobmeta_t &jobmeta)
{
    int rc = -1;
    std::map<std::string, int64_t> dfu;
    const std::string &dom = m_match->dom_subsystem ();

    if (m_graph_db->metadata.v_rt_edges[dom].get_trav_token ()
        != m_best_k_cnt) {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": resource state wasn't properly set up for update.\n";
        return -1;
    }

    unsigned int excl = m_graph_db->metadata.v_rt_edges[dom].get_exclusive ();
    bool x = (excl == 0)? false : true;
    unsigned int needs = m_graph_db->metadata.v_rt_edges[dom].get_needs ();
    m_color.reset ();

    rc = upd_dfv (root, writers, needs, x, jobmeta, true, dfu);
    return (rc > 0)? 0 : -1;
}

int dfu_impl_t::update ()
{
    m_color.reset ();
    return 0;
}

int dfu_impl_t::update (vtx_t root, std::shared_ptr<match_writers_t> &writers,
                        const std::string &str,
                        std::shared_ptr<resource_reader_base_t> &reader,
                        jobmeta_t &jobmeta)
{
    int rc = -1;
    bool x = false;
    unsigned int excl = 0;
    unsigned int needs = 0;
    std::map<std::string, int64_t> dfu;
    const std::string &dom = m_match->dom_subsystem ();

    tick ();
    if ( (rc = reader->update (m_graph_db->resource_graph,
                               m_graph_db->metadata, str,
                               jobmeta.jobid, jobmeta.at,
                               jobmeta.duration,
                               !jobmeta.allocate, m_best_k_cnt)) != 0) {
        m_err_msg += reader->err_message ();
        reader->clear_err_message ();
        return rc;
    }

    if (m_graph_db->metadata.v_rt_edges[dom].get_trav_token ()
        != m_best_k_cnt) {
        // This condition occurs when the subgraph came from a
        // traverver different from this traverser, for example,
        // a traverser whose dominant subsystem is different than this.
        return 0;
    }

    excl = m_graph_db->metadata.v_rt_edges[dom].get_exclusive ();
    x = (excl == 0)? false : true;
    needs = static_cast<unsigned int>(m_graph_db->metadata
                                          .v_rt_edges[dom].get_needs ());
    m_color.reset ();
    return (upd_dfv (root, writers, needs, x, jobmeta, false, dfu) > 0)? 0: -1;
}

int dfu_impl_t::remove (vtx_t root, int64_t jobid)
{
    int rc = -1;
    bool root_has_jtag = ((*m_graph)[root].idata.tags.find (jobid)
                          != (*m_graph)[root].idata.tags.end ());
    m_color.reset ();
    return (root_has_jtag)? rem_dfv (root, jobid) : rem_exv (jobid);
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

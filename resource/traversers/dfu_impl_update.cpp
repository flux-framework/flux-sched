/*****************************************************************************\
 * Copyright 2019 Lawrence Livermore National Security, LLC
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

#include <boost/optional/optional.hpp>

#include "resource/traversers/dfu_impl.hpp"

using namespace Flux::Jobspec;
using namespace Flux::resource_model;
using namespace Flux::resource_model::detail;

////////////////////////////////////////////////////////////////////////////////
// DFU Traverser Implementation Private Update API
////////////////////////////////////////////////////////////////////////////////

int dfu_impl_t::emit_vtx (vtx_t u,
                          std::shared_ptr<match_writers_t> &w,
                          unsigned int needs,
                          bool exclusive,
                          bool excl_parent)
{
    const std::map<std::string, std::string> agfilter_data;
    return w->emit_vtx (level (), (*m_graph), u, needs, agfilter_data, exclusive, excl_parent);
}

int dfu_impl_t::emit_edg (edg_t e, std::shared_ptr<match_writers_t> &w, bool excl_parent)
{
    return w->emit_edg (level (), (*m_graph), e, excl_parent);
}

int dfu_impl_t::upd_txfilter (vtx_t u,
                              const jobmeta_t &jobmeta,
                              const std::map<resource_type_t, int64_t> &dfu)
{
    // idata tag and exclusive checker update
    int64_t span = -1;
    planner_t *x_checker = NULL;

    // Tag on a vertex with exclusive access or all of its ancestors
    (*m_graph)[u].idata.tags[jobmeta.jobid] = (jobmeta.jobid);
    // Every vertex receiving job-keyed state gets a tag here first, so
    // this is the single site that indexes the vertex under the job.
    m_graph_db->metadata.add_job_vertex (jobmeta.jobid, u);
    // Update x_checker used for quick exclusivity check during matching
    if ((x_checker = (*m_graph)[u].idata.x_checker) == NULL) {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": x_checker not installed.\n";
        return -1;
    }
    if ((span = planner_add_span (x_checker, jobmeta.at, jobmeta.duration, 1)) == -1) {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": planner_add_span returned -1.\n";
        m_err_msg += strerror (errno);
        m_err_msg += "\n";
        return -1;
    }
    (*m_graph)[u].idata.x_spans[jobmeta.jobid] = span;
    return 0;
}

int dfu_impl_t::upd_agfilter (vtx_t u,
                              subsystem_t s,
                              jobmeta_t jobmeta,
                              const std::map<resource_type_t, int64_t> &dfu)
{
    // idata subtree aggregate pruning filter
    boost::optional<planner_multi_t *&> opt_subtree_plan = (*m_graph)[u].idata.subplans.try_at (s);
    if (opt_subtree_plan && *opt_subtree_plan && !dfu.empty ()) {
        int64_t span = -1;
        std::vector<uint64_t> aggregate;
        // Update the subtree aggregate pruning filter of this vertex
        // using the new aggregates passed by dfu.
        count_relevant_types (*opt_subtree_plan, dfu, aggregate);
        span = planner_multi_add_span (*opt_subtree_plan,
                                       jobmeta.at,
                                       jobmeta.duration,
                                       aggregate.data (),
                                       aggregate.size ());
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

int dfu_impl_t::upd_idata (vtx_t u,
                           subsystem_t s,
                           jobmeta_t jobmeta,
                           const std::map<resource_type_t, int64_t> &dfu)
{
    int rc = 0;
    if ((rc = upd_txfilter (u, jobmeta, dfu)) != 0)
        goto done;
    if ((rc = upd_agfilter (u, s, jobmeta, dfu)) != 0)
        goto done;
done:
    return rc;
}

int dfu_impl_t::upd_by_outedges (subsystem_t subsystem, jobmeta_t jobmeta, vtx_t u, edg_t e)
{
    size_t len = 0;
    vtx_t tgt = target (e, *m_graph);
    boost::optional<planner_multi_t *&> opt_subplan =
        (*m_graph)[tgt].idata.subplans.try_at (subsystem);
    if (opt_subplan && *opt_subplan) {
        if ((len = planner_multi_resources_len (*opt_subplan)) == 0)
            return -1;

        // Set dynamic traversing order based on the following heuristics:
        //     1. Current-time (jobmeta.now) resource availability
        //     2. Last pruning filter resource type (if additional
        //        pruning filter type was given, that's a good
        //        indication that it is the scarcest resource)
        int64_t avail = planner_multi_avail_resources_at (*opt_subplan, jobmeta.now, len - 1);
        // Special case to skip (e.g., leaf resource vertices)
        if (avail == 0 && planner_multi_span_size (*opt_subplan) == 0)
            return 0;

        auto key = std::make_pair ((*m_graph)[e].idata.get_weight (), (*m_graph)[tgt].uniq_id);
        m_graph_db->metadata.by_outedges[u].erase (key);

        (*m_graph)[e].idata.set_weight ((avail == -1) ? 0 : avail);
        key = std::make_pair ((*m_graph)[e].idata.get_weight (), (*m_graph)[tgt].uniq_id);
        // Reinsert so that outedges are maintained according to the current
        // resource availability state. Leverage the fact that std::map
        // uses a RedBlack tree keep its elemented in sorted order.
        auto ret = m_graph_db->metadata.by_outedges[u].insert (std::make_pair (key, e));
        if (!ret.second) {
            errno = ENOMEM;
            return -1;
        }
    }
    return 0;
}

int dfu_impl_t::upd_plan (vtx_t u,
                          subsystem_t s,
                          unsigned int needs,
                          bool excl,
                          const jobmeta_t &jobmeta,
                          bool full,
                          int &n)
{
    int rc = 0;
    int64_t span = -1;
    planner_t *plans = NULL;

    if (excl) {
        n++;
        if (!full) {
            // If not full mode, plan has already been updated, thus return.
            return 0;
        }

        if ((plans = (*m_graph)[u].schedule.plans) == NULL) {
            m_err_msg += __FUNCTION__;
            m_err_msg += ": plans not installed.\n";
        }
        if ((span = planner_add_span (plans, jobmeta.at, jobmeta.duration, (const uint64_t)needs))
            == -1) {
            m_err_msg += __FUNCTION__;
            m_err_msg += ": planner_add_span returned -1.\n";
            if (errno != 0) {
                m_err_msg += strerror (errno);
                m_err_msg += "\n";
            }
            rc = -1;
            goto done;
        }

        switch (jobmeta.alloc_type) {
            case jobmeta_t::alloc_type_t::AT_ALLOC:
                (*m_graph)[u].schedule.allocations[jobmeta.jobid] = span;
                break;
            case jobmeta_t::alloc_type_t::AT_ALLOC_ORELSE_RESERVE:
                (*m_graph)[u].schedule.reservations[jobmeta.jobid] = span;
                break;
            case jobmeta_t::alloc_type_t::AT_SATISFIABILITY:
                break;
            default:
                rc = -1;
                errno = EINVAL;
                break;
        }
    }

done:
    return rc;
}

int dfu_impl_t::accum_to_parent (vtx_t u,
                                 subsystem_t subsystem,
                                 unsigned int needs,
                                 bool excl,
                                 const std::map<resource_type_t, int64_t> &dfu,
                                 std::map<resource_type_t, int64_t> &to_parent)
{
    // Build up the new aggregates that will be used by subtree
    // aggregate pruning filter. If exclusive, none of the vertex's resource
    // is available (size). If not, all will be available (size - needs).
    if (excl)
        accum_if (subsystem, (*m_graph)[u].type, (*m_graph)[u].size, to_parent);
    else
        accum_if (subsystem, (*m_graph)[u].type, (*m_graph)[u].size - needs, to_parent);

    // Pass up the new subtree aggregates collected so far to the parent.
    for (auto &kv : dfu)
        accum_if (subsystem, kv.first, kv.second, to_parent);

    return 0;
}

int dfu_impl_t::upd_meta (vtx_t u,
                          subsystem_t s,
                          unsigned int needs,
                          bool excl,
                          int n,
                          const jobmeta_t &jobmeta,
                          const std::map<resource_type_t, int64_t> &dfu,
                          std::map<resource_type_t, int64_t> &to_parent)
{
    int rc = 0;
    if (n == 0)
        goto done;
    if ((rc = upd_idata (u, s, jobmeta, dfu)) == -1)
        goto done;
    if ((rc = accum_to_parent (u, s, needs, excl, dfu, to_parent)) == -1)
        goto done;
done:
    return rc;
}

int dfu_impl_t::upd_sched (vtx_t u,
                           std::shared_ptr<match_writers_t> &writers,
                           subsystem_t s,
                           unsigned int needs,
                           bool excl,
                           int n,
                           const jobmeta_t &jobmeta,
                           bool full,
                           const std::map<resource_type_t, int64_t> &dfu,
                           std::map<resource_type_t, int64_t> &to_parent,
                           bool excl_parent)
{
    int rc = -1;

    // No need to update scheduling if NO_ALLOC is set but still count exclusive vertices
    if (jobmeta.alloc_type == jobmeta_t::alloc_type_t::AT_NO_ALLOC) {
        if (excl)
            n++;
    } else {
        if ((rc = upd_plan (u, s, needs, excl, jobmeta, full, n)) == -1)
            goto done;
        if ((rc = upd_meta (u, s, needs, excl, n, jobmeta, dfu, to_parent)) == -1) {
            goto done;
        }
    }
    if (n > 0) {
        if ((rc = emit_vtx (u, writers, needs, excl, excl_parent)) == -1) {
            m_err_msg += __FUNCTION__;
            m_err_msg += ": emit_vtx returned -1.\n";
        }
    }
    m_trav_level--;

done:
    return n;
}

int dfu_impl_t::upd_upv (vtx_t u,
                         std::shared_ptr<match_writers_t> &writers,
                         subsystem_t subsystem,
                         unsigned int needs,
                         bool excl,
                         const jobmeta_t &jobmeta,
                         bool full,
                         std::map<resource_type_t, int64_t> &to_parent)
{
    // NYI: update resources on the UPV direction
    return 0;
}

bool dfu_impl_t::modify_traversal (vtx_t u, bool emit_shadow_from_parent) const
{
    // We modify our traversal if the parent says so if the
    // visiting vertex resource type is exclusive by configuration
    return emit_shadow_from_parent || m_match->is_resource_type_exclusive ((*m_graph)[u].type);
}

bool dfu_impl_t::stop_explore_best (edg_t e, bool mod_trav) const
{
    return (*m_graph)[e].idata.get_sequence_number () != m_sequence_number && !mod_trav;
}

bool dfu_impl_t::get_eff_exclusive (bool x, bool mod_trav) const
{
    return x || mod_trav;
}

unsigned dfu_impl_t::get_eff_needs (unsigned needs, unsigned size, bool mod_trav) const
{
    return mod_trav ? size : needs;
}

int dfu_impl_t::upd_dfv (vtx_t u,
                         std::shared_ptr<match_writers_t> &writers,
                         unsigned int needs,
                         bool excl,
                         const jobmeta_t &jobmeta,
                         bool full,
                         std::map<resource_type_t, int64_t> &to_parent,
                         bool emit_shadow,
                         bool excl_parent)
{
    int n_plans = 0;
    std::map<resource_type_t, int64_t> dfu;
    subsystem_t dom = m_match->dom_subsystem ();
    f_out_edg_iterator_t ei, ei_end;
    bool mod = modify_traversal (u, emit_shadow);
    excl = excl || mod;
    m_trav_level++;
    (*m_graph)[u].idata.colors[dom] = m_color.gray ();
    for (auto &subsystem : m_match->subsystems ()) {
        for (tie (ei, ei_end) = out_edges (u, *m_graph); ei != ei_end; ++ei) {
            if (!in_subsystem (*ei, subsystem) || stop_explore (*ei, subsystem))
                continue;

            if (stop_explore_best (*ei, mod))
                continue;

            vtx_t tgt = target (*ei, *m_graph);
            int n_plan_sub = 0;
            bool x = get_eff_exclusive ((*m_graph)[*ei].idata.get_exclusive (), mod);
            unsigned needs =
                get_eff_needs ((*m_graph)[*ei].idata.get_needs (), (*m_graph)[tgt].size, mod);

            if (subsystem == dom) {
                // Value of `excl_parent` for child vertex is the value of `excl` for its parent
                n_plan_sub += upd_dfv (tgt, writers, needs, x, jobmeta, full, dfu, mod, excl);
            } else {
                n_plan_sub += upd_upv (tgt, writers, subsystem, needs, x, jobmeta, full, dfu);
            }

            if (n_plan_sub > 0) {
                if (m_match->get_stop_on_k_matches () > 0
                    && upd_by_outedges (subsystem, jobmeta, u, *ei) < 0) {
                    m_err_msg += __FUNCTION__;
                    m_err_msg += ": upd_by_outedges returned -1.\n";
                }
                if (emit_edg (*ei, writers, excl) == -1) {
                    m_err_msg += __FUNCTION__;
                    m_err_msg += ": emit_edg returned -1.\n";
                }
                n_plans += n_plan_sub;
            }
        }
    }
    (*m_graph)[u].idata.colors[dom] = m_color.black ();
    return upd_sched (u,
                      writers,
                      dom,
                      needs,
                      excl,
                      n_plans,
                      jobmeta,
                      full,
                      dfu,
                      to_parent,
                      excl_parent);
}

int dfu_impl_t::rem_exclusive_filter (vtx_t u, int64_t jobid, const modify_data_t &mod_data)
{
    int rc = -1;
    int64_t span = -1;
    planner_t *x_checker = NULL;

    auto span_it = (*m_graph)[u].idata.x_spans.find (jobid);
    if (span_it == (*m_graph)[u].idata.x_spans.end ()) {
        if (mod_data.mod_type == job_modify_t::VTX_CANCEL) {
            m_err_msg += __FUNCTION__;
            m_err_msg += ": jobid isn't found in x_spans table.\n ";
            goto done;
        } else {
            // Valid for CANCEL if clearing inconsistency between
            // core and sched.
            rc = 0;
            goto done;
        }
    }

    x_checker = (*m_graph)[u].idata.x_checker;
    span = span_it->second;
    (*m_graph)[u].idata.x_spans.erase (span_it);
    if ((rc = planner_rem_span (x_checker, span)) == -1) {
        m_err_msg += __FUNCTION__;
        m_err_msg += "planner_rem_span returned -1.\n";
        m_err_msg += (*m_graph)[u].name + ".\n";
        m_err_msg += strerror (errno);
        m_err_msg += ".\n";
    }

done:
    return rc;
}

bool dfu_impl_t::rem_tag (vtx_t u, int64_t jobid)
{
    auto tag_it = (*m_graph)[u].idata.tags.find (jobid);
    if (tag_it == (*m_graph)[u].idata.tags.end ()) {
        // stop removal
        return true;
    } else {
        (*m_graph)[u].idata.tags.erase (tag_it);
        return false;
    }
}

int dfu_impl_t::mod_agfilter (vtx_t u,
                              int64_t jobid,
                              subsystem_t subsystem,
                              const modify_data_t &mod_data,
                              bool &stop)
{
    int rc = 0;
    bool removed = false;
    boost::optional<planner_multi_t *&> opt_subtree_plan;
    auto &job2span = (*m_graph)[u].idata.job2span;
    std::map<int64_t, int64_t>::iterator span_it;

    opt_subtree_plan = (*m_graph)[u].idata.subplans.try_at (subsystem);
    if (!opt_subtree_plan || !(*opt_subtree_plan))
        goto done;
    span_it = job2span.find (jobid);
    if (span_it == job2span.end ()) {
        if (mod_data.mod_type == job_modify_t::PARTIAL_CANCEL)
            stop = true;
        goto done;
    }
    if (span_it->second == -1) {
        rc = -1;
        goto done;
    }
    if (mod_data.mod_type != job_modify_t::PARTIAL_CANCEL) {
        if ((rc = planner_multi_rem_span (*opt_subtree_plan, span_it->second)) != 0) {
            m_err_msg += __FUNCTION__;
            m_err_msg += ": planner_multi_rem_span returned -1.\n";
            m_err_msg += (*m_graph)[u].name + " " + strerror (errno) + ".\n";
            goto done;
        }
        job2span.erase (span_it);
    } else {  // PARTIAL_CANCEL
        if ((*m_graph)[u].idata.tags.find (jobid) == (*m_graph)[u].idata.tags.end ()) {
            // stop removal
            stop = true;
            goto done;
        }
        if (mod_data.type_to_count.size () > 0) {
            std::vector<const char *> reduced_types;
            std::vector<uint64_t> reduced_counts;
            for (const auto &t2ct_it : mod_data.type_to_count) {
                reduced_types.push_back (t2ct_it.first.c_str ());
                reduced_counts.push_back (t2ct_it.second);
            }
            if ((rc = planner_multi_reduce_span (*opt_subtree_plan,
                                                 span_it->second,
                                                 reduced_counts.data (),
                                                 reduced_types.data (),
                                                 mod_data.type_to_count.size (),
                                                 removed))
                != 0) {
                m_err_msg += __FUNCTION__;
                m_err_msg += ": planner_multi_reduce_span returned -1.\n";
                m_err_msg += (*m_graph)[u].name + " " + strerror (errno) + ".\n";
                goto done;
            }
        } else {
            m_err_msg += __FUNCTION__;
            m_err_msg += ": type_to_count empty.\n";
            m_err_msg += (*m_graph)[u].name + " " + strerror (errno) + ".\n";
            rc = -1;
            goto done;
        }
        if (removed) {
            // The aggregate span is fully drained and gone from the
            // planner; drop the dangling span id. The vertex's tag and
            // exclusive-filter span are owned by the final purge
            // (cancel_vertex with CANCEL): removing them here would let
            // another job match this vertex exclusively while this job
            // still holds resources beneath it, and severs the state
            // needed for exact full-cancel accounting.
            job2span.erase (span_it);
        }
    }

done:
    return rc;
}

int dfu_impl_t::mod_idata (vtx_t u,
                           int64_t jobid,
                           subsystem_t subsystem,
                           const modify_data_t &mod_data,
                           bool &stop)
{
    // Only remove the txfilter span and tag first if we're completely
    // cancelling the vertex
    if (mod_data.mod_type != job_modify_t::PARTIAL_CANCEL) {
        // returns true if stopping
        if ((stop = rem_tag (u, jobid)))
            return 0;
        if (rem_exclusive_filter (u, jobid, mod_data) != 0)
            return -1;
    }
    // If mod_type == job_modify_t::PARTIAL_CANCEL here, mod_agfilter
    // only reduces the aggregate-filter span; tags and exclusive-filter
    // spans are removed by the final purge (CANCEL) alone.
    return mod_agfilter (u, jobid, subsystem, mod_data, stop);
}

int dfu_impl_t::mod_plan (vtx_t u, int64_t jobid, modify_data_t &mod_data)
{
    int rc = 0;
    int64_t span = -1;
    int64_t prev_count = -1;
    int64_t to_remove = 0;
    bool removed = false;
    std::map<int64_t, int64_t>::iterator alloc_span;
    std::map<int64_t, int64_t>::iterator res_span;
    planner_t *plans = NULL;

    alloc_span = (*m_graph)[u].schedule.allocations.find (jobid);
    if (alloc_span != (*m_graph)[u].schedule.allocations.end ()) {
        span = alloc_span->second;
        if (mod_data.mod_type != job_modify_t::PARTIAL_CANCEL) {
            (*m_graph)[u].schedule.allocations.erase (alloc_span);
        } else {
            // This condition is encountered when the vertex is
            // not associated with a broker rank. We may need
            // extra logic here to handle more advanced partial
            // cancel in the future.
            goto done;
        }
    } else if ((res_span = (*m_graph)[u].schedule.reservations.find (jobid))
               != (*m_graph)[u].schedule.reservations.end ()) {
        span = res_span->second;
        // Can't be PARTIAL_CANCEL
        (*m_graph)[u].schedule.reservations.erase (res_span);
    } else {
        goto done;
    }

    plans = (*m_graph)[u].schedule.plans;
    if (mod_data.mod_type != job_modify_t::PARTIAL_CANCEL) {
        if (mod_data.mod_type == job_modify_t::VTX_CANCEL) {
            if ((prev_count = planner_span_resource_count (plans, span)) < 0) {
                m_err_msg += __FUNCTION__;
                m_err_msg += ": planner_span_resource_count failed.\n";
                m_err_msg += (*m_graph)[u].name + " " + strerror (errno) + ".\n";
                rc = -1;
                goto done;
            }
        }
        if ((rc = planner_rem_span (plans, span)) == -1) {
            m_err_msg += __FUNCTION__;
            m_err_msg += ": planner_rem_span returned -1.\n";
            m_err_msg += (*m_graph)[u].name + " " + strerror (errno) + ".\n";
            goto done;
        }
        // Accumulate counts per type to partially remove from filters
        if (mod_data.mod_type == job_modify_t::VTX_CANCEL) {
            mod_data.rank_to_counts[(*m_graph)[u].rank][(*m_graph)[u].type] += prev_count;
        }
    } else {  // PARTIAL_CANCEL
        m_err_msg += __FUNCTION__;
        m_err_msg += ": traverser tried to remove schedule and span";
        m_err_msg += " after vtx_cancel during partial cancel:\n";
        m_err_msg += (*m_graph)[u].name + " " + strerror (errno) + ".\n";
        rc = -1;
    }

done:
    return rc;
}

int dfu_impl_t::cancel_vertex (vtx_t vtx, modify_data_t &mod_data, int64_t jobid)
{
    int rc = -1;
    bool stop = false;
    subsystem_t dom = m_match->dom_subsystem ();

    if ((rc = mod_idata (vtx, jobid, dom, mod_data, stop)) == -1) {
        errno = EINVAL;
        return rc;
    }
    if ((rc = mod_plan (vtx, jobid, mod_data)) == -1)
        errno = EINVAL;
    // A PARTIAL_CANCEL visit only reduces the vertex's aggregate span;
    // the vertex still holds state for the job and stays indexed. Full
    // per-vertex removal (CANCEL/VTX_CANCEL) unindexes it -- but only
    // on success: a vertex whose purge failed must remain discoverable
    // so that a retry revisits it instead of silently succeeding.
    if (rc == 0 && mod_data.mod_type != job_modify_t::PARTIAL_CANCEL)
        m_graph_db->metadata.remove_job_vertex (jobid, vtx);

    return rc;
}

int dfu_impl_t::clear_vertex (vtx_t vtx, modify_data_t &mod_data)
{
    bool stop = false;
    subsystem_t dom = m_match->dom_subsystem ();
    int64_t base_time = 0;
    int64_t duration = 0;
    planner_t *plans = NULL;
    boost::optional<planner_multi_t *&> opt_multi_plans;

    // Compute removed span counts
    plans = (*m_graph)[vtx].schedule.plans;
    int64_t count = 0;
    int64_t total_count = 0;
    for (const auto &alloc_it : (*m_graph)[vtx].schedule.allocations) {
        if ((count = planner_span_resource_count (plans, alloc_it.second)) < 0) {
            m_err_msg += __FUNCTION__;
            m_err_msg += ": planner_span_resource_count failed.\n";
            m_err_msg += (*m_graph)[vtx].name + " " + strerror (errno) + ".\n";
            return -1;
        }
        total_count += count;
    }
    mod_data.rank_to_counts[(*m_graph)[vtx].rank][(*m_graph)[vtx].type] += total_count;
    // Reset planner
    base_time = planner_base_time (plans);
    duration = planner_duration (plans);
    if (planner_reset (plans, base_time, duration) != 0) {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": planner_reset failed.\n";
        m_err_msg += (*m_graph)[vtx].name + " " + strerror (errno) + ".\n";
        return -1;
    }
    // Reset planner_multi
    opt_multi_plans = (*m_graph)[vtx].idata.subplans.try_at (dom);
    if (opt_multi_plans && *opt_multi_plans) {
        base_time = planner_multi_base_time (*opt_multi_plans);
        duration = planner_multi_duration (*opt_multi_plans);
        if (planner_multi_reset (*opt_multi_plans, base_time, duration) != 0) {
            m_err_msg += __FUNCTION__;
            m_err_msg += ": planner_multi_reset failed.\n";
            m_err_msg += (*m_graph)[vtx].name + " " + strerror (errno) + ".\n";
            return -1;
        }
    }
    // Unindex the vertex from every job holding state on it (pair-wise:
    // each job's other vertices remain indexed). Enumerate all five
    // keyed containers rather than assuming tags accompany the rest.
    for (const auto &kv : (*m_graph)[vtx].idata.tags)
        m_graph_db->metadata.remove_job_vertex (kv.first, vtx);
    for (const auto &kv : (*m_graph)[vtx].idata.x_spans)
        m_graph_db->metadata.remove_job_vertex (kv.first, vtx);
    for (const auto &kv : (*m_graph)[vtx].idata.job2span)
        m_graph_db->metadata.remove_job_vertex (kv.first, vtx);
    for (const auto &kv : (*m_graph)[vtx].schedule.allocations)
        m_graph_db->metadata.remove_job_vertex (kv.first, vtx);
    for (const auto &kv : (*m_graph)[vtx].schedule.reservations)
        m_graph_db->metadata.remove_job_vertex (kv.first, vtx);
    // Clear tags, xspans, agfilters
    (*m_graph)[vtx].idata.tags.clear ();
    (*m_graph)[vtx].idata.x_spans.clear ();
    (*m_graph)[vtx].idata.job2span.clear ();
    // Clear allocations and reservations
    (*m_graph)[vtx].schedule.allocations.clear ();
    (*m_graph)[vtx].schedule.reservations.clear ();

    return 0;
}

int dfu_impl_t::get_subgraph_vertices (vtx_t vtx, std::set<vtx_t> &vtx_set)
{
    vtx_t next_vtx;
    subsystem_t dom = m_match->dom_subsystem ();
    f_out_edg_iterator_t ei, ei_end;

    (*m_graph)[vtx].idata.colors[dom] = m_color.gray ();
    for (tie (ei, ei_end) = out_edges (vtx, *m_graph); ei != ei_end; ++ei) {
        if ((*m_graph)[*ei].subsystem == dom) {
            next_vtx = target (*ei, *m_graph);
            vtx_set.insert (next_vtx);
            get_subgraph_vertices (next_vtx, vtx_set);
        }
    }
    (*m_graph)[vtx].idata.colors[dom] = m_color.black ();

    return 0;
}

int dfu_impl_t::get_parent_vtx (vtx_t vtx, vtx_t &parent_vtx)
{
    int rc = -1;
    vtx_t next_vtx = boost::graph_traits<resource_graph_t>::null_vertex ();
    boost::graph_traits<resource_graph_t>::in_edge_iterator ei, ei_end;
    subsystem_t dom = m_match->dom_subsystem ();

    boost::tie (ei, ei_end) = boost::in_edges (vtx, *m_graph);
    for (; ei != ei_end; ++ei) {
        next_vtx = boost::source (*ei, *m_graph);
        if ((*m_graph)[*ei].subsystem == dom) {
            parent_vtx = next_vtx;
            rc = 0;
            break;
        }
    }

    return rc;
}

int dfu_impl_t::remove_metadata_outedges (vtx_t source_vertex, vtx_t dest_vertex)
{
    std::vector<edg_t> remove_edges;
    auto iter = m_graph_db->metadata.by_outedges.find (source_vertex);
    if (iter == m_graph_db->metadata.by_outedges.end ())
        return -1;
    auto &outedges = iter->second;
    for (auto kv = outedges.begin (); kv != outedges.end (); ++kv) {
        if (boost::target (kv->second, *m_graph) == dest_vertex) {
            kv = outedges.erase (kv);
            // TODO: Consider adding break here
        }
    }

    return 0;
}

void dfu_impl_t::remove_graph_metadata (vtx_t v)
{
    m_graph_db->metadata.by_outedges.erase (v);
    for (auto &kv : (*m_graph)[v].paths) {
        m_graph_db->metadata.by_path.erase (kv.second);
    }
    auto &target_by_type = m_graph_db->metadata.by_type[(*m_graph)[v].type];
    for (auto it = target_by_type.begin (); it != target_by_type.end (); ++it) {
        if (*it == v) {
            target_by_type.erase (it);
            break;
        }
    }
    auto &target_by_name = m_graph_db->metadata.by_name[(*m_graph)[v].name];
    for (auto it = target_by_name.begin (); it != target_by_name.end (); ++it) {
        if (*it == v) {
            target_by_name.erase (it);
            break;
        }
    }
    auto &target_by_rank = m_graph_db->metadata.by_rank[(*m_graph)[v].rank];
    for (auto it = target_by_rank.begin (); it != target_by_rank.end (); ++it) {
        if (*it == v) {
            target_by_rank.erase (it);
            break;
        }
    }
    // Unindex the vertex from every job holding state on it so no
    // by_jobid entry points at an orphaned vertex (pair-wise: each
    // job's other vertices remain indexed). Enumerate all five keyed
    // containers rather than assuming tags accompany the rest.
    for (const auto &kv : (*m_graph)[v].idata.tags)
        m_graph_db->metadata.remove_job_vertex (kv.first, v);
    for (const auto &kv : (*m_graph)[v].idata.x_spans)
        m_graph_db->metadata.remove_job_vertex (kv.first, v);
    for (const auto &kv : (*m_graph)[v].idata.job2span)
        m_graph_db->metadata.remove_job_vertex (kv.first, v);
    for (const auto &kv : (*m_graph)[v].schedule.allocations)
        m_graph_db->metadata.remove_job_vertex (kv.first, v);
    for (const auto &kv : (*m_graph)[v].schedule.reservations)
        m_graph_db->metadata.remove_job_vertex (kv.first, v);
}

int dfu_impl_t::remove_subgraph (const std::vector<vtx_t> &roots, std::set<vtx_t> &vertices)
{
    for (const auto &root : roots) {
        vtx_t parent_vtx = boost::graph_traits<resource_graph_t>::null_vertex ();
        m_color.reset ();
        if (get_parent_vtx (root, parent_vtx) != 0)
            return -1;

        if (remove_metadata_outedges (parent_vtx, root) != 0)
            return -1;
    }
    for (auto &vtx : vertices) {
        // clear vertex edges but don't delete vertex
        boost::clear_vertex (vtx, *m_graph);
        remove_graph_metadata (vtx);
    }

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
// DFU Traverser Implementation Update API
////////////////////////////////////////////////////////////////////////////////

int dfu_impl_t::update (vtx_t root, std::shared_ptr<match_writers_t> &writers, jobmeta_t &jobmeta)
{
    int rc = -1;
    std::map<resource_type_t, int64_t> dfu;
    subsystem_t dom = m_match->dom_subsystem ();

    if (m_graph_db->metadata.v_rt_edges[dom].get_sequence_number () != m_sequence_number) {
        errno = EINVAL;
        m_err_msg += __FUNCTION__;
        m_err_msg += ": resource state wasn't properly set up for update.\n";
        return -1;
    }

    unsigned int excl = m_graph_db->metadata.v_rt_edges[dom].get_exclusive ();
    bool x = (excl == 0) ? false : true;
    unsigned int needs = m_graph_db->metadata.v_rt_edges[dom].get_needs ();
    m_color.reset ();

    bool emit_shadow = modify_traversal (root, false);
    // Regardless of value of `x`, value for `excl_parent` parameter starts as `false`
    if ((rc = upd_dfv (root, writers, needs, x, jobmeta, true, dfu, emit_shadow, false)) > 0) {
        int64_t starttime = jobmeta.at;
        int64_t endtime = jobmeta.at + jobmeta.duration;
        if (writers->emit_tm (starttime, endtime) == -1) {
            m_err_msg += __FUNCTION__;
            m_err_msg += ": emit_tm returned -1.\n";
        }
        if (jobmeta.is_queue_set ()) {
            if (writers->emit_attrs ("queue", jobmeta.get_queue ()) == -1) {
                m_err_msg += __FUNCTION__;
                m_err_msg += ": emit_attrs returned -1.\n";
            }
        }
    }

    return (rc > 0) ? 0 : -1;
}

int dfu_impl_t::update ()
{
    m_color.reset ();
    return 0;
}

int dfu_impl_t::update (vtx_t root,
                        std::shared_ptr<match_writers_t> &writers,
                        const std::string &str,
                        std::shared_ptr<resource_reader_base_t> &reader,
                        jobmeta_t &jobmeta)
{
    int rc = -1;
    bool x = false;
    unsigned int excl = 0;
    unsigned int needs = 0;
    std::map<resource_type_t, int64_t> dfu;
    subsystem_t dom = m_match->dom_subsystem ();
    bool rsv = (jobmeta.alloc_type == jobmeta_t::alloc_type_t::AT_ALLOC_ORELSE_RESERVE);

    tick ();
    if ((rc = reader->update (m_graph_db->resource_graph,
                              m_graph_db->metadata,
                              str,
                              jobmeta.jobid,
                              jobmeta.at,
                              jobmeta.duration,
                              rsv,
                              m_sequence_number))
        != 0) {
        m_err_msg += reader->err_message ();
        reader->clear_err_message ();
        return rc;
    }

    if (m_graph_db->metadata.v_rt_edges[dom].get_sequence_number () != m_sequence_number) {
        // This condition occurs when the subgraph came from a
        // traverser different from this traverser, for example,
        // a traverser whose dominant subsystem is different than this.
        return 0;
    }

    excl = m_graph_db->metadata.v_rt_edges[dom].get_exclusive ();
    x = (excl == 0) ? false : true;
    needs = static_cast<unsigned int> (m_graph_db->metadata.v_rt_edges[dom].get_needs ());
    m_color.reset ();
    bool emit_shadow = modify_traversal (root, false);
    // Regardless of value of `x`, value for `excl_parent` parameter starts as `false`
    if ((rc = upd_dfv (root, writers, needs, x, jobmeta, false, dfu, emit_shadow, false)) > 0) {
        int64_t starttime = jobmeta.at;
        int64_t endtime = jobmeta.at + jobmeta.duration;
        if (writers->emit_tm (starttime, endtime) == -1) {
            m_err_msg += __FUNCTION__;
            m_err_msg += ": emit_tm returned -1.\n";
        }
        if (jobmeta.is_queue_set ()) {
            if (writers->emit_attrs ("queue", jobmeta.get_queue ()) == -1) {
                m_err_msg += __FUNCTION__;
                m_err_msg += ": emit_attrs returned -1.\n";
            }
        }
    }

    return (rc > 0) ? 0 : -1;
}

int dfu_impl_t::remove (vtx_t root, int64_t jobid)
{
    int rc = 0;
    m_preorder = 0;
    m_postorder = 0;

    auto job_it = m_graph_db->metadata.by_jobid.find (jobid);
    if (job_it == m_graph_db->metadata.by_jobid.end ())
        // Removing an unknown (or already fully removed) job is
        // idempotent by design: callers issue final cancels with
        // noent_ok semantics.
        return 0;
    // Visit exactly the vertices holding the job's state -- no graph
    // traversal, no dependence on rank indexes or tag trails. Iterate
    // a snapshot of the descriptors: cancel_vertex () unindexes each
    // vertex from the live entry as its purge succeeds, so on failure
    // the entry retains exactly the vertices still holding state and a
    // retry revisits them.
    std::vector<vtx_t> vertices (job_it->second.begin (), job_it->second.end ());
    for (const vtx_t &vtx : vertices) {
        modify_data_t mod_data;
        mod_data.mod_type = job_modify_t::CANCEL;
        m_preorder++;
        if (cancel_vertex (vtx, mod_data, jobid) != 0) {
            m_err_msg += __FUNCTION__;
            m_err_msg += ": cancel_vertex failed on " + (*m_graph)[vtx].name + ".\n";
            rc = -1;
            continue;
        }
        m_postorder++;
    }
    return rc;
}

int dfu_impl_t::remove (vtx_t root,
                        const std::string &R_to_cancel,
                        std::shared_ptr<resource_reader_base_t> &reader,
                        int64_t jobid,
                        bool &full_cancel)
{
    int rc = -1;
    modify_data_t mod_data;
    resource_graph_t &g = m_graph_db->resource_graph;
    resource_graph_metadata_t &m = m_graph_db->metadata;
    subsystem_t dom = m_match->dom_subsystem ();
    m_preorder = 0;
    m_postorder = 0;

    tick ();
    mod_data.sequence_number = m_sequence_number;
    if (reader->partial_cancel (g, m, mod_data, R_to_cancel, jobid) != 0) {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": partial_cancel returned error.\n";
        return -1;
    }
    // A reader-driven cancel (e.g. JGF) visits and releases vertices
    // itself; fold its counts in so both reader paths report the same
    // statistics for an equivalent release
    m_preorder += mod_data.n_visited;
    m_postorder += mod_data.n_purged;

    // If rank_to_counts size is 0, reader was not JGF
    if (mod_data.rank_to_counts.size () == 0) {
        // Set modify type to be vertex cancel
        mod_data.mod_type = job_modify_t::VTX_CANCEL;
        for (const auto &rank : mod_data.ranks) {
            auto rank_vector = m.by_rank.find (rank);
            if (rank_vector == m.by_rank.end ()) {
                m_err_msg += __FUNCTION__;
                m_err_msg += std::to_string (rank) + " not found in by_rank map.\n";
                return -1;
            }
            uint32_t subgraph_root_len = std::numeric_limits<uint32_t>::max ();
            // Max characters 2^32 - 1 for a huge graph
            for (const vtx_t &vtx : rank_vector->second) {
                // If no job tag is found on the vertex, job must not have
                // allocated all rank resources
                if ((*m_graph)[vtx].idata.tags.find (jobid) != (*m_graph)[vtx].idata.tags.end ()) {
                    m_preorder++;
                    if ((rc = cancel_vertex (vtx, mod_data, jobid)) != 0) {
                        m_err_msg += __FUNCTION__;
                        m_err_msg += ": cancel_vertex failed\n.";
                        m_err_msg += (*m_graph)[vtx].name + ".\n";
                        return rc;
                    }
                    m_postorder++;
                }
                if ((*m_graph)[vtx].paths.at (dom).length () < subgraph_root_len) {
                    subgraph_root_len = (*m_graph)[vtx].paths.at (dom).length ();
                    mod_data.rank_to_root[rank] = vtx;
                }
            }
        }
    }

    if (mod_data.rank_to_root.size () == 0) {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": rank_to_root is empty.\n";
        return -1;
    }

    std::unordered_map<vtx_t, std::unordered_map<resource_type_t, int64_t>> parent_counts;
    for (const auto &rank_root : mod_data.rank_to_root) {
        const auto &rank_it = mod_data.rank_to_counts.find (rank_root.first);
        if (rank_it == mod_data.rank_to_counts.end ()) {
            m_err_msg += __FUNCTION__ + std::string (": ");
            m_err_msg += std::to_string (rank_root.first) + " not found in rank_to_counts.\n";
            return -1;
        }
        vtx_t subgraph_root_vtx = rank_root.second;
        // Accumulate type_to_count for all vertices up to graph root
        vtx_t parent_vtx = boost::graph_traits<resource_graph_t>::null_vertex ();
        vtx_t curr_vtx = subgraph_root_vtx;
        while (get_parent_vtx (curr_vtx, parent_vtx) == 0) {
            for (const auto &vtx_count : rank_it->second) {
                parent_counts[parent_vtx][vtx_count.first] += vtx_count.second;
            }
            curr_vtx = parent_vtx;
        }
    }

    // Reduce the ancestor aggregate filters by the freed counts.
    // Ancestors are visited (their aggregate spans reduced) but not
    // purged: they retain the job's state until the job is removed.
    for (const auto &v : parent_counts) {
        modify_data_t mod_data_new;
        mod_data_new.mod_type = job_modify_t::PARTIAL_CANCEL;
        mod_data_new.type_to_count = v.second;
        m_preorder++;
        if ((rc = cancel_vertex (v.first, mod_data_new, jobid)) != 0) {
            m_err_msg += __FUNCTION__;
            m_err_msg += ": cancel_vertex failed\n.";
            m_err_msg += (*m_graph)[v.first].name + ".\n";
            return rc;
        }
    }

    // Exact full-cancel semantics: the job is fully canceled when no
    // vertex holds any of its state. The walk above purged and
    // unindexed the freed ranked vertices, so what remains in by_jobid
    // is either real resource state (e.g. vertices whose rank is never
    // named in a freed R -- not a full cancel) or bookkeeping-only
    // residue on the ancestor chain (tags and exclusive-filter spans
    // whose aggregate spans have fully drained). Checking the whole
    // remainder costs O(|remainder|), so use the root's drained
    // aggregate span as a cheap trigger for the exact check.
    full_cancel = false;
    auto job_it = m_graph_db->metadata.by_jobid.find (jobid);
    if (job_it == m_graph_db->metadata.by_jobid.end ()) {
        full_cancel = true;
    } else if ((*m_graph)[root].idata.job2span.find (jobid)
               == (*m_graph)[root].idata.job2span.end ()) {
        bool residue_only = true;
        for (const vtx_t &vtx : job_it->second) {
            if ((*m_graph)[vtx].schedule.allocations.contains (jobid)
                || (*m_graph)[vtx].schedule.reservations.contains (jobid)
                || (*m_graph)[vtx].idata.job2span.contains (jobid)) {
                residue_only = false;
                break;
            }
        }
        if (residue_only) {
            bool purge_failed = false;
            // Snapshot as in remove (root, jobid): the live entry
            // shrinks as each vertex's purge succeeds and retains any
            // vertex whose purge fails.
            std::vector<vtx_t> vertices (job_it->second.begin (), job_it->second.end ());
            for (const vtx_t &vtx : vertices) {
                modify_data_t mod_data_new;
                mod_data_new.mod_type = job_modify_t::CANCEL;
                m_preorder++;
                if (cancel_vertex (vtx, mod_data_new, jobid) != 0) {
                    m_err_msg += __FUNCTION__;
                    m_err_msg += ": cancel_vertex failed on " + (*m_graph)[vtx].name + ".\n";
                    purge_failed = true;
                    rc = -1;
                    continue;
                }
                m_postorder++;
            }
            full_cancel = !purge_failed;
        }
    }

    return rc;
}

int dfu_impl_t::remove (vtx_t root, const std::set<int64_t> &ranks)
{
    int rc = -1;
    modify_data_t mod_data;
    resource_graph_t &g = m_graph_db->resource_graph;
    resource_graph_metadata_t &m = m_graph_db->metadata;
    m_preorder = 0;
    m_postorder = 0;
    std::unordered_set<int64_t> jobids;
    subsystem_t dom = m_match->dom_subsystem ();

    for (const int64_t &rank : ranks) {
        auto rank_vector = m.by_rank.find (rank);
        if (rank_vector == m.by_rank.end ()) {
            m_err_msg += __FUNCTION__ + std::string (": ");
            m_err_msg += std::to_string (rank) + " not found in by_rank map.\n";
            return -1;
        }
        mod_data.ranks.insert (rank);
        uint32_t subgraph_root_len = std::numeric_limits<uint32_t>::max ();
        // Max characters 2^32 - 1 for a huge graph
        for (const vtx_t &vtx : rank_vector->second) {
            for (const auto &jobid : (*m_graph)[vtx].idata.tags) {
                jobids.insert (jobid.first);
            }
            // Clear all job data from the vertex.
            if ((rc = clear_vertex (vtx, mod_data)) != 0) {
                m_err_msg += __FUNCTION__;
                m_err_msg += ": clear_vertex failed.\n";
                m_err_msg += (*m_graph)[vtx].name + ".\n";
                return rc;
            }
            if ((*m_graph)[vtx].paths.at (dom).length () < subgraph_root_len) {
                subgraph_root_len = (*m_graph)[vtx].paths.at (dom).length ();
                mod_data.rank_to_root[rank] = vtx;
            }
        }
    }
    if (jobids.size () == 0) {
        // Nothing to do
        return 0;
    }
    std::unordered_map<vtx_t,
                       std::unordered_map<int64_t, std::unordered_map<resource_type_t, int64_t>>>
        parent_jobid_counts;
    for (const auto &rank_root : mod_data.rank_to_root) {
        const auto &rank_it = mod_data.rank_to_counts.find (rank_root.first);
        if (rank_it == mod_data.rank_to_counts.end ()) {
            m_err_msg += __FUNCTION__ + std::string (": ");
            m_err_msg += std::to_string (rank_root.first) + " not found in rank_to_counts.\n";
            return -1;
        }
        vtx_t subgraph_root_vtx = rank_root.second;
        // Accumulate type_to_count for all vertices up to graph root
        vtx_t parent_vtx = boost::graph_traits<resource_graph_t>::null_vertex ();
        vtx_t curr_vtx = subgraph_root_vtx;
        while (get_parent_vtx (curr_vtx, parent_vtx) == 0) {
            for (const auto &vtx_count : rank_it->second) {
                for (const int64_t &jobid : jobids) {
                    parent_jobid_counts[parent_vtx][jobid][vtx_count.first] += vtx_count.second;
                }
            }
            curr_vtx = parent_vtx;
        }
    }
    // Now partial cancel DFV from graph root
    for (const auto &v : parent_jobid_counts) {
        for (const auto &jobid : v.second) {
            modify_data_t mod_data_new;
            mod_data_new.mod_type = job_modify_t::PARTIAL_CANCEL;
            mod_data_new.type_to_count = jobid.second;
            if ((rc = cancel_vertex (v.first, mod_data_new, jobid.first)) != 0) {
                m_err_msg += __FUNCTION__ + std::string (": ");
                m_err_msg += (*m_graph)[v.first].name + ": cancel_vertex failed.\n";
                return rc;
            }
        }
    }
    return rc;
}

int dfu_impl_t::mark (const std::string &root_path, resource_pool_t::status_t status)
{
    std::map<std::string, std::vector<vtx_t>>::const_iterator vit_root =
        m_graph_db->metadata.by_path.find (root_path);
    std::set<vtx_t> vtx_set;

    if (vit_root == m_graph_db->metadata.by_path.end ()) {
        errno = EINVAL;
        m_err_msg += __FUNCTION__;
        m_err_msg += ": could not find subtree path (" + root_path + ") in resource graph.\n";
        return -1;
    }
    for (auto &v : vit_root->second) {
        // Ensure node stats idempotence
        if ((*m_graph)[v].status != status) {
            (*m_graph)[v].status = status;
            vtx_set.insert (v);
            get_subgraph_vertices (v, vtx_set);
        }
    }
    for (const auto &v : vtx_set) {
        if ((*m_graph)[v].type == node_rt)
            m_graph_db->metadata.update_node_stats ((*m_graph)[v].size, status);
    }

    return 0;
}

int dfu_impl_t::mark (std::set<int64_t> &ranks, resource_pool_t::status_t status)
{
    try {
        std::map<int64_t, std::vector<vtx_t>>::iterator vit;
        std::string subtree_path = "", tmp_path = "";
        subsystem_t dom = m_match->dom_subsystem ();
        vtx_t subtree_root;

        int total = 0;
        for (auto &rank : ranks) {
            // Now iterate through subgraphs keyed by rank and
            // set status appropriately
            vit = m_graph_db->metadata.by_rank.find (rank);
            if (vit == m_graph_db->metadata.by_rank.end ())
                continue;

            subtree_root = vit->second.front ();
            subtree_path = (*m_graph)[subtree_root].paths.at (dom);
            for (vtx_t v : vit->second) {
                // The shortest path string is the subtree root.
                tmp_path = (*m_graph)[v].paths.at (dom);
                if (tmp_path.length () < subtree_path.length ()) {
                    subtree_path = tmp_path;
                    subtree_root = v;
                }
            }
            // Ensure node stats idempotence
            if ((*m_graph)[subtree_root].status != status) {
                (*m_graph)[subtree_root].status = status;
                ++total;
            }
        }
        m_graph_db->metadata.update_node_stats (total, status);
    } catch (std::out_of_range &) {
        errno = ENOENT;
        return -1;
    }
    return 0;
}

int dfu_impl_t::remove_subgraph (const std::set<int64_t> &ranks)
{
    vtx_t subgraph_root_vtx = boost::graph_traits<resource_graph_t>::null_vertex ();
    vtx_t rank_root_vtx = boost::graph_traits<resource_graph_t>::null_vertex ();
    std::set<vtx_t> vtx_set;
    std::vector<vtx_t> roots_list;
    std::string tmp_path = "";
    subsystem_t dom = m_match->dom_subsystem ();
    int str_len = INT_MAX;

    for (const auto &rank : ranks) {
        auto br_iter = m_graph_db->metadata.by_rank.find (rank);
        if (br_iter != m_graph_db->metadata.by_rank.end ()) {
            str_len = INT_MAX;
            vtx_t rank_root_vtx = boost::graph_traits<resource_graph_t>::null_vertex ();
            for (const auto &v : br_iter->second) {
                vtx_set.insert (v);
                tmp_path = (*m_graph)[v].paths.at (dom);
                if (tmp_path.length () < str_len) {
                    str_len = tmp_path.length ();
                    rank_root_vtx = v;
                }
            }
            roots_list.push_back (rank_root_vtx);
        }
    }

    if (remove_subgraph (roots_list, vtx_set) != 0)
        return -1;

    return 0;
}

int dfu_impl_t::remove_subgraph (const std::string &target)
{
    vtx_t subgraph_root_vtx = boost::graph_traits<resource_graph_t>::null_vertex ();
    std::set<vtx_t> vtx_set;
    std::vector<vtx_t> roots_list;

    auto iter = m_graph_db->metadata.by_path.find (target);
    if (iter == m_graph_db->metadata.by_path.end ()) {
        return -1;
    }
    for (const auto &v : iter->second) {
        subgraph_root_vtx = v;
    }
    vtx_set.insert (subgraph_root_vtx);
    roots_list.push_back (subgraph_root_vtx);
    get_subgraph_vertices (subgraph_root_vtx, vtx_set);

    if (remove_subgraph (roots_list, vtx_set) != 0)
        return -1;

    return 0;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

/*****************************************************************************\
 * Copyright 2014 Lawrence Livermore National Security, LLC
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

#include <cstdlib>
#include <cerrno>
#include "resource/traversers/dfu.hpp"
#include "resource/schema/perf_data.hpp"

using namespace Flux::resource_model;
using namespace Flux::resource_model::detail;
using namespace Flux::Jobspec;

// Global perf struct from schema
extern struct match_perf_t perf;

////////////////////////////////////////////////////////////////////////////////
// DFU Traverser Private API Definitions
////////////////////////////////////////////////////////////////////////////////

int dfu_traverser_t::is_satisfiable (Jobspec::Jobspec &jobspec,
                                     detail::jobmeta_t &meta,
                                     bool x,
                                     vtx_t root,
                                     std::unordered_map<resource_type_t, int64_t> &dfv)
{
    int rc = 0;
    std::vector<uint64_t> agg;
    int saved_errno = errno;
    subsystem_t dom = get_match_cb ()->dom_subsystem ();

    meta.alloc_type = jobmeta_t::alloc_type_t::AT_SATISFIABILITY;
    planner_multi_t *p = (*get_graph ())[root].idata.subplans.at (dom);
    meta.at = planner_multi_base_time (p) + planner_multi_duration (p) - meta.duration - 1;
    detail::dfu_impl_t::count_relevant_types (p, dfv, agg);
    errno = 0;
    if ((rc = detail::dfu_impl_t::select (jobspec, root, meta, x)) < 0) {
        rc = -1;
        errno = (!errno) ? ENODEV : errno;
        detail::dfu_impl_t::update ();
    }
    m_total_preorder = detail::dfu_impl_t::get_preorder_count ();
    m_total_postorder = detail::dfu_impl_t::get_postorder_count ();

    if (!errno)
        errno = saved_errno;
    return rc;
}

int dfu_traverser_t::request_feasible (detail::jobmeta_t const &meta,
                                       match_op_t op,
                                       vtx_t root,
                                       std::unordered_map<resource_type_t, int64_t> &dfv)
{
    if (op == match_op_t::MATCH_UNKNOWN)
        return 0;

    const auto target_nodes = dfv[node_rt];
    const bool checking_satisfiability =
        op == match_op_t::MATCH_ALLOCATE_W_SATISFIABILITY || op == match_op_t::MATCH_SATISFIABILITY;

    if ((!meta.constraint) && (target_nodes <= get_graph_db ()->metadata.nodes_up))
        return 0;

    // check if there are enough nodes up at all
    if (target_nodes > get_graph_db ()->metadata.nodes_up) {
        if (op == match_op_t::MATCH_ALLOCATE_ORELSE_RESERVE || op == match_op_t::MATCH_ALLOCATE) {
            errno = EBUSY;
            return -1;
        }
        if (checking_satisfiability) {
            // if we're checking satisfiability, only return here if
            // it's actually unsatisfiable
            errno = ENODEV;
            return -1;
        }
    }

    const auto by_type_iter = get_graph_db ()->metadata.by_type.find (node_rt);
    if (by_type_iter == get_graph_db ()->metadata.by_type.end ())
        return 0;
    auto &all_nodes = by_type_iter->second;

    auto &g = *get_graph ();
    const int64_t graph_end =
        std::chrono::duration_cast<std::chrono::seconds> (
            get_graph_db ()->metadata.graph_duration.graph_end.time_since_epoch ())
            .count ();
    // only the initial time matters for allocate
    const int64_t target_time = op == match_op_t::MATCH_ALLOCATE ? meta.at : graph_end - 1;
    int feasible_nodes = 0;
    for (auto const &node_vtx : all_nodes) {
        auto const &node = g[node_vtx];
        // if it matches the constraints
        if ((!meta.constraint || meta.constraint->match (node))
            // if it's up and not drained
            && (checking_satisfiability || node.status == resource_pool_t::status_t::UP)
            // if it's available
            && planner_avail_resources_during (node.schedule.plans, target_time, meta.duration)
                   == 1) {
            ++feasible_nodes;
            if (feasible_nodes >= target_nodes) {
                break;
            }
        }
    }
    if (feasible_nodes < target_nodes) {
        // no chance, don't even try
        if (op == match_op_t::MATCH_ALLOCATE_ORELSE_RESERVE || op == match_op_t::MATCH_ALLOCATE) {
            errno = EBUSY;
            return -1;
        }
        if (checking_satisfiability) {
            // if we're checking satisfyability, only return here if
            // it's actually unsatisfiable
            errno = ENODEV;
            return -1;
        }
    }

    return 0;
}

int dfu_traverser_t::schedule (Jobspec::Jobspec &jobspec,
                               detail::jobmeta_t &meta,
                               bool x,
                               match_op_t op,
                               vtx_t root,
                               std::unordered_map<resource_type_t, int64_t> &dfv)
{
    int64_t t = 0;
    int64_t sched_iters = 1;  // Track the schedule iterations in perf stats
    int rc = -1;
    size_t len = 0;
    std::vector<uint64_t> agg;
    uint64_t duration = 0;
    int saved_errno = errno;
    planner_multi_t *p = NULL;
    subsystem_t dom = get_match_cb ()->dom_subsystem ();

    // precheck to see if enough resources are available for this to be feasible
    if ((rc = request_feasible (meta, op, root, dfv)) < 0)
        goto out;

    if ((rc = detail::dfu_impl_t::select (jobspec, root, meta, x)) == 0) {
        m_total_preorder = detail::dfu_impl_t::get_preorder_count ();
        m_total_postorder = detail::dfu_impl_t::get_postorder_count ();
        goto out;
    }

    /* Currently no resources/devices available... Do more... */
    switch (op) {
        case match_op_t::MATCH_ALLOCATE_W_SATISFIABILITY: {
            /* With satisfiability check */
            errno = EBUSY;
            meta.alloc_type = jobmeta_t::alloc_type_t::AT_SATISFIABILITY;
            p = (*get_graph ())[root].idata.subplans.at (dom);
            meta.at = planner_multi_base_time (p) + planner_multi_duration (p) - meta.duration - 1;
            detail::dfu_impl_t::count_relevant_types (p, dfv, agg);
            if (detail::dfu_impl_t::select (jobspec, root, meta, x) < 0) {
                errno = (errno == EBUSY) ? ENODEV : errno;
                detail::dfu_impl_t::update ();
            }
            m_total_preorder += detail::dfu_impl_t::get_preorder_count ();
            m_total_postorder += detail::dfu_impl_t::get_postorder_count ();
            // increment match traversal loop count
            ++sched_iters;
            break;
        }
        case match_op_t::MATCH_ALLOCATE_ORELSE_RESERVE: {
            /* Or else reserve */
            errno = 0;
            meta.alloc_type = jobmeta_t::alloc_type_t::AT_ALLOC_ORELSE_RESERVE;
            t = meta.at + 1;
            p = (*get_graph ())[root].idata.subplans.at (dom);
            len = planner_multi_resources_len (p);
            duration = meta.duration;
            detail::dfu_impl_t::count_relevant_types (p, dfv, agg);
            for (t = planner_multi_avail_time_first (p, t, duration, agg.data (), len);
                 (t != -1 && rc && !errno);
                 t = planner_multi_avail_time_next (p)) {
                meta.at = t;
                rc = detail::dfu_impl_t::select (jobspec, root, meta, x);
                m_total_preorder += detail::dfu_impl_t::get_preorder_count ();
                m_total_postorder += detail::dfu_impl_t::get_postorder_count ();
                // increment match traversal loop count
                ++sched_iters;
            }
            // The planner layer returns
            //     ENOENT when no scheduleable point exists
            //     ERANGE when the total available core count at the root < the request
            if (rc < 0 && (errno == ENOENT || errno == ERANGE)) {
                errno = EBUSY;
                meta.alloc_type = jobmeta_t::alloc_type_t::AT_SATISFIABILITY;
                meta.at = planner_multi_base_time (p) + planner_multi_duration (p) - duration - 1;
                if (detail::dfu_impl_t::select (jobspec, root, meta, x) < 0) {
                    errno = (errno == EBUSY) ? ENODEV : errno;
                    detail::dfu_impl_t::update ();
                }
                m_total_preorder += detail::dfu_impl_t::get_preorder_count ();
                m_total_postorder += detail::dfu_impl_t::get_postorder_count ();
                // increment match traversal loop count
                ++sched_iters;
            }
            break;
        }
        case match_op_t::MATCH_ALLOCATE:
            errno = EBUSY;
            break;
        default:
            break;
    }

out:
    errno = (!errno) ? saved_errno : errno;
    // Update the perf temporary iteration count. If this schedule invocation
    // corresponds to the max match time this value will be output in the
    // stats RPC.
    perf.tmp_iter_count = sched_iters;
    return rc;
}

////////////////////////////////////////////////////////////////////////////////
// DFU Traverser Public API Definitions
////////////////////////////////////////////////////////////////////////////////

dfu_traverser_t::dfu_traverser_t ()
{
}

dfu_traverser_t::dfu_traverser_t (std::shared_ptr<resource_graph_db_t> db,
                                  std::shared_ptr<dfu_match_cb_t> m)
    : detail::dfu_impl_t (db, m)
{
}

dfu_traverser_t::dfu_traverser_t (const dfu_traverser_t &o) : detail::dfu_impl_t (o)
{
}

dfu_traverser_t &dfu_traverser_t::operator= (const dfu_traverser_t &o)
{
    detail::dfu_impl_t::operator= (o);
    return *this;
}

dfu_traverser_t::~dfu_traverser_t ()
{
}

const resource_graph_t *dfu_traverser_t::get_graph () const
{
    return detail::dfu_impl_t::get_graph ();
}

const std::shared_ptr<const resource_graph_db_t> dfu_traverser_t::get_graph_db () const
{
    return detail::dfu_impl_t::get_graph_db ();
}

const std::shared_ptr<const dfu_match_cb_t> dfu_traverser_t::get_match_cb () const
{
    return detail::dfu_impl_t::get_match_cb ();
}

const std::string &dfu_traverser_t::err_message () const
{
    return detail::dfu_impl_t::err_message ();
}

const unsigned int dfu_traverser_t::get_total_preorder_count () const
{
    return m_total_preorder;
}

const unsigned int dfu_traverser_t::get_total_postorder_count () const
{
    return m_total_postorder;
}

void dfu_traverser_t::set_graph_db (std::shared_ptr<resource_graph_db_t> g)
{
    detail::dfu_impl_t::set_graph_db (g);
}

void dfu_traverser_t::set_match_cb (std::shared_ptr<dfu_match_cb_t> m)
{
    detail::dfu_impl_t::set_match_cb (m);
}

void dfu_traverser_t::clear_err_message ()
{
    detail::dfu_impl_t::clear_err_message ();
}

bool dfu_traverser_t::is_initialized () const
{
    return m_initialized;
}

int dfu_traverser_t::initialize ()
{
    // Clear the error message to disambiguate errors
    clear_err_message ();

    int rc = 0;
    vtx_t root;
    if (!get_graph () || !get_graph_db () || !get_match_cb ()) {
        errno = EINVAL;
        return -1;
    }

    m_initialized = false;
    detail::dfu_impl_t::reset_color ();
    for (auto &subsystem : get_match_cb ()->subsystems ()) {
        std::map<resource_type_t, int64_t> from_dfv;
        if (get_graph_db ()->metadata.roots.find (subsystem)
            == get_graph_db ()->metadata.roots.end ()) {
            errno = ENOTSUP;
            rc = -1;
            break;
        }
        root = get_graph_db ()->metadata.roots.at (subsystem);
        rc += detail::dfu_impl_t::prime_pruning_filter (subsystem, root, from_dfv);
    }
    m_initialized = (rc == 0) ? true : false;
    return rc;
}

int dfu_traverser_t::initialize (std::shared_ptr<resource_graph_db_t> db,
                                 std::shared_ptr<dfu_match_cb_t> m)
{
    set_graph_db (db);
    set_match_cb (m);
    return initialize ();
}

int dfu_traverser_t::run (Jobspec::Jobspec &jobspec,
                          std::shared_ptr<match_writers_t> &writers,
                          match_op_t op,
                          int64_t jobid,
                          int64_t *at)
{
    // Clear the error message to disambiguate errors
    clear_err_message ();

    subsystem_t dom = get_match_cb ()->dom_subsystem ();
    graph_duration_t graph_duration = get_graph_db ()->metadata.graph_duration;
    if (!get_graph () || !get_graph_db ()
        || (get_graph_db ()->metadata.roots.find (dom) == get_graph_db ()->metadata.roots.end ())
        || !get_match_cb () || jobspec.resources.empty ()) {
        errno = EINVAL;
        return -1;
    }

    int rc = -1;
    int64_t graph_end = std::chrono::duration_cast<std::chrono::seconds> (
                            graph_duration.graph_end.time_since_epoch ())
                            .count ();
    detail::jobmeta_t meta;
    vtx_t root = get_graph_db ()->metadata.roots.at (dom);
    bool x = detail::dfu_impl_t::exclusivity (jobspec.resources, root);
    const auto exclusive_types = detail::dfu_impl_t::get_exclusive_resource_types ();
    std::unordered_map<resource_type_t, int64_t> dfv;

    detail::dfu_impl_t::prime_jobspec (jobspec.resources, dfv);
    if (meta.build (jobspec, detail::jobmeta_t::alloc_type_t::AT_ALLOC, jobid, *at, graph_duration)
        < 0)
        return -1;

    if ((op == match_op_t::MATCH_SATISFIABILITY)
        && (rc = is_satisfiable (jobspec, meta, x, root, dfv)) == 0) {
        detail::dfu_impl_t::update ();
    } else if ((rc = schedule (jobspec, meta, x, op, root, dfv)) == 0) {
        *at = meta.at;
        if (*at == graph_end) {
            detail::dfu_impl_t::reset_exclusive_resource_types (exclusive_types);
            // no schedulable point found even at the end of the time, return EBUSY
            errno = EBUSY;
            return -1;
        }
        if (*at < 0 or *at > graph_end) {
            detail::dfu_impl_t::reset_exclusive_resource_types (exclusive_types);
            errno = EINVAL;
            return -1;
        }
        // If job ends after the resource graph expires, reduce the duration
        // so it coincides with the graph expiration. Note that we could
        // arguably return ENOTSUP/EINVAL instead. Also note that we
        // know *at < graph_end from the previous check, and meta.duration
        // is < int64_t max from meta.build.
        if ((*at + static_cast<int64_t> (meta.duration)) > graph_end)
            meta.duration = graph_end - *at;
        // returns 0 or -1
        rc = detail::dfu_impl_t::update (root, writers, meta);
    }
    // returns 0 or -1
    rc += detail::dfu_impl_t::reset_exclusive_resource_types (exclusive_types);

    return rc;
}

int dfu_traverser_t::run (const std::string &str,
                          std::shared_ptr<match_writers_t> &writers,
                          std::shared_ptr<resource_reader_base_t> &reader,
                          int64_t id,
                          int64_t at,
                          uint64_t duration)
{
    // Clear the error message to disambiguate errors
    clear_err_message ();

    if (!get_match_cb () || !get_graph () || !get_graph_db () || !reader || at < 0) {
        errno = EINVAL;
        return -1;
    }

    subsystem_t dom = get_match_cb ()->dom_subsystem ();
    if (get_graph_db ()->metadata.roots.find (dom) == get_graph_db ()->metadata.roots.end ()) {
        errno = EINVAL;
        return -1;
    }

    vtx_t root = get_graph_db ()->metadata.roots.at (dom);
    detail::jobmeta_t meta;
    meta.jobid = id;
    meta.at = at;
    meta.duration = duration;

    return detail::dfu_impl_t::update (root, writers, str, reader, meta);
}

int dfu_traverser_t::find (std::shared_ptr<match_writers_t> &writers, const std::string &criteria)
{
    // Clear the error message to disambiguate errors
    clear_err_message ();
    return detail::dfu_impl_t::find (writers, criteria);
}

int dfu_traverser_t::remove (int64_t jobid)
{
    int rc = 0;
    // Clear the error message to disambiguate errors
    clear_err_message ();

    subsystem_t dom = get_match_cb ()->dom_subsystem ();
    if (!get_graph () || !get_graph_db ()
        || get_graph_db ()->metadata.roots.find (dom) == get_graph_db ()->metadata.roots.end ()
        || !get_match_cb ()) {
        errno = EINVAL;
        return -1;
    }

    vtx_t root = get_graph_db ()->metadata.roots.at (dom);

    rc = detail::dfu_impl_t::remove (root, jobid);
    m_total_preorder = detail::dfu_impl_t::get_preorder_count ();
    m_total_postorder = detail::dfu_impl_t::get_postorder_count ();
    return rc;
}

int dfu_traverser_t::remove (const std::string &R_to_cancel,
                             std::shared_ptr<resource_reader_base_t> &reader,
                             int64_t jobid,
                             bool &full_cancel)
{
    int rc = 0;
    // Clear the error message to disambiguate errors
    clear_err_message ();

    subsystem_t dom = get_match_cb ()->dom_subsystem ();
    if (!get_graph () || !get_graph_db ()
        || get_graph_db ()->metadata.roots.find (dom) == get_graph_db ()->metadata.roots.end ()
        || !get_match_cb ()) {
        errno = EINVAL;
        return -1;
    }

    vtx_t root = get_graph_db ()->metadata.roots.at (dom);
    rc = detail::dfu_impl_t::remove (root, R_to_cancel, reader, jobid, full_cancel);
    m_total_preorder = detail::dfu_impl_t::get_preorder_count ();
    m_total_postorder = detail::dfu_impl_t::get_postorder_count ();
    return rc;
}

int dfu_traverser_t::mark (const std::string &root_path, resource_pool_t::status_t status)
{
    // Clear the error message to disambiguate errors
    clear_err_message ();
    return detail::dfu_impl_t::mark (root_path, status);
}

int dfu_traverser_t::mark (std::set<int64_t> &ranks, resource_pool_t::status_t status)
{
    // Clear the error message to disambiguate errors
    clear_err_message ();
    return detail::dfu_impl_t::mark (ranks, status);
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

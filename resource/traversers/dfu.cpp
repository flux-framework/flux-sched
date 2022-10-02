/*****************************************************************************\
 * Copyright 2014 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#include <iostream>
#include <cstdlib>
#include <cerrno>
#include "resource/traversers/dfu.hpp"

extern "C" {
#if HAVE_CONFIG_H
#include "config.h"
#endif
}

using namespace Flux::resource_model;
using namespace Flux::resource_model::detail;
using namespace Flux::Jobspec;

/****************************************************************************
 *                                                                          *
 *                    DFU Traverser Private API Definitions                 *
 *                                                                          *
 ****************************************************************************/

int dfu_traverser_t::is_satisfiable (Jobspec::Jobspec &jobspec,
                                     detail::jobmeta_t &meta, bool x,
                                     vtx_t root,
                                     std::unordered_map<std::string,
                                                        int64_t> &dfv)
{
    int rc = 0;
    std::vector<uint64_t> agg;
    int saved_errno = errno;
    const subsystem_t &dom = get_match_cb ()->dom_subsystem ();

    meta.alloc_type = jobmeta_t::alloc_type_t::AT_SATISFIABILITY;
    planner_multi_t *p = (*get_graph ())[root].idata.subplans.at (dom);
    meta.at = planner_multi_base_time (p)
              + planner_multi_duration (p) - meta.duration - 1;
    detail::dfu_impl_t::count_relevant_types (p, dfv, agg);
    errno = 0;
    if ( (rc = detail::dfu_impl_t::select (jobspec, root, meta, x)) < 0) {
        rc = -1;
        errno = (!errno)? ENODEV : errno;
        detail::dfu_impl_t::update ();
    }
    m_total_preorder = detail::dfu_impl_t::get_preorder_count ();
    m_total_postorder = detail::dfu_impl_t::get_postorder_count ();

    if (!errno)
        errno = saved_errno;
    return rc;
}

int dfu_traverser_t::schedule (Jobspec::Jobspec &jobspec,
                               detail::jobmeta_t &meta, bool x, match_op_t op,
                               vtx_t root,
                               std::unordered_map<std::string, int64_t> &dfv)
{
    int64_t t = 0;
    int rc = -1;
    size_t len = 0;
    std::vector<uint64_t> agg;
    uint64_t duration = 0;
    int saved_errno = errno;
    planner_multi_t *p = NULL;
    const subsystem_t &dom = get_match_cb ()->dom_subsystem ();

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
        meta.at = planner_multi_base_time (p)
                  + planner_multi_duration (p) - meta.duration - 1;
        detail::dfu_impl_t::count_relevant_types (p, dfv, agg);
        if (detail::dfu_impl_t::select (jobspec, root, meta, x) < 0) {
            errno = (errno == EBUSY)? ENODEV : errno;
            detail::dfu_impl_t::update ();
        }
        m_total_preorder += detail::dfu_impl_t::get_preorder_count ();
        m_total_postorder += detail::dfu_impl_t::get_postorder_count ();
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
        for (t = planner_multi_avail_time_first (p, t, duration, &agg[0], len);
             (t != -1 && rc && !errno); t = planner_multi_avail_time_next (p)) {
            meta.at = t;
            rc = detail::dfu_impl_t::select (jobspec, root, meta, x);
            m_total_preorder += detail::dfu_impl_t::get_preorder_count ();
            m_total_postorder += detail::dfu_impl_t::get_postorder_count ();
        }
        // The planner layer returns
        //     ENOENT when no scheduleable point exists
        //     ERANGE when the total available core count at the root < the request
        if (rc < 0 && (errno == ENOENT || errno == ERANGE)) {
            errno = EBUSY;
            meta.alloc_type = jobmeta_t::alloc_type_t::AT_SATISFIABILITY;
            meta.at = planner_multi_base_time (p)
                      + planner_multi_duration (p) - duration - 1;
            if (detail::dfu_impl_t::select (jobspec, root, meta, x) < 0) {
                errno = (errno == EBUSY)? ENODEV : errno;
                detail::dfu_impl_t::update ();
            }
            m_total_preorder += detail::dfu_impl_t::get_preorder_count ();
            m_total_postorder += detail::dfu_impl_t::get_postorder_count ();
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
    errno = (!errno)? saved_errno : errno;
    return rc;
}


/****************************************************************************
 *                                                                          *
 *                    DFU Traverser Public API Definitions                  *
 *                                                                          *
 ****************************************************************************/

dfu_traverser_t::dfu_traverser_t ()
{

}

dfu_traverser_t::dfu_traverser_t (std::shared_ptr<f_resource_graph_t> g,
                                  std::shared_ptr<resource_graph_db_t> db,
                                  std::shared_ptr<dfu_match_cb_t> m)
    : detail::dfu_impl_t (g, db, m)
{

}

dfu_traverser_t::dfu_traverser_t (const dfu_traverser_t &o)
    : detail::dfu_impl_t (o)
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

const std::shared_ptr<const f_resource_graph_t> dfu_traverser_t::
                                                    get_graph () const
{
   return detail::dfu_impl_t::get_graph ();
}

const std::shared_ptr<const resource_graph_db_t> dfu_traverser_t::
                                                     get_graph_db () const
{
    return detail::dfu_impl_t::get_graph_db ();
}

const std::shared_ptr<const dfu_match_cb_t> dfu_traverser_t::
                                                get_match_cb () const
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

void dfu_traverser_t::set_graph (std::shared_ptr<f_resource_graph_t> g)
{
    detail::dfu_impl_t::set_graph (g);
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
    int rc = 0;
    vtx_t root;
    if (!get_graph () || !get_graph_db () || !get_match_cb ()) {
        errno = EINVAL;
        return -1;
    }

    m_initialized = false;
    detail::dfu_impl_t::reset_color ();
    for (auto &subsystem : get_match_cb ()->subsystems ()) {
        std::map<std::string, int64_t> from_dfv;
        if (get_graph_db ()->metadata.roots.find (subsystem)
            == get_graph_db ()->metadata.roots.end ()) {
            errno = ENOTSUP;
            rc = -1;
            break;
        }
        root = get_graph_db ()->metadata.roots.at(subsystem);
        rc += detail::dfu_impl_t::prime_pruning_filter (subsystem,
                                                        root, from_dfv);
    }
    m_initialized = (rc == 0)? true : false;
    return rc;
}

int dfu_traverser_t::initialize (std::shared_ptr<f_resource_graph_t> g,
                                 std::shared_ptr<resource_graph_db_t> db,
                                 std::shared_ptr<dfu_match_cb_t> m)
{
    set_graph (g);
    set_graph_db (db);
    set_match_cb (m);
    return initialize ();
}

int dfu_traverser_t::run (Jobspec::Jobspec &jobspec,
                          std::shared_ptr<match_writers_t> &writers,
                          match_op_t op, int64_t jobid, int64_t *at)
{
    const subsystem_t &dom = get_match_cb ()->dom_subsystem ();
    graph_duration_t graph_duration =
                                      get_graph_db ()->metadata.graph_duration;
    if (!get_graph () || !get_graph_db ()
        || (get_graph_db ()->metadata.roots.find (dom)
            == get_graph_db ()->metadata.roots.end ())
        || !get_match_cb () || jobspec.resources.empty ()) {
        errno = EINVAL;
        return -1;
    }

    int rc = -1;
    int64_t graph_end = graph_duration.graph_end.time_since_epoch ().count ();
    detail::jobmeta_t meta;
    vtx_t root = get_graph_db ()->metadata.roots.at (dom);
    bool x = detail::dfu_impl_t::exclusivity (jobspec.resources, root);
    const std::set<std::string> exclusive_types =
                        detail::dfu_impl_t::get_exclusive_resource_types ();
    std::unordered_map<std::string, int64_t> dfv;

    detail::dfu_impl_t::prime_jobspec (jobspec.resources, dfv);
    if (meta.build (jobspec, detail::jobmeta_t::alloc_type_t::AT_ALLOC, jobid,
                    *at, graph_duration) < 0)
        return -1;

    if ( (op == match_op_t::MATCH_SATISFIABILITY)
        && (rc = is_satisfiable (jobspec, meta, x, root, dfv)) == 0) {
        detail::dfu_impl_t::update ();
    } else if ( (rc = schedule (jobspec, meta, x, op, root, dfv)) ==  0) {
        *at = meta.at;
        if (*at < 0 or *at >= graph_end) {
            detail::dfu_impl_t::reset_exclusive_resource_types
                                                        (exclusive_types);
            errno = EINVAL;
            return -1;
        }
        // If job ends after the resource graph expires, reduce the duration
        // so it coincides with the graph expiration. Note that we could
        // arguably return ENOTSUP/EINVAL instead. Also note that we 
        // know *at < graph_end from the previous check, and meta.duration 
        // is < int64_t max from meta.build.
        if ( (*at + static_cast<int64_t> (meta.duration)) > graph_end)
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
                          int64_t id, int64_t at, uint64_t duration)
{
    if (!get_match_cb () || !get_graph ()
        || !get_graph_db () || !reader || at < 0) {
        errno = EINVAL;
        return -1;
    }

    const subsystem_t &dom = get_match_cb ()->dom_subsystem ();
    if (get_graph_db ()->metadata.roots.find (dom)
        == get_graph_db ()->metadata.roots.end ()) {
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

int dfu_traverser_t::find (std::shared_ptr<match_writers_t> &writers,
                           const std::string &criteria)
{
    return detail::dfu_impl_t::find (writers, criteria);
}

int dfu_traverser_t::remove (int64_t jobid)
{
    const subsystem_t &dom = get_match_cb ()->dom_subsystem ();
    if (!get_graph () || !get_graph_db ()
        || get_graph_db ()->metadata.roots.find (dom)
           == get_graph_db ()->metadata.roots.end ()
        || !get_match_cb ()) {
        errno = EINVAL;
        return -1;
    }

    vtx_t root = get_graph_db ()->metadata.roots.at (dom);
    return detail::dfu_impl_t::remove (root, jobid);
}

int dfu_traverser_t::mark (const std::string &root_path, 
                           resource_pool_t::status_t status)
{
    return detail::dfu_impl_t::mark (root_path, status);
}

int dfu_traverser_t::mark (std::set<int64_t> &ranks, 
                           resource_pool_t::status_t status)
{
    return detail::dfu_impl_t::mark (ranks, status);
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

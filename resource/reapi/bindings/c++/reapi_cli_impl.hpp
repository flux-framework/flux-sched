/*****************************************************************************\
 * Copyright 2019 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef REAPI_CLI_IMPL_HPP
#define REAPI_CLI_IMPL_HPP

extern "C" {
#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <flux/core.h>
}

#include <stdexcept>
#include <jansson.h>
#include <boost/algorithm/string.hpp>
#include "resource/reapi/bindings/c++/reapi_cli.hpp"
#include "resource/readers/resource_reader_factory.hpp"

namespace Flux {
namespace resource_model {
namespace detail {

const int NOT_YET_IMPLEMENTED = -1;

static double get_elapsed_time (timeval &st, timeval &et)
{
    double ts1 = (double)st.tv_sec + (double)st.tv_usec/1000000.0f;
    double ts2 = (double)et.tv_sec + (double)et.tv_usec/1000000.0f;
    return ts2 - ts1;
}

/****************************************************************************
 *                                                                          *
 *               REAPI CLI Class Private Definitions                        *
 *                                                                          *
 ****************************************************************************/

std::string reapi_cli_t::m_err_msg = "";

/****************************************************************************
 *                                                                          *
 *            REAPI CLI Class Public API Definitions                        *
 *                                                                          *
 ****************************************************************************/

int reapi_cli_t::match_allocate (void *h, bool orelse_reserve,
                                 const std::string &jobspec,
                                 const uint64_t jobid, bool &reserved,
                                 std::string &R, int64_t &at, double &ov)
{
    resource_query_t *rq = static_cast<resource_query_t *> (h);
    int rc = -1;
    at = 0;
    ov = 0.0f;
    job_lifecycle_t st;
    std::shared_ptr<job_info_t> job_info = nullptr;
    struct timeval start_time, end_time;
    std::stringstream o;

    try {
        Flux::Jobspec::Jobspec job {jobspec};

        if ( (rc = gettimeofday (&start_time, NULL)) < 0) {
            m_err_msg += __FUNCTION__;
            m_err_msg += ": ERROR: gettimeofday: "
                          + std::string (strerror (errno)) + "\n";
            goto out;
        }

        if (orelse_reserve)
            rc = rq->traverser_run (job,
                                match_op_t::MATCH_ALLOCATE_ORELSE_RESERVE,
                                (int64_t)jobid, at);
        else
            rc = rq->traverser_run (job, match_op_t::MATCH_ALLOCATE, 
                                    (int64_t)jobid, at);

        if (rq->get_traverser_err_msg () != "") {
            m_err_msg += __FUNCTION__;
            m_err_msg += ": ERROR: " + rq->get_traverser_err_msg () + "\n";
            rq->clear_traverser_err_msg ();
            rc = -1;
            goto out;
        }
    } 
    catch (Flux::Jobspec::parse_error &e) {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": ERROR: Jobspec error for "
                      + std::to_string (rq->get_job_counter ())
                      + ": " + std::string (e.what ()) + "\n";
        rc = -1;
        goto out;
    }
    
    if ( (rc = rq->writers->emit (o)) < 0) {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": ERROR: match writer emit: "
                      + std::string (strerror (errno)) + "\n";
        goto out;
    }

    R = o.str ();


    reserved = (at != 0)? true : false; 
    st = (reserved)? 
                job_lifecycle_t::RESERVED : job_lifecycle_t::ALLOCATED; 
    if (reserved) 
        rq->set_reservation (jobid); 
    else
        rq->set_allocation (jobid);
 
    job_info = std::make_shared<job_info_t> (jobid, st, at, "", "", ov); 
    if (job_info == nullptr) {
        errno = ENOMEM; 
        m_err_msg += __FUNCTION__; 
        m_err_msg += ": ERROR: can't allocate memory: " 
                     + std::string (strerror (errno))+ "\n"; 
        rc = -1; 
        goto out; 
    } 
 
    rq->set_job (jobid, job_info); 
    rq->incr_job_counter ();
                         
    if ( (rc = gettimeofday (&end_time, NULL)) < 0) {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": ERROR: gettimeofday: "
                      + std::string (strerror (errno)) + "\n";
        goto out;
    }

    ov = get_elapsed_time (start_time, end_time);

out:
    return rc;
}

int reapi_cli_t::update_allocate (void *h, const uint64_t jobid,
                                  const std::string &R, int64_t &at, double &ov,
                                  std::string &R_out)
{
    return NOT_YET_IMPLEMENTED;
}

int reapi_cli_t::match_allocate_multi (void *h, bool orelse_reserve,
                                       const char *jobs,
                                       queue_adapter_base_t *adapter)
{
    return NOT_YET_IMPLEMENTED;
}

int reapi_cli_t::cancel (void *h, const uint64_t jobid, bool noent_ok)
{
    resource_query_t *rq = static_cast<resource_query_t *> (h);
    int rc = -1;

    if (rq->allocation_exists (jobid)) {
        if ( (rc = rq->remove_job (jobid)) == 0)
            rq->erase_allocation (jobid);
    } else if (rq->reservation_exists (jobid)) {
        if ( (rc = rq->remove_job (jobid)) == 0)
            rq->erase_reservation (jobid);
    } else {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": ERROR: nonexistent job "
                      + std::to_string (jobid) + "\n";
        goto out;
    }

    if (rc != 0) {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": ERROR: error encountered while removing job "
                      + std::to_string (jobid) + "\n";
    }

out:
    return rc;
}

int reapi_cli_t::find (void *h, std::string criteria,
                       json_t *&o )
{
    int rc = -1;
    resource_query_t *rq = static_cast<resource_query_t *> (h);
    
    if ( (rc = rq->traverser_find (criteria)) < 0) {
        if (rq->get_traverser_err_msg () != "") {
             m_err_msg += __FUNCTION__;
             m_err_msg += rq->get_traverser_err_msg ();
            rq->clear_traverser_err_msg ();
        }
        return rc;
    }
    

    if ( (rc = rq->writers->emit_json (&o)) < 0) {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": ERROR: find writer emit: "
                      + std::string (strerror (errno)) + "\n";
       return rc;
    }

    return rc;
}

int reapi_cli_t::info (void *h, const uint64_t jobid, std::string &mode,
                       bool &reserved, int64_t &at, double &ov)
{
    resource_query_t *rq = static_cast<resource_query_t *> (h);
    std::shared_ptr<job_info_t> info = nullptr;

    if ( !(rq->job_exists (jobid))) {
       m_err_msg += __FUNCTION__;
       m_err_msg += ": ERROR: nonexistent job "
                     + std::to_string (jobid) + "\n";
       return -1;
    }

    info = rq->get_job (jobid);
    get_jobstate_str (info->state, mode);
    reserved = (info->state == job_lifecycle_t::RESERVED)? true : false;
    at = info->scheduled_at;
    ov = info->overhead;

    return 0;
}

int reapi_cli_t::stat (void *h, int64_t &V, int64_t &E,int64_t &J,
                       double &load, double &min, double &max, double &avg)
{
    return NOT_YET_IMPLEMENTED;
}

const std::string &reapi_cli_t::get_err_message ()
{
    return m_err_msg;
}

void reapi_cli_t::clear_err_message ()
{
    m_err_msg = "";
}

/****************************************************************************
 *                                                                          *
 *            Resource Query Class Private API Definitions                  *
 *                                                                          *
 ****************************************************************************/

std::shared_ptr<f_resource_graph_t> resource_query_t::create_filtered_graph ()
{
    std::shared_ptr<f_resource_graph_t> fg = nullptr;

    resource_graph_t &g = db->resource_graph;
    vtx_infra_map_t vmap = get (&resource_pool_t::idata, g);
    edg_infra_map_t emap = get (&resource_relation_t::idata, g);
    const multi_subsystemsS &filter = 
                        matcher->subsystemsS ();
    subsystem_selector_t<vtx_t, f_vtx_infra_map_t> vtxsel (vmap, filter);
    subsystem_selector_t<edg_t, f_edg_infra_map_t> edgsel (emap, filter);

    try {
        fg = std::make_shared<f_resource_graph_t> (g, edgsel, vtxsel);
    } catch (std::bad_alloc &e) {
        errno = ENOMEM;
        m_err_msg += __FUNCTION__;
        m_err_msg += ": Error allocating memory: " + std::string (e.what ())
                    + "\n";
        fg = nullptr;
    }

    return fg;
}

int resource_query_t::subsystem_exist (const std::string &n)
{
    int rc = 0;
    if (db->metadata.roots.find (n) == db->metadata.roots.end ())
        rc = -1;
    return rc;
}

int resource_query_t::set_subsystems_use (const std::string &n)
{
    int rc = 0;
    matcher->set_matcher_name (n);
    const std::string &matcher_type = matcher->matcher_name ();

    if (boost::iequals (matcher_type, std::string ("CA"))) {
        if ( (rc = subsystem_exist ("containment")) == 0)
            matcher->add_subsystem ("containment", "*");
    } else if (boost::iequals (matcher_type, std::string ("IBA"))) {
        if ( (rc = subsystem_exist ("ibnet")) == 0)
            matcher->add_subsystem ("ibnet", "*");
    } else if (boost::iequals (matcher_type, std::string ("IBBA"))) {
        if ( (rc = subsystem_exist ("ibnetbw")) == 0)
            matcher->add_subsystem ("ibnetbw", "*");
    } else if (boost::iequals (matcher_type, std::string ("PFS1BA"))) {
        if ( (rc = subsystem_exist ("pfs1bw")) == 0)
            matcher->add_subsystem ("pfs1bw", "*");
    } else if (boost::iequals (matcher_type, std::string ("PA"))) {
        if ( (rc = subsystem_exist ("power")) == 0)
            matcher->add_subsystem ("power", "*");
    } else if (boost::iequals (matcher_type, std::string ("C+PFS1BA"))) {
        if ( (rc = subsystem_exist ("containment")) == 0)
            matcher->add_subsystem ("containment", "contains");
        if ( !rc && (rc = subsystem_exist ("pfs1bw")) == 0)
            matcher->add_subsystem ("pfs1bw", "*");
    } else if (boost::iequals (matcher_type, std::string ("C+IBA"))) {
        if ( (rc = subsystem_exist ("containment")) == 0)
            matcher->add_subsystem ("containment", "contains");
        if ( !rc && (rc = subsystem_exist ("ibnet")) == 0)
            matcher->add_subsystem ("ibnet", "connected_up");
    } else if (boost::iequals (matcher_type, std::string ("C+PA"))) {
        if ( (rc = subsystem_exist ("containment")) == 0)
            matcher->add_subsystem ("containment", "*");
        if ( !rc && (rc = subsystem_exist ("power")) == 0)
            matcher->add_subsystem ("power", "draws_from");
    } else if (boost::iequals (matcher_type, std::string ("IB+IBBA"))) {
        if ( (rc = subsystem_exist ("ibnet")) == 0)
            matcher->add_subsystem ("ibnet", "connected_down");
        if ( !rc && (rc = subsystem_exist ("ibnetbw")) == 0)
            matcher->add_subsystem ("ibnetbw", "*");
    } else if (boost::iequals (matcher_type, std::string ("C+P+IBA"))) {
        if ( (rc = subsystem_exist ("containment")) == 0)
            matcher->add_subsystem ("containment", "contains");
        if ( (rc = subsystem_exist ("power")) == 0)
            matcher->add_subsystem ("power", "draws_from");
        if ( !rc && (rc = subsystem_exist ("ibnet")) == 0)
            matcher->add_subsystem ("ibnet", "connected_up");
    } else if (boost::iequals (matcher_type, std::string ("V+PFS1BA"))) {
        if ( (rc = subsystem_exist ("virtual1")) == 0)
            matcher->add_subsystem ("virtual1", "*");
        if ( !rc && (rc = subsystem_exist ("pfs1bw")) == 0)
            matcher->add_subsystem ("pfs1bw", "*");
    } else if (boost::iequals (matcher_type, std::string ("VA"))) {
        if ( (rc = subsystem_exist ("virtual1")) == 0)
            matcher->add_subsystem ("virtual1", "*");
    } else if (boost::iequals (matcher_type, std::string ("ALL"))) {
        if ( (rc = subsystem_exist ("containment")) == 0)
            matcher->add_subsystem ("containment", "*");
        if ( !rc && (rc = subsystem_exist ("ibnet")) == 0)
            matcher->add_subsystem ("ibnet", "*");
        if ( !rc && (rc = subsystem_exist ("ibnetbw")) == 0)
            matcher->add_subsystem ("ibnetbw", "*");
        if ( !rc && (rc = subsystem_exist ("pfs1bw")) == 0)
            matcher->add_subsystem ("pfs1bw", "*");
        if ( (rc = subsystem_exist ("power")) == 0)
            matcher->add_subsystem ("power", "*");
    } else {
        rc = -1;
    }

    return rc;
}

int resource_query_t::set_resource_ctx_params (const std::string &options)
{
    int rc = -1;
    json_t *tmp_json = NULL, *opt_json = NULL;

    // Set default values
    perf.min = std::numeric_limits<double>::max();
    perf.max = 0.0f;
    perf.accum = 0.0f;
    params.load_file = "conf/default";
    params.load_format = "jgf";
    params.load_allowlist = "";
    params.matcher_name = "CA";
    params.matcher_policy = "first";
    params.o_fname = "";
    params.r_fname = "";
    params.o_fext = "dot";
    params.match_format = "jgf";
    params.o_format = emit_format_t::GRAPHVIZ_DOT;
    params.prune_filters = "ALL:core";
    params.reserve_vtx_vec = 0;
    params.elapse_time = false;
    params.disable_prompt = false;

    if ( !(opt_json = json_loads (options.c_str (), JSON_DECODE_ANY, NULL))) {
        errno = ENOMEM;
        m_err_msg += __FUNCTION__;
        m_err_msg += ": Error loading options\n";
        goto out;
    }

    // Override defaults if present in options argument
    if ( (tmp_json = json_object_get (opt_json, "load_format"))) {
        params.load_format = json_string_value (tmp_json);
        if (!params.load_format.c_str ()) { 
            errno = EINVAL;
            m_err_msg += __FUNCTION__;
            m_err_msg += ": Error loading load_format\n";
            json_decref (opt_json);
            goto out;
        }
    }
    if ( (tmp_json = json_object_get (opt_json, "load_allowlist"))) {
        params.load_allowlist = json_string_value (tmp_json);
        if (!params.load_allowlist.c_str ()) { 
            errno = EINVAL;
            m_err_msg += __FUNCTION__;
            m_err_msg += ": Error loading load_allowlist\n";
            json_decref (opt_json);
            goto out;
        }
    }
    if ( (tmp_json = json_object_get (opt_json, "matcher_name"))) {
        params.matcher_name = json_string_value (tmp_json);
        if (!params.matcher_name.c_str ()) { 
            errno = EINVAL;
            m_err_msg += __FUNCTION__;
            m_err_msg += ": Error loading matcher_name\n";
            json_decref (opt_json);
            goto out;
        }
    }
    if ( (tmp_json = json_object_get (opt_json, "matcher_policy"))) {
        params.matcher_policy = json_string_value (tmp_json);
        if (!params.matcher_policy.c_str ()) { 
            errno = EINVAL;
            m_err_msg += __FUNCTION__;
            m_err_msg += ": Error loading matcher_policy\n";
            json_decref (opt_json);
            goto out;
        }
    }
    if ( (tmp_json = json_object_get (opt_json, "match_format"))) {
        params.match_format = json_string_value (tmp_json);
        if (!params.match_format.c_str ()) { 
            errno = EINVAL;
            m_err_msg += __FUNCTION__;
            m_err_msg += ": Error loading match_format\n";
            json_decref (opt_json);
            goto out;
        }
    }
    if ( (tmp_json = json_object_get (opt_json, "prune_filters"))) {
        params.prune_filters = json_string_value (tmp_json);
        if (!params.prune_filters.c_str ()) { 
            errno = EINVAL;
            m_err_msg += __FUNCTION__;
            m_err_msg += ": Error loading prune_filters\n";
            json_decref (opt_json);
            goto out;
        }
    }
    if ( (tmp_json = json_object_get (opt_json, "reserve_vtx_vec")))
        // No need for check here; returns 0 on failure
        params.reserve_vtx_vec = json_integer_value (tmp_json);

    rc = 0;

out:
    return rc;
}

/****************************************************************************
 *                                                                          *
 *            Resource Query Class Public API Definitions                   *
 *                                                                          *
 ****************************************************************************/

resource_query_t::resource_query_t () 
{

}

resource_query_t::~resource_query_t () 
{

}

resource_query_t::resource_query_t (const std::string &rgraph,
                                    const std::string &options)
{
    m_err_msg = "";
    std::string tmp_err = "";
    std::stringstream buffer{};
    std::shared_ptr<resource_reader_base_t> rd;
    match_format_t format;

    // Both calls can throw bad_alloc. Client must handle the errors.
    db = std::make_shared<resource_graph_db_t> ();
    traverser = std::make_shared<dfu_traverser_t> ();

    if (set_resource_ctx_params (options) < 0) {
        tmp_err = __FUNCTION__;
        tmp_err += ": ERROR: can't set resource graph parameters\n";
        throw std::runtime_error (tmp_err);
    }

    if ( !(matcher = create_match_cb (params.matcher_policy))) {
        tmp_err = __FUNCTION__;
        tmp_err += ": ERROR: can't create matcher\n";
        throw std::runtime_error (tmp_err);
    }

    if (params.reserve_vtx_vec != 0)
        db->resource_graph.m_vertices.reserve (params.reserve_vtx_vec);

    if ( (rd = create_resource_reader (params.load_format)) == nullptr) {
        tmp_err = __FUNCTION__;
        tmp_err +=  ": ERROR: can't create reader\n";
        throw std::runtime_error (tmp_err);
    }

    if (params.load_allowlist != "") {
        if (rd->set_allowlist (params.load_allowlist) < 0) {
            m_err_msg += __FUNCTION__;
            m_err_msg += ": ERROR: can't set allowlist\n";
        }
        if (!rd->is_allowlist_supported ())
            m_err_msg += "WARN: allowlist unsupported\n";
    }

    if (db->load (rgraph, rd) != 0) {
        tmp_err = __FUNCTION__;
        tmp_err +=  ": ERROR: " + rd->err_message () + "\n";
        tmp_err += "ERROR: error generating resources\n";
        throw std::runtime_error (tmp_err);
    }

    if (set_subsystems_use (params.matcher_name) != 0) {
        tmp_err = __FUNCTION__;
        tmp_err +=  ": ERROR: can't set subsystem\n";
        throw std::runtime_error (tmp_err);
    }

    if ( !(fgraph = create_filtered_graph ())) {
        tmp_err = __FUNCTION__;
        tmp_err +=  ": ERROR: can't create filtered graph\n";
        throw std::runtime_error (tmp_err);
    }

    jobid_counter = 1;
    if (params.prune_filters != ""
        && matcher->set_pruning_types_w_spec (
                                matcher->dom_subsystem (),
                                params.prune_filters)
                                < 0) {
        tmp_err = __FUNCTION__;
        tmp_err +=  ": ERROR: can't initialize pruning filters\n";
        throw std::runtime_error (tmp_err);
    }

    if (traverser->initialize (fgraph, db, matcher) != 0) {
        tmp_err = __FUNCTION__;
        tmp_err +=  ": ERROR: can't initialize traverser\n";
        throw std::runtime_error (tmp_err);
    }

    format = match_writers_factory_t::get_writers_type (params.match_format);
    if ( !(writers = match_writers_factory_t::create (format))) {
        tmp_err = __FUNCTION__;
        tmp_err +=  ": ERROR: can't create match writer\n";
        throw std::runtime_error (tmp_err);
    }

    return;
}

const std::string &resource_query_t::get_resource_query_err_msg () const
{
    return m_err_msg;
}

const std::string &resource_query_t::get_traverser_err_msg () const
{
    return traverser->err_message ();
}

const bool resource_query_t::job_exists (const uint64_t jobid)
{
    return jobs.find (jobid) != jobs.end ();
}

const uint64_t resource_query_t::get_job_counter () const
{
    return jobid_counter;
}

const std::shared_ptr<job_info_t> &resource_query_t::get_job (const uint64_t jobid)
{
    return jobs.at (jobid);
}

const bool resource_query_t::allocation_exists (const uint64_t jobid)
{
    return allocations.find (jobid) != allocations.end ();
}

const bool resource_query_t::reservation_exists (const uint64_t jobid)
{
    return reservations.find (jobid) != reservations.end ();
}

void resource_query_t::clear_resource_query_err_msg ()
{
    m_err_msg = "";
}

void resource_query_t::clear_traverser_err_msg ()
{
    traverser->clear_err_message ();
}

void resource_query_t::set_reservation (const uint64_t jobid)
{
    reservations[jobid] = jobid;
}

void resource_query_t::erase_reservation (const uint64_t jobid)
{
    reservations.erase (jobid);
}

void resource_query_t::set_allocation (const uint64_t jobid)
{
    allocations[jobid] = jobid;
}

void resource_query_t::erase_allocation (const uint64_t jobid)
{
    allocations.erase (jobid);
}

void resource_query_t::set_job (const uint64_t jobid,
                                const std::shared_ptr<job_info_t> &job)
{
    jobs[jobid] = job;
}

int resource_query_t::remove_job (const uint64_t jobid)
{
    int rc = -1;

    if (jobid > (uint64_t) std::numeric_limits<int64_t>::max ()) {
        errno = EOVERFLOW;
        return rc;
    }

    rc = traverser->remove (static_cast<int64_t> (jobid));
    if (rc == 0) {
        if (jobs.find (jobid) != jobs.end ()) {
            std::shared_ptr<job_info_t> info = jobs[jobid];
            info->state = job_lifecycle_t::CANCELED;
        }
    } else {
        m_err_msg += traverser->err_message ();
        traverser->clear_err_message ();
    }
    return rc;
}

void resource_query_t::incr_job_counter ()
{
    jobid_counter++;
}

int resource_query_t::traverser_run (Flux::Jobspec::Jobspec &job, match_op_t op,
                                     int64_t jobid, int64_t &at)
{
    return traverser->run (job, writers, op, jobid, &at);
}

int resource_query_t::traverser_find (std::string criteria)
{
    return traverser->find (writers, criteria);
}


} // namespace Flux::resource_model::detail
} // namespace Flux::resource_model
} // namespace Flux

#endif // REAPI_CLI_IMPL_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

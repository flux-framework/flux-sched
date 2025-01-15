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
#include <vector>
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
    double ts1 = (double)st.tv_sec + (double)st.tv_usec / 1000000.0f;
    double ts2 = (double)et.tv_sec + (double)et.tv_usec / 1000000.0f;
    return ts2 - ts1;
}

////////////////////////////////////////////////////////////////////////////////
// REAPI CLI Class Private Definitions
////////////////////////////////////////////////////////////////////////////////

std::string reapi_cli_t::m_err_msg = "";

////////////////////////////////////////////////////////////////////////////////
// REAPI CLI Class Public API Definitions
////////////////////////////////////////////////////////////////////////////////

int reapi_cli_t::match_allocate (void *h,
                                 match_op_t match_op,
                                 const std::string &jobspec,
                                 const uint64_t jobid,
                                 bool &reserved,
                                 std::string &R,
                                 int64_t &at,
                                 double &ov)
{
    resource_query_t *rq = static_cast<resource_query_t *> (h);
    int rc = -1;
    at = 0;
    ov = 0.0f;
    job_lifecycle_t st;
    std::shared_ptr<job_info_t> job_info = nullptr;
    struct timeval start_time, end_time;
    std::stringstream o;
    bool matched = false;

    if (!match_op_valid (match_op)) {
        m_err_msg += __FUNCTION__;
        m_err_msg +=
            ": ERROR: Invalid Match Option: " + std::string (match_op_to_string (match_op)) + "\n";
        rc = -1;
        goto out;
    }

    try {
        Flux::Jobspec::Jobspec job{jobspec};

        if ((rc = gettimeofday (&start_time, NULL)) < 0) {
            m_err_msg += __FUNCTION__;
            m_err_msg += ": ERROR: gettimeofday: " + std::string (strerror (errno)) + "\n";
            goto out;
        }

        rc = rq->traverser_run (job, match_op, (int64_t)jobid, at);

        if (rq->get_traverser_err_msg () != "") {
            m_err_msg += __FUNCTION__;
            m_err_msg += ": ERROR: " + rq->get_traverser_err_msg () + "\n";
            rq->clear_traverser_err_msg ();
            rc = -1;
            goto out;
        }
    } catch (Flux::Jobspec::parse_error &e) {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": ERROR: Jobspec error for " + std::to_string (rq->get_job_counter ()) + ": "
                     + std::string (e.what ()) + "\n";
        rc = -1;
        goto out;
    }

    if ((rc != 0) && (errno == ENOMEM)) {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": ERROR: Memory error for " + std::to_string (rq->get_job_counter ());
        rc = -1;
        goto out;
    }

    // Check for an unsuccessful match
    if ((rc == 0) && (match_op != match_op_t::MATCH_SATISFIABILITY)) {
        matched = true;
        errno = 0;
    }

    if ((rc = rq->writers->emit (o)) < 0) {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": ERROR: match writer emit: " + std::string (strerror (errno)) + "\n";
        goto out;
    }

    R = o.str ();

    if ((rc = gettimeofday (&end_time, NULL)) < 0) {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": ERROR: gettimeofday: " + std::string (strerror (errno)) + "\n";
        goto out;
    }

    ov = get_elapsed_time (start_time, end_time);

    if (matched) {
        reserved = (at != 0) ? true : false;
        st = (reserved) ? job_lifecycle_t::RESERVED : job_lifecycle_t::ALLOCATED;
        if (reserved)
            rq->set_reservation (jobid);
        else
            rq->set_allocation (jobid);

        job_info = std::make_shared<job_info_t> (jobid, st, at, "", "", ov);
        if (job_info == nullptr) {
            errno = ENOMEM;
            m_err_msg += __FUNCTION__;
            m_err_msg += ": ERROR: can't allocate memory: " + std::string (strerror (errno)) + "\n";
            rc = -1;
            goto out;
        }
        rq->set_job (jobid, job_info);
    }

    if (match_op != match_op_t::MATCH_SATISFIABILITY)
        rq->incr_job_counter ();

out:
    return rc;
}

int reapi_cli_t::update_allocate (void *h,
                                  const uint64_t jobid,
                                  const std::string &R,
                                  int64_t &at,
                                  double &ov,
                                  std::string &R_out)
{
    return NOT_YET_IMPLEMENTED;
}

int reapi_cli_t::add_subgraph (void *h, const std::string &R_subgraph)
{
    resource_query_t *rq = static_cast<resource_query_t *> (h);
    int rc = -1;

    rq->clear_resource_query_err_msg ();
    if ((rc = rq->add_subgraph (R_subgraph)) != 0) {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": ERROR: add subgraph error: " + std::string (strerror (errno)) + "\n";
        m_err_msg += rq->get_resource_query_err_msg () + "\n";
    }

    return rc;
}

int reapi_cli_t::remove_subgraph (void *h, const std::string &subgraph_path)
{
    resource_query_t *rq = static_cast<resource_query_t *> (h);
    int rc = -1;

    rq->clear_resource_query_err_msg ();
    if ((rc = rq->remove_subgraph (subgraph_path)) != 0) {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": ERROR: remove_subgraph error: " + std::string (strerror (errno)) + "\n";
        m_err_msg += rq->get_resource_query_err_msg () + "\n";
    }

    return rc;
}

int reapi_cli_t::match_allocate_multi (void *h,
                                       bool orelse_reserve,
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
        if ((rc = rq->remove_job (jobid)) == 0)
            rq->erase_allocation (jobid);
    } else if (rq->reservation_exists (jobid)) {
        if ((rc = rq->remove_job (jobid)) == 0)
            rq->erase_reservation (jobid);
    } else {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": ERROR: nonexistent job " + std::to_string (jobid) + "\n";
        rc = noent_ok ? 0 : -1;
        goto out;
    }

    if (rc != 0) {
        m_err_msg += __FUNCTION__;
        m_err_msg +=
            ": ERROR: error encountered while removing job " + std::to_string (jobid) + "\n";
    }

out:
    return rc;
}

int reapi_cli_t::cancel (void *h,
                         const uint64_t jobid,
                         const std::string &R,
                         bool noent_ok,
                         bool &full_removal)
{
    resource_query_t *rq = static_cast<resource_query_t *> (h);
    int rc = -1;

    if (rq->allocation_exists (jobid)) {
        if ((rc = rq->remove_job (jobid, R, full_removal)) == 0) {
            if (full_removal)
                rq->erase_allocation (jobid);
        }
    } else {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": WARNING: can't find allocation for jobid: " + std::to_string (jobid) + "\n";
        rc = noent_ok ? 0 : -1;
        goto out;
    }

    if (rc != 0) {
        m_err_msg += __FUNCTION__;
        m_err_msg +=
            ": ERROR: error encountered while removing job " + std::to_string (jobid) + "\n";
    }

out:
    return rc;
}

int reapi_cli_t::find (void *h, std::string criteria, json_t *&o)
{
    int rc = -1;
    resource_query_t *rq = static_cast<resource_query_t *> (h);

    if ((rc = rq->traverser_find (criteria)) < 0) {
        if (rq->get_traverser_err_msg () != "") {
            m_err_msg += __FUNCTION__;
            m_err_msg += rq->get_traverser_err_msg ();
            rq->clear_traverser_err_msg ();
        }
        return rc;
    }

    if ((rc = rq->writers->emit_json (&o)) < 0) {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": ERROR: find writer emit: " + std::string (strerror (errno)) + "\n";
        return rc;
    }

    return rc;
}

int reapi_cli_t::info (void *h,
                       const uint64_t jobid,
                       std::string &mode,
                       bool &reserved,
                       int64_t &at,
                       double &ov)
{
    resource_query_t *rq = static_cast<resource_query_t *> (h);
    std::shared_ptr<job_info_t> info = nullptr;

    if (!(rq->job_exists (jobid))) {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": ERROR: nonexistent job " + std::to_string (jobid) + "\n";
        return -1;
    }

    info = rq->get_job (jobid);
    get_jobstate_str (info->state, mode);
    reserved = (info->state == job_lifecycle_t::RESERVED) ? true : false;
    at = info->scheduled_at;
    ov = info->overhead;

    return 0;
}

int reapi_cli_t::info (void *h, const uint64_t jobid, std::shared_ptr<job_info_t> &job)
{
    resource_query_t *rq = static_cast<resource_query_t *> (h);

    if (!(rq->job_exists (jobid))) {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": ERROR: nonexistent job " + std::to_string (jobid) + "\n";
        return -1;
    }

    job = rq->get_job (jobid);
    return 0;
}

unsigned int reapi_cli_t::preorder_count (void *h)
{
    resource_query_t *rq = static_cast<resource_query_t *> (h);

    return rq->preorder_count ();
}

unsigned int reapi_cli_t::postorder_count (void *h)
{
    resource_query_t *rq = static_cast<resource_query_t *> (h);

    return rq->postorder_count ();
}

int reapi_cli_t::stat (void *h,
                       int64_t &V,
                       int64_t &E,
                       int64_t &J,
                       double &load,
                       double &min,
                       double &max,
                       double &avg)
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

////////////////////////////////////////////////////////////////////////////////
// Resource Query Class Private API Definitions
////////////////////////////////////////////////////////////////////////////////

int resource_query_t::subsystem_exist (const std::string_view &n)
{
    int rc = 0;
    if (db->metadata.roots.find (subsystem_t{n}) == db->metadata.roots.end ())
        rc = -1;
    return rc;
}

int resource_query_t::set_subsystems_use (const std::string &n)
{
    int rc = 0;
    matcher->set_matcher_name (n);
    const std::string &matcher_type = matcher->matcher_name ();
    subsystem_t ibnet_sub{"ibnet"};
    subsystem_t pfs1bw_sub{"pfs1bw"};
    subsystem_t power_sub{"power"};
    subsystem_t ibnetbw_sub{"ibnetbw"};
    subsystem_t virtual1_sub{"virtual1"};
    std::map<std::string, std::vector<subsystem_t>>
        subsystem_map{{"CA", {containment_sub}},
                      {"IBA", {ibnet_sub}},
                      {"IBBA", {ibnetbw_sub}},
                      {"PA", {power_sub}},
                      {"PFS1BA", {pfs1bw_sub}},
                      {"C+IBA", {containment_sub, ibnet_sub}},
                      {"C+IBBA", {containment_sub, ibnetbw_sub}},
                      {"C+PA", {containment_sub, power_sub}},
                      {"C+PFS1BA", {containment_sub, pfs1bw_sub}},
                      {"IB+IBBA", {ibnet_sub, ibnetbw_sub}},
                      {"C+P+IBA", {containment_sub, power_sub, ibnet_sub}},
                      {"V+PFS1BA", {virtual1_sub, pfs1bw_sub}},
                      {"VA", {virtual1_sub}},
                      {"ALL", {containment_sub, ibnet_sub, ibnetbw_sub, pfs1bw_sub, power_sub}}};
    {
        // add lower case versions
        auto lower_case = subsystem_map;
        for (auto &[k, v] : lower_case) {
            std::string tmp = k;
            boost::algorithm::to_lower (tmp);
            subsystem_map.emplace (tmp, v);
        }
    }
    std::map<subsystem_t, std::string> subsys_to_edge_name = {
        {containment_sub, "contains"},
        {ibnet_sub, "connected_down"},
        {ibnetbw_sub, "*"},
        {pfs1bw_sub, "*"},
        {virtual1_sub, "*"},
        {power_sub, "supplies_to"},
    };

    for (auto &sub : subsystem_map.at (n)) {
        matcher->add_subsystem (sub, subsys_to_edge_name.at (sub));
    }

    return 0;
}

int resource_query_t::set_resource_ctx_params (const std::string &options)
{
    int rc = -1;
    json_t *tmp_json = NULL, *opt_json = NULL;

    // Set default values
    perf.min = std::numeric_limits<double>::max ();
    perf.max = 0.0f;
    perf.accum = 0.0f;
    params.load_file = "conf/default";
    params.load_format = "grug";
    params.load_allowlist = "";
    params.matcher_name = "CA";
    params.matcher_policy = "first";
    params.traverser_policy = "simple";
    params.o_fname = "";
    params.r_fname = "";
    params.o_fext = "dot";
    params.match_format = "jgf";
    params.o_format = emit_format_t::GRAPHVIZ_DOT;
    params.prune_filters = "ALL:core,ALL:node";
    params.reserve_vtx_vec = 0;
    params.elapse_time = false;
    params.disable_prompt = false;

    if (!(opt_json = json_loads (options.c_str (), JSON_DECODE_ANY, NULL))) {
        errno = ENOMEM;
        m_err_msg += __FUNCTION__;
        m_err_msg += ": Error loading options\n";
        goto out;
    }

    // Override defaults if present in options argument
    if ((tmp_json = json_object_get (opt_json, "load_format"))) {
        params.load_format = json_string_value (tmp_json);
        if (!params.load_format.c_str ()) {
            errno = EINVAL;
            m_err_msg += __FUNCTION__;
            m_err_msg += ": Error loading load_format\n";
            json_decref (opt_json);
            goto out;
        }
    }
    if ((tmp_json = json_object_get (opt_json, "load_allowlist"))) {
        params.load_allowlist = json_string_value (tmp_json);
        if (!params.load_allowlist.c_str ()) {
            errno = EINVAL;
            m_err_msg += __FUNCTION__;
            m_err_msg += ": Error loading load_allowlist\n";
            json_decref (opt_json);
            goto out;
        }
    }
    if ((tmp_json = json_object_get (opt_json, "matcher_name"))) {
        params.matcher_name = json_string_value (tmp_json);
        if (!params.matcher_name.c_str ()) {
            errno = EINVAL;
            m_err_msg += __FUNCTION__;
            m_err_msg += ": Error loading matcher_name\n";
            json_decref (opt_json);
            goto out;
        }
    }
    if ((tmp_json = json_object_get (opt_json, "matcher_policy"))) {
        params.matcher_policy = json_string_value (tmp_json);
        if (!params.matcher_policy.c_str ()) {
            errno = EINVAL;
            m_err_msg += __FUNCTION__;
            m_err_msg += ": Error loading matcher_policy\n";
            json_decref (opt_json);
            goto out;
        }
    }
    if ((tmp_json = json_object_get (opt_json, "traverser_policy"))) {
        params.traverser_policy = json_string_value (tmp_json);
        if (!params.traverser_policy.c_str ()) {
            errno = EINVAL;
            m_err_msg += __FUNCTION__;
            m_err_msg += ": Error loading traverser_policy\n";
            json_decref (opt_json);
            goto out;
        }
    }
    if ((tmp_json = json_object_get (opt_json, "match_format"))) {
        params.match_format = json_string_value (tmp_json);
        if (!params.match_format.c_str ()) {
            errno = EINVAL;
            m_err_msg += __FUNCTION__;
            m_err_msg += ": Error loading match_format\n";
            json_decref (opt_json);
            goto out;
        }
    }
    if ((tmp_json = json_object_get (opt_json, "prune_filters"))) {
        params.prune_filters = json_string_value (tmp_json);
        if (!params.prune_filters.c_str ()) {
            errno = EINVAL;
            m_err_msg += __FUNCTION__;
            m_err_msg += ": Error loading prune_filters\n";
            json_decref (opt_json);
            goto out;
        }
    }
    if ((tmp_json = json_object_get (opt_json, "reserve_vtx_vec")))
        // No need for check here; returns 0 on failure
        params.reserve_vtx_vec = json_integer_value (tmp_json);
    if ((tmp_json = json_object_get (opt_json, "elapse_time")))
        // No need for check here; returns 0 on failure
        params.elapse_time = json_boolean_value (tmp_json);

    rc = 0;

out:
    return rc;
}

////////////////////////////////////////////////////////////////////////////////
// Resource Query Class Public API Definitions
////////////////////////////////////////////////////////////////////////////////

resource_query_t::resource_query_t ()
{
}

resource_query_t::~resource_query_t ()
{
}

resource_query_t::resource_query_t (const std::string &rgraph, const std::string &options)
{
    m_err_msg = "";
    std::string tmp_err = "";
    std::stringstream buffer{};
    std::shared_ptr<resource_reader_base_t> rd;
    match_format_t format;

    // Both calls can throw bad_alloc. Client must handle the errors.
    db = std::make_shared<resource_graph_db_t> ();

    if (set_resource_ctx_params (options) < 0) {
        tmp_err = __FUNCTION__;
        tmp_err += ": ERROR: can't set resource graph parameters\n";
        throw std::runtime_error (tmp_err);
    }

    if (!(matcher = create_match_cb (params.matcher_policy))) {
        tmp_err = __FUNCTION__;
        tmp_err += ": ERROR: can't create matcher\n";
        throw std::runtime_error (tmp_err);
    }

    traverser = std::make_shared<dfu_traverser_t> (params.traverser_policy);

    if (params.reserve_vtx_vec != 0)
        db->resource_graph.m_vertices.reserve (params.reserve_vtx_vec);

    if ((rd = create_resource_reader (params.load_format)) == nullptr) {
        tmp_err = __FUNCTION__;
        tmp_err += ": ERROR: can't create reader\n";
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
        tmp_err += ": ERROR: " + rd->err_message () + "\n";
        tmp_err += "ERROR: error generating resources\n";
        throw std::runtime_error (tmp_err);
    }

    if (set_subsystems_use (params.matcher_name) != 0) {
        tmp_err = __FUNCTION__;
        tmp_err += ": ERROR: can't set subsystem\n";
        throw std::runtime_error (tmp_err);
    }

    jobid_counter = 1;
    if (params.prune_filters != ""
        && matcher->set_pruning_types_w_spec (matcher->dom_subsystem (), params.prune_filters)
               < 0) {
        tmp_err = __FUNCTION__;
        tmp_err += ": ERROR: can't initialize pruning filters\n";
        throw std::runtime_error (tmp_err);
    }

    if (traverser->initialize (db, matcher) != 0) {
        tmp_err = __FUNCTION__;
        tmp_err += ": ERROR: can't initialize traverser\n";
        throw std::runtime_error (tmp_err);
    }

    format = match_writers_factory_t::get_writers_type (params.match_format);
    if (!(writers = match_writers_factory_t::create (format))) {
        tmp_err = __FUNCTION__;
        tmp_err += ": ERROR: can't create match writer\n";
        throw std::runtime_error (tmp_err);
    }

    subsystem_t::storage_t::finalize ();
    resource_type_t::storage_t::finalize ();
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

const unsigned int resource_query_t::preorder_count ()
{
    return traverser->get_total_preorder_count ();
}

const unsigned int resource_query_t::postorder_count ()
{
    return traverser->get_total_postorder_count ();
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

void resource_query_t::set_job (const uint64_t jobid, const std::shared_ptr<job_info_t> &job)
{
    jobs[jobid] = job;
}

int resource_query_t::remove_job (const uint64_t jobid)
{
    int rc = -1;

    if (jobid > (uint64_t)std::numeric_limits<int64_t>::max ()) {
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
    }
    return rc;
}

int resource_query_t::remove_job (const uint64_t jobid, const std::string &R, bool &full_removal)
{
    int rc = -1;
    std::shared_ptr<resource_reader_base_t> reader;

    if (jobid > (uint64_t)std::numeric_limits<int64_t>::max ()) {
        errno = EOVERFLOW;
        return rc;
    }
    if (R == "") {
        errno = EINVAL;
        return rc;
    }
    if ((reader = create_resource_reader (params.load_format)) == nullptr) {
        m_err_msg = __FUNCTION__;
        m_err_msg += ": ERROR: can't create reader\n";
        return rc;
    }

    rc = traverser->remove (R, reader, static_cast<int64_t> (jobid), full_removal);
    if (rc == 0) {
        if (full_removal) {
            auto job_info_it = jobs.find (jobid);
            if (job_info_it != jobs.end ()) {
                job_info_it->second->state = job_lifecycle_t::CANCELED;
            }
        }
    } else {
        m_err_msg += traverser->err_message ();
    }

    return rc;
}

int resource_query_t::add_subgraph (const std::string &R_subgraph)
{
    int rc = -1;
    std::shared_ptr<resource_reader_base_t> reader;
    vtx_t v = boost::graph_traits<resource_graph_t>::null_vertex ();

    if ((reader = create_resource_reader ("jgf")) == nullptr) {
        m_err_msg = __FUNCTION__;
        m_err_msg += ": ERROR: can't create JGF reader\n";
        goto out;
    }
    // Could be adding new resource types, so open the interner storage before adding the subgraph
    subsystem_t::storage_t::open ();
    resource_type_t::storage_t::open ();
    if ((rc = reader->unpack_at (db->resource_graph, db->metadata, v, R_subgraph, -1)) != 0) {
        m_err_msg = __FUNCTION__;
        m_err_msg += ": ERROR: reader returned error: ";
        m_err_msg += reader->err_message () + "\n";
        goto out;
    }
    if ((rc = traverser->initialize ()) != 0) {
        m_err_msg = __FUNCTION__;
        m_err_msg += ": ERROR: reinitializing traverser after adding subgraph. ";
        m_err_msg += reader->err_message () + "\n";
        goto out;
    }
    rc = 0;

out:
    // Close the interner storage after adding the subgraph
    subsystem_t::storage_t::finalize ();
    resource_type_t::storage_t::finalize ();
    return rc;
}

int resource_query_t::remove_subgraph (const std::string &subgraph_path)
{
    int rc = -1;

    if ((rc = traverser->remove_subgraph (subgraph_path)) != 0) {
        m_err_msg = __FUNCTION__;
        m_err_msg += ": ERROR: reader returned error: ";
        m_err_msg += traverser->err_message () + "\n";
        return rc;
    }
    if ((rc = traverser->initialize ()) != 0) {
        m_err_msg = __FUNCTION__;
        m_err_msg += ": ERROR: reinitialize traverser after subgraph removal. ";
        m_err_msg += traverser->err_message () + "\n";
    }
    return rc;
}

void resource_query_t::incr_job_counter ()
{
    jobid_counter++;
}

int resource_query_t::traverser_run (Flux::Jobspec::Jobspec &job,
                                     match_op_t op,
                                     int64_t jobid,
                                     int64_t &at)
{
    return traverser->run (job, writers, op, jobid, &at);
}

int resource_query_t::traverser_find (std::string criteria)
{
    return traverser->find (writers, criteria);
}

}  // namespace detail
}  // namespace resource_model
}  // namespace Flux

#endif  // REAPI_CLI_IMPL_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

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
    int traverser_rc;
    at = 0;
    ov = 0.0f;
    job_lifecycle_t st;
    std::shared_ptr<job_info_t> job_info = nullptr;
    struct timeval start_time, end_time;
    std::stringstream o;
    int traverser_errno;
    bool matched = false;

    if (!rq) {
        errno = EINVAL;
        return -1;
    }

    if (!match_op_valid (match_op)) {
        m_err_msg += __FUNCTION__;
        m_err_msg +=
            ": ERROR: Invalid Match Option: " + std::string (match_op_to_string (match_op)) + "\n";
        errno = EINVAL;
        return -1;
    }

    if (gettimeofday (&start_time, NULL) < 0) {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": ERROR: gettimeofday: " + std::string (strerror (errno)) + "\n";
        return -1;
    }

    Flux::Jobspec::Jobspec job;
    try {
        job = Flux::Jobspec::Jobspec{jobspec};
    } catch (Flux::Jobspec::parse_error &e) {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": ERROR: Jobspec error for " + std::to_string (rq->get_job_counter ()) + ": "
                     + std::string (e.what ()) + "\n";
        errno = EINVAL;
        return -1;
    }

    /* The traverser returns -1 with errno set on failure.  It does not throw.
     * Continue on (but ultimately return -1) if errno is one of
     * EBUSY - temporarily unavailable
     * ENODEV - unsatisfiable
     * Otherwise, return immediately.
     */
    if ((traverser_rc = rq->traverser_run (job, match_op, (int64_t)jobid, at)) < 0) {
        traverser_errno = errno;
        if (rq->get_traverser_err_msg () != "") {
            m_err_msg += __FUNCTION__;
            m_err_msg += ": ERROR: " + rq->get_traverser_err_msg () + "\n";
            rq->clear_traverser_err_msg ();  // clear error for next call
        } else if (traverser_errno != EBUSY && traverser_errno != ENODEV) {
            m_err_msg += __FUNCTION__;
            m_err_msg += ": ERROR: traverser: " + std::string (strerror (traverser_errno)) + "\n";
        }
        if (traverser_errno != EBUSY && traverser_errno != ENODEV)
            return -1;
    } else if (match_op != match_op_t::MATCH_SATISFIABILITY) {
        matched = true;
    }

    if (rq->writers->emit (o) < 0) {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": ERROR: match writer emit: " + std::string (strerror (errno)) + "\n";
        return -1;
    }

    R = o.str ();

    if (gettimeofday (&end_time, NULL) < 0) {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": ERROR: gettimeofday: " + std::string (strerror (errno)) + "\n";
        return -1;
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
            return -1;
        }
        rq->set_job (jobid, job_info);
    }

    if (match_op != match_op_t::MATCH_SATISFIABILITY)
        rq->incr_job_counter ();

    if (traverser_rc < 0) {
        errno = traverser_errno;
        return -1;
    }
    return 0;
}

int reapi_cli_t::update_allocate (void *h,
                                  const uint64_t jobid,
                                  const std::string &R,
                                  int64_t &at,
                                  double &ov,
                                  std::string &R_out)
{
    errno = ENOSYS;  // not implemented
    return -1;
}

int reapi_cli_t::match_allocate_multi (void *h,
                                       bool orelse_reserve,
                                       const char *jobs,
                                       queue_adapter_base_t *adapter)
{
    errno = ENOSYS;  // not implemented
    return -1;
}

int reapi_cli_t::cancel (void *h, const uint64_t jobid, bool noent_ok)
{
    resource_query_t *rq = static_cast<resource_query_t *> (h);
    int rc = -1;

    if (!rq) {
        errno = EINVAL;
        return -1;
    }

    if (rq->allocation_exists (jobid)) {
        if ((rc = rq->remove_job (jobid)) == 0)
            rq->erase_allocation (jobid);
    } else if (rq->reservation_exists (jobid)) {
        if ((rc = rq->remove_job (jobid)) == 0)
            rq->erase_reservation (jobid);
    } else {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": ERROR: nonexistent job " + std::to_string (jobid) + "\n";
        if (noent_ok) {
            rc = 0;
        } else {
            errno = ENOENT;
            rc = -1;
        }
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

    if (!rq) {
        errno = EINVAL;
        return -1;
    }

    if (rq->allocation_exists (jobid)) {
        if ((rc = rq->remove_job (jobid, R, full_removal)) == 0) {
            if (full_removal)
                rq->erase_allocation (jobid);
        }
    } else {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": WARNING: can't find allocation for jobid: " + std::to_string (jobid) + "\n";
        if (noent_ok) {
            rc = 0;
        } else {
            errno = ENOENT;
            rc = -1;
        }
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

    if (!rq) {
        errno = EINVAL;
        return -1;
    }

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

    if (!rq) {
        errno = EINVAL;
        return -1;
    }

    if (!(rq->job_exists (jobid))) {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": ERROR: nonexistent job " + std::to_string (jobid) + "\n";
        errno = ENOENT;
        return -1;
    }

    info = rq->get_job (jobid);
    mode = get_jobstate_str (info->state);
    reserved = (info->state == job_lifecycle_t::RESERVED) ? true : false;
    at = info->scheduled_at;
    ov = info->overhead;

    return 0;
}

int reapi_cli_t::info (void *h, const uint64_t jobid, std::shared_ptr<job_info_t> &job)
{
    resource_query_t *rq = static_cast<resource_query_t *> (h);

    if (!rq) {
        errno = EINVAL;
        return -1;
    }

    if (!(rq->job_exists (jobid))) {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": ERROR: nonexistent job " + std::to_string (jobid) + "\n";
        errno = ENOENT;
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
    errno = ENOSYS;  // not implemented
    return -1;
}

static int parse_rpstatus (const char *status, resource_pool_t::status_t &rpstatus)
{
    if (!status)
        return -1;
    if (strcasecmp (status, "up") == 0)
        rpstatus = resource_pool_t::status_t::UP;
    else if (strcasecmp (status, "down") == 0)
        rpstatus = resource_pool_t::status_t::DOWN;
    else
        return -1;
    return 0;
}

int reapi_cli_t::set_status (void *h, const std::string &resource_path, const char *status)
{
    int rc = -1;
    resource_query_t *rq = static_cast<resource_query_t *> (h);
    resource_pool_t::status_t rpstatus;

    if (!rq || resource_path.empty () || parse_rpstatus (status, rpstatus) < 0) {
        errno = EINVAL;
        return -1;
    }
    rc = rq->traverser->mark (resource_path, rpstatus);
    if (rc < 0) {
        if (errno == 0)
            errno = EINVAL;
        m_err_msg = __FUNCTION__;
        m_err_msg += ": " + rq->traverser->err_message ();
    }
    return rc;
}

int reapi_cli_t::set_status (void *h, int64_t rank, const char *status)
{
    int rc = -1;
    resource_query_t *rq = static_cast<resource_query_t *> (h);
    resource_pool_t::status_t rpstatus;

    if (!rq || parse_rpstatus (status, rpstatus) < 0) {
        errno = EINVAL;
        return -1;
    }

    // Note: traverser->mark() can throw std::out_of_range
    std::set<int64_t> ranks;
    if (rank == FLUX_NODEID_ANY) {
        for (const auto &kv : rq->db->metadata.by_rank)
            if (kv.first >= 0)
                ranks.insert (kv.first);
    } else {
        if (rq->db->metadata.by_rank.find (rank) == rq->db->metadata.by_rank.end ()) {
            errno = ENOENT;
            m_err_msg = __FUNCTION__;
            m_err_msg += ": rank not found: " + std::to_string (rank);
            return -1;
        }
        ranks.insert (rank);
    }

    rc = rq->traverser->mark (ranks, rpstatus);
    if (rc < 0) {
        if (errno == 0)
            errno = EINVAL;
        m_err_msg = __FUNCTION__;
        m_err_msg += ": failed to mark rank " + std::to_string (rank);
    }
    return rc;
}

int reapi_cli_t::get_status (void *h, const std::string &resource_path, const char *&status)
{
    resource_query_t *rq = static_cast<resource_query_t *> (h);

    if (!rq || resource_path.empty ()) {
        errno = EINVAL;
        return -1;
    }

    auto vit = rq->db->metadata.by_path.find (resource_path);
    if (vit == rq->db->metadata.by_path.end ()) {
        errno = ENOENT;
        m_err_msg = __FUNCTION__;
        m_err_msg += ": resource path not found: " + resource_path;
        return -1;
    }

    // Get the first vertex at this path
    if (vit->second.empty ()) {
        errno = EINVAL;
        m_err_msg = __FUNCTION__;
        m_err_msg += ": no vertices at path: " + resource_path;
        return -1;
    }

    vtx_t v = vit->second.front ();
    // Note: can throw std::out_of_range if status field not in vertex
    status = resource_pool_t::status_to_str (rq->db->resource_graph[v].status);
    return 0;
}

int reapi_cli_t::get_status (void *h, int64_t rank, const char *&status)
{
    resource_query_t *rq = static_cast<resource_query_t *> (h);

    if (!rq || rank == FLUX_NODEID_ANY) {
        errno = EINVAL;
        return -1;
    }

    auto vit = rq->db->metadata.by_rank.find (rank);
    if (vit == rq->db->metadata.by_rank.end ()) {
        errno = ENOENT;
        m_err_msg = __FUNCTION__;
        m_err_msg += ": rank not found: " + std::to_string (rank);
        return -1;
    }

    // Get the node vertex (subtree root) at this rank
    if (vit->second.empty ()) {
        errno = EINVAL;
        m_err_msg = __FUNCTION__;
        m_err_msg += ": no vertices at rank: " + std::to_string (rank);
        return -1;
    }

    // Find the subtree root (shortest path = node vertex)
    vtx_t subtree_root = vit->second.front ();
    subsystem_t dom = rq->matcher->dom_subsystem ();
    const resource_graph_t &g = rq->db->resource_graph;

    // Note: paths.at() can throw std::out_of_range
    for (vtx_t v : vit->second) {
        std::string path = g[v].paths.at (dom);
        std::string root_path = g[subtree_root].paths.at (dom);
        if (path.length () < root_path.length ()) {
            subtree_root = v;
        }
    }

    status = resource_pool_t::status_to_str (g[subtree_root].status);
    return 0;
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

static void initialize_matchers_and_traversers (resource_query_t *rq)
{
    std::string tmp_err = "";

    if (!(rq->matcher = create_match_cb (rq->params.matcher_policy))) {
        tmp_err = __FUNCTION__;
        tmp_err += ": ERROR: can't create matcher\n";
        throw std::runtime_error (tmp_err);
    }
    if (rq->set_subsystems_use (rq->params.matcher_name) != 0) {
        tmp_err = __FUNCTION__;
        tmp_err += ": ERROR: can't set subsystems\n";
        throw std::runtime_error (tmp_err);
    }
    if (rq->params.prune_filters != ""
        && rq->matcher->set_pruning_types_w_spec (rq->matcher->dom_subsystem (),
                                                  rq->params.prune_filters)
               < 0) {
        tmp_err = __FUNCTION__;
        tmp_err += ": ERROR: can't set pruning filters\n";
        throw std::runtime_error (tmp_err);
    }

    // Wire a fresh traverser to the new db and matcher.
    rq->traverser = std::make_shared<dfu_traverser_t> (rq->params.traverser_policy);
    if (rq->traverser->initialize (rq->db, rq->matcher) != 0) {
        tmp_err = __FUNCTION__;
        tmp_err += ": ERROR: can't initialize traverser\n";
        throw std::runtime_error (tmp_err);
    }

    // Create fresh writers (stateless: only carry output format config).
    match_format_t format = match_writers_factory_t::get_writers_type (rq->params.match_format);
    if (!(rq->writers = match_writers_factory_t::create (format))) {
        tmp_err = __FUNCTION__;
        tmp_err += ": ERROR: can't create match writers\n";
        throw std::runtime_error (tmp_err);
    }
}

resource_query_t::resource_query_t (const resource_query_t &o)
{
    std::string tmp_err = "";

    params = o.params;
    m_err_msg = o.m_err_msg;
    jobid_counter = o.jobid_counter;
    perf = o.perf;
    jobs = o.jobs;
    allocations = o.allocations;
    reservations = o.reservations;

    // Deep-copy the resource graph database; resource_graph_db_t's copy
    // constructor rebuilds by_outedges with stable descriptors from the new
    // graph, preserving all allocation state and up/down status.
    db = std::make_shared<resource_graph_db_t> (*o.db);

    // Reset vertex colors: deep-copied idata.colors hold old-epoch values
    // from the source traverser.  initialize() bumps m_color_base, making
    // the old black values appear black again and causing stop_explore to
    // skip every vertex in prime_pruning_filter, zeroing subtree planners.
    resource_graph_t &g = db->resource_graph;
    vtx_iterator_t vi, vi_end;
    for (boost::tie (vi, vi_end) = boost::vertices (g); vi != vi_end; ++vi)
        g[*vi].idata.colors.clear ();

    // Re-create the matcher from the stored policy rather than copying
    // through the base-class pointer, which would slice the concrete subtype.
    initialize_matchers_and_traversers (this);
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

    jobid_counter = 1;
    initialize_matchers_and_traversers (this);

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

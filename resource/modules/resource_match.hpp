/*****************************************************************************\
 * Copyright 2014 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef RESOURCE_MATCH_HPP
#define RESOURCE_MATCH_HPP

extern "C" {
#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <flux/core.h>
#include <flux/idset.h>
}

#include <cstdint>
#include <limits>
#include <sstream>
#include <cerrno>
#include <map>
#include <cinttypes>
#include <chrono>

#include "resource_match.hpp"
#include "resource/schema/resource_graph.hpp"
#include "resource/readers/resource_reader_factory.hpp"
#include "resource/traversers/dfu.hpp"
#include "resource/jobinfo/jobinfo.hpp"
#include "resource/policies/dfu_match_policy_factory.hpp"
#include "resource_match_opts.hpp"
#include "resource/schema/perf_data.hpp"
#include <jansson.hpp>

using namespace Flux::resource_model;
using namespace Flux::opts_manager;


////////////////////////////////////////////////////////////////////////////////
// Resource Matching Service Module Context
////////////////////////////////////////////////////////////////////////////////

class msg_wrap_t {
   public:
    msg_wrap_t () = default;
    msg_wrap_t (const msg_wrap_t &o);
    msg_wrap_t &operator= (const msg_wrap_t &o);
    ~msg_wrap_t ();
    const flux_msg_t *get_msg () const;
    void set_msg (const flux_msg_t *msg);

   private:
    const flux_msg_t *m_msg = nullptr;
};

struct resobj_t {
    std::string exec_target_range;
    std::vector<uint64_t> core;
    std::vector<uint64_t> gpu;
};

class resource_interface_t {
   public:
    resource_interface_t () = default;
    resource_interface_t (const resource_interface_t &o);
    resource_interface_t &operator= (const resource_interface_t &o);

    ~resource_interface_t ();
    int fetch_and_reset_update_rc ();
    int get_update_rc () const;
    void set_update_rc (int rc);

    const std::string &get_ups () const;
    void set_ups (const char *ups);
    bool is_ups_set () const;
    flux_future_t *update_f = nullptr;

   private:
    std::string m_ups = "";
    int m_update_rc = 0;
};

struct resource_ctx_t : public resource_interface_t {
    ~resource_ctx_t ();
    flux_t *h;                     /* Flux handle */
    flux_msg_handler_t **handlers; /* Message handlers */
    Flux::opts_manager::optmgr_composer_t<Flux::opts_manager::resource_opts_t>
        opts;                                             /* Option manager */
    std::shared_ptr<dfu_match_cb_t> matcher;              /* Match callback object */
    std::shared_ptr<dfu_traverser_t> traverser;           /* Graph traverser object */
    std::shared_ptr<resource_graph_db_t> db;              /* Resource graph data store */
    std::shared_ptr<match_writers_t> writers;             /* Vertex/Edge writers */
    std::shared_ptr<resource_reader_base_t> reader;       /* resource reader */
    std::map<uint64_t, std::shared_ptr<job_info_t>> jobs; /* Jobs table */
    std::map<uint64_t, uint64_t> allocations;             /* Allocation table */
    std::map<uint64_t, uint64_t> reservations;            /* Reservation table */
    std::map<std::string, std::shared_ptr<msg_wrap_t>> notify_msgs;
    bool m_resources_updated = true;      /* resources have been updated */
    bool m_resources_down_updated = true; /* down resources have been updated */
    bool m_get_up_down_updates = true;
    bool m_acquire_resources_from_core = false;
    /* last time allocated resources search updated */
    std::chrono::time_point<std::chrono::system_clock> m_resources_alloc_updated;
    /* R caches */
    json::value m_r_all;
    json::value m_r_down;
    json::value m_r_alloc;
    /* All resources from resource.acquire */
    json_t *m_acquired_resources = NULL;
};

////////////////////////////////////////////////////////////////////////////////
// Request Handler Routines
////////////////////////////////////////////////////////////////////////////////

inline std::string get_status_string (int64_t now, int64_t at)
{
    return (at == now) ? "ALLOCATED" : "RESERVED";
}

inline bool is_existent_jobid (const std::shared_ptr<resource_ctx_t> &ctx, uint64_t jobid)
{
    return (ctx->jobs.find (jobid) != ctx->jobs.end ()) ? true : false;
}

int Rlite_equal (const std::shared_ptr<resource_ctx_t> &ctx, const char *R1, const char *R2);

int run_match (std::shared_ptr<resource_ctx_t> &ctx,
                      int64_t jobid,
                      const char *cmd,
                      const std::string &jstr,
                      int64_t *now,
                      int64_t *at,
                      double *overhead,
                      std::stringstream &o,
                      flux_error_t *errp);

int run_update (std::shared_ptr<resource_ctx_t> &ctx,
                       int64_t jobid,
                       const char *R,
                       int64_t &at,
                       double &overhead,
                       std::stringstream &o);

int run_remove (std::shared_ptr<resource_ctx_t> &ctx,
                       int64_t jobid,
                       const char *R,
                       bool part_cancel,
                       bool &full_removal);

int run_find (std::shared_ptr<resource_ctx_t> &ctx,
                     const std::string &criteria,
                     const std::string &format_str,
                     json_t **R);

////////////////////////////////////////////////////////////////////////////////
// Resource Graph and Traverser Initialization
////////////////////////////////////////////////////////////////////////////////

int populate_resource_db (std::shared_ptr<resource_ctx_t> &ctx, const char *ctx_handle);

int mark (std::shared_ptr<resource_ctx_t> &ctx,
                 const char *ids,
                 resource_pool_t::status_t status);

int update_resource_db (std::shared_ptr<resource_ctx_t> &ctx,
                               json_t *resources,
                               const char *up,
                               const char *down);

int select_subsystems (std::shared_ptr<resource_ctx_t> &ctx);

#endif  // RESOURCE_MATCH_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

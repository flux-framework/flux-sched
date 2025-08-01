/*****************************************************************************\
 * Copyright 2019 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef REAPI_CLI_HPP
#define REAPI_CLI_HPP

extern "C" {
#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <flux/core.h>
#include <cstdint>
#include <cerrno>
}

#include <fstream>
#include <memory>
#include "resource/reapi/bindings/c++/reapi.hpp"
#include "resource/jobinfo/jobinfo.hpp"
#include "resource/policies/dfu_match_policy_factory.hpp"
#include "resource/traversers/dfu.hpp"
#include "resource/policies/base/match_op.h"

namespace Flux {
namespace resource_model {
namespace detail {

enum class emit_format_t {
    GRAPHVIZ_DOT,
    GRAPH_ML,
};

struct match_perf_t {
    double min;   /* Min match time */
    double max;   /* Max match time */
    double accum; /* Total match time accumulated */
};

struct resource_params_t {
    std::string load_file;      /* load file name */
    std::string load_format;    /* load reader format */
    std::string load_allowlist; /* load resource allowlist */
    std::string matcher_name;   /* Matcher name */
    std::string matcher_policy; /* Matcher policy name */
    std::string o_fname;        /* Output file to dump the filtered graph */
    std::ofstream r_out;        /* Output file stream for emitted R */
    std::string r_fname;        /* Output file to dump the emitted R */
    std::string o_fext;         /* File extension */
    std::string prune_filters;  /* Raw prune-filter specification */
    std::string match_format;   /* Format to emit a matched resources */
    emit_format_t o_format;
    bool elapse_time;       /* Print elapse time */
    bool disable_prompt;    /* Disable resource-query> prompt */
    bool flux_hwloc;        /* get hwloc info from flux instance */
    size_t reserve_vtx_vec; /* Allow for reserving vertex vector size */
};

class resource_query_t {
   public:
    resource_query_t ();
    resource_query_t (const std::string &rgraph, const std::string &options);
    ~resource_query_t ();

    /* Accessors */
    const std::string &get_resource_query_err_msg () const;
    const std::string &get_traverser_err_msg () const;
    const bool job_exists (const uint64_t jobid);
    const uint64_t get_job_counter () const;
    const std::shared_ptr<job_info_t> &get_job (const uint64_t jobid);
    const bool reservation_exists (const uint64_t jobid);
    const bool allocation_exists (const uint64_t jobid);
    const unsigned int preorder_count ();
    const unsigned int postorder_count ();

    /* Mutators */
    void clear_resource_query_err_msg ();
    void clear_traverser_err_msg ();
    void set_reservation (const uint64_t jobid);
    void erase_reservation (const uint64_t jobid);
    void set_allocation (const uint64_t jobid);
    void erase_allocation (const uint64_t jobid);
    void set_job (const uint64_t jobid, const std::shared_ptr<job_info_t> &job);
    int remove_job (const uint64_t jobid);
    int remove_job (const uint64_t jobid, const std::string &R, bool &full_removal);
    void incr_job_counter ();

    /* Run the traverser to match the jobspec */
    int traverser_run (Flux::Jobspec::Jobspec &job, match_op_t op, int64_t jobid, int64_t &at);
    int traverser_find (std::string criteria);

    // must be public; results in a deleted stringstream if converted to
    // a private member function
    std::shared_ptr<match_writers_t> writers; /* Vertex/Edge writers */

    // must be public for use resource query, will eventually be public
    // private:
    /************************************************************************
     *                                                                      *
     *        (eventually) Private Member Data                              *
     *                                                                      *
     ************************************************************************/

    std::string m_err_msg;                      /* class error message */
    resource_params_t params;                   /* Parameters for resource graph context */
    uint64_t jobid_counter;                     /* Hold the current jobid value */
    std::shared_ptr<dfu_match_cb_t> matcher;    /* Match callback object */
    std::shared_ptr<dfu_traverser_t> traverser; /* Graph traverser object */
    std::shared_ptr<resource_graph_db_t> db;    /* Resource graph data store */
    match_perf_t perf;                          /* Match performance stats */
    std::map<uint64_t, std::shared_ptr<job_info_t>> jobs; /* Jobs table */
    std::map<uint64_t, uint64_t> allocations;             /* Allocation table */
    std::map<uint64_t, uint64_t> reservations;            /* Reservation table */

    /************************************************************************
     *                                                                      *
     *                        Private Util API                              *
     *                                                                      *
     ************************************************************************/

    int subsystem_exist (const std::string_view &n);
    int set_subsystems_use (const std::string &n);
    int set_resource_ctx_params (const std::string &options);
};

class reapi_cli_t : public reapi_t {
   public:
    static int match_allocate (void *h,
                               match_op_t match_op,
                               const std::string &jobspec,
                               const uint64_t jobid,
                               bool &reserved,
                               std::string &R,
                               int64_t &at,
                               double &ov);
    static int match_allocate_multi (void *h,
                                     bool orelse_reserve,
                                     const char *jobs,
                                     queue_adapter_base_t *adapter);
    static int update_allocate (void *h,
                                const uint64_t jobid,
                                const std::string &R,
                                int64_t &at,
                                double &ov,
                                std::string &R_out);
    static int cancel (void *h, const uint64_t jobid, bool noent_ok);
    static int cancel (void *h,
                       const uint64_t jobid,
                       const std::string &R,
                       bool noent_ok,
                       bool &full_removal);
    static int find (void *h, std::string criteria, json_t *&o);
    static int info (void *h,
                     const uint64_t jobid,
                     std::string &mode,
                     bool &reserved,
                     int64_t &at,
                     double &ov);
    static int info (void *h, const uint64_t jobid, std::shared_ptr<job_info_t> &job);
    static unsigned int preorder_count (void *h);
    static unsigned int postorder_count (void *h);
    static int stat (void *h,
                     int64_t &V,
                     int64_t &E,
                     int64_t &J,
                     double &load,
                     double &min,
                     double &max,
                     double &avg);
    static const std::string &get_err_message ();
    static void clear_err_message ();

   private:
    static std::string m_err_msg;
};

}  // namespace detail
}  // namespace resource_model
}  // namespace Flux

#endif  // REAPI_MODULE_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

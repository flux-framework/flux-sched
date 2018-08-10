/*****************************************************************************\
 *  Copyright (c) 2014 Lawrence Livermore National Security, LLC.  Produced at
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

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iterator>
#include <cstdint>
#include <cerrno>
#include <vector>
#include <map>
#include <boost/algorithm/string.hpp>
#include "resource/schema/resource_graph.hpp"
#include "resource/generators/gen.hpp"
#include "resource/traversers/dfu.hpp"
#include "resource/jobinfo/jobinfo.hpp"
#include "resource/policies/dfu_match_policy_factory.hpp"

extern "C" {
#if HAVE_CONFIG_H
#include "config.h"
#endif
}

using namespace std;
using namespace Flux::resource_model;

/******************************************************************************
 *                                                                            *
 *                Resource Matching Service Module Context                    *
 *                                                                            *
 ******************************************************************************/

struct rmatch_args_t {
    string grug;
    string match_subsystems;
    string match_policy;
    string prune_filters;
    string R_format;
};

struct rmatch_ctx_t {
    rmatch_args_t args;          /* Module load options */
    dfu_match_cb_t *matcher;     /* Match callback object */
    dfu_traverser_t *traverser;  /* Graph traverser object */
    resource_graph_db_t db;      /* Resource graph data store */
    f_resource_graph_t *fgraph;  /* Graph filtered by subsystems to use */
    map<uint64_t, job_info_t *> jobs;     /* Jobs table */
    map<uint64_t, uint64_t> allocations;  /* Allocation table */
    map<uint64_t, uint64_t> reservations; /* Reservation table */
};


/******************************************************************************
 *                                                                            *
 *                          Request Handler Prototypes                        *
 *                                                                            *
 ******************************************************************************/

static void match_request_cb (flux_t *h, flux_msg_handler_t *w,
                              const flux_msg_t *msg, void *arg);


/******************************************************************************
 *                                                                            *
 *                   Module Initialization Routines                           *
 *                                                                            *
 ******************************************************************************/

static void freectx (void *arg)
{
    if (ctx) {
        delete ctx->matcher;
        delete ctx->traverser;
        delete ctx->fgraph;
        for (auto &kv : ctx->jobs) {
            delete kv.second;    /* job_info_t* type */
            ctx->jobs.erase (kv.first);
        }
        ctx->jobs.clear ();
        ctx->allocations.clear ();
        ctx->reservations.clear ();
        delete ctx;
    }
}

static void set_default_args (rmatch_args_t &args)
{
    args.grug = "none";
    args.match_subsystems = "CA";
    args.match_policy = "high";
    args.prune_filters = "core";
    args.R_format = "R_lite";
}

static rmatch_ctx_t *getctx (flux_t *h)
{
    rmatch_ctx_t *ctx = (rmatch_ctx_t *)flux_aux_get (h, "resource-query");
    if (!ctx) {
        ctx = new rmatch_ctx_t (sizeof (*ctx));
        set_default_args (ctx->args);
        ctx->matcher = create_match_cb (ctx->args.match_policy);
        ctx->traverser = new dfu_traverser_t ();
        ctx->fgraph = NULL; /* Cannot be allocated at this point */
    }
    return ctx;
}

static int process_args (int argc, char **argv, const rmatch_args_t &args)
{
    int rc = 0;
    string dflt = "";

    for (int i = 0; i < argc; i++) {
        if (!strncmp ("grug-conf=", argv[i], sizeof ("grug-conf"))) {
            opts.grug = strstr (argv[i], "=") + 1;
        } else if (!strncmp ("subsystems=", argv[i], sizeof ("subsystems"))) {
            dflt = opts.match_subsystems;
            opts.match_subsystems = strstr (argv[i], "=") + 1;
        } else if (!strncmp ("policy=", argv[i], sizeof ("policy"))) {
            dflt = opts.match_policy;
            opts.match_policy = strstr (argv[i], "=") + 1;
            if (!known_match_policy (opts.match_policy)) {
                flux_log (h, LOG_ERR,
                          "Unknown match policy (%s)! Use default (%s).",
                           opts.match_policy, dflt);
                opts.match_policy = dflt;
            }
        } else if (!strncmp ("filters=", argv[i], sizeof ("fileters"))) {
            dflt = opts.prune_filters;
            opts.prune_filters = strstr (argv[i], "=") + 1;
        } else if (!strncmp ("R-format=", argv[i], sizeof ("R-format"))) {
            dflt = opts.R_format;
            opts.R_format = strstr (argv[i], "=") + 1;
            if (!known_R_format (opts.R_format)) {
                flux_log (h, LOG_ERR,
                          "Unknown R format (%s)! Use default (%s).",
                           opts.R_format, dflt);
                opts.R_format = dflt;
            }
        } else {
            rc = -1;
            errno = EINVAL;
            goto done;
        }
    }

done:
    return rc;
}

static rmatch_ctx_t *init_module (flux_t *h, int argc, char **argv)
{
    rmatch_ctx_t *ctx = NULL;
    uint32_t rank = 1;

    if (!(ctx = getctx (h))) {
        flux_log (h, LOG_ERR, "can't allocate the context");
        goto done;
    }
    if (flux_get_rank (h, &rank)) {
        flux_log (h, LOG_ERR, "can't determine rank");
        goto done;
    }
    if (rank) {
        flux_log (h, LOG_ERR, "resource-query module must only run on rank 0");
        goto done;
    }
    if (process_args (argc, argv, &(ctx->args)) != 0) {
        flux_log (h, LOG_ERR, "can't process module args");
        goto done;
    }

done:
    return ctx;
}


/******************************************************************************
 *                                                                            *
 *              Resource Graph and Traverser Initialization                   *
 *                                                                            *
 ******************************************************************************/

static int populate_resource_db (rmatch_ctx_t &ctx)
{
    int rc = 0;
    resource_generator_t rgen;
    if (ctx->args.grug != "none") {
        if ((rc = rgen.read_graphml (ctx->args.grug, ctx->db)) != 0) {
            errno = EINVAL;
            rc = -1;
            flux_log (h, LOG_ERR, "error in generating resources");
            goto done;
        }
    } else {
        errno = ENOTSUP;
        rc = -1;
        flux_log (h, LOG_ERR, "hwloc reader not implemented");
        goto done;
    }

done:
    return rc;
}

static int select_subsystems (rmatch_ctx_t *ctx)
{
    /*
     * Format of match_subsystems
     * subsystem1[:relation1[:relation2...]],subsystem2[...
     */
    int rc = 0;
    stringstream ss (ctx->args.match_subsystems);
    subsystem_t subsystem;
    string token;

    while (getline (ss, token, ',')) {

        size_t found = token.find_first_of (":");

        if (found != std::string::npos) {
            subsystem = token;
            if (!ctx->resource_graph_db.known_subsystem (subsystem)) {
                rc = -1;
                errno = EINVAL;
                goto done;
            }
            ctx->matcher->add_subsystem (subsystem, "*");
        } else {
            subsystem = token.substr (0, found);
            if (!ctx->resource_graph_db.known_subsystem (subsystem)) {
                rc = -1;
                errno = EINVAL;
                goto done;
            }
            stringstream relations (token.substr (found+1, std::string::npos));
            string relation;
            while (getline (relations, relation, ':'))
                ctx->matcher->add_subsystem (subsystem, relation);
        }
    }

done:
    return rc;
}

static int init_resource_graph (rmatch_ctx_t *ctx)
{
    int rc = 0;
    if ((rc = populate_resource_db (ctx)) != 0) {
        flux_log (h, LOG_ERR, "can't populate graph resource database");
        goto done;
    }
    if ((rc = select_subsystems (ctx)) != 0) {
        flux_log (h, LOG_ERR, "error processing subsystems %s",
                  ctx->args.match_subsystems);
        goto done;
    }

    resource_graph_t &g = ctx->db.resource_graph;

    // Set vertex and edge maps
    vtx_infra_map_t vmap = get (&resource_pool_t::idata, g);
    edg_infra_map_t emap = get (&resource_relation_t::idata, g);

    // Set vertex and edge filters based on subsystems to use
    const multi_subsystemsS &filter = ctx->matcher->subsystemsS ();
    subsystem_selector_t<vtx_t, f_vtx_infra_map_t> vtxsel (vmap, filter);
    subsystem_selector_t<edg_t, f_edg_infra_map_t> edgsel (emap, filter);

    // Create a filtered graph based on the filters
    ctx->fgraph = new f_resource_graph_t (g, edgsel, vtxsel);

    // Set pruning filter type for scheduler driven aggregate update
    const string &dom = ctx->matcher->dom_subsystem ();
    // TODO: Hook this with ctx->args.prune_filters
    ctx->matcher->set_pruning_type (dom, ANY_RESOURCE_TYPE, "core");

    // Initialize the DFU traverser
    ctx->traverser->initialize (ctx->fgraph, &(ctx->db.roots), ctx->matcher);

done:
    return rc;
}


/******************************************************************************
 *                                                                            *
 *                        Request Handler Routines                            *
 *                                                                            *
 ******************************************************************************/

static double get_elapse_time (timeval &st, timeval &et)
{
    double ts1 = (double)st.tv_sec + (double)st.tv_usec/1000000.0f;
    double ts2 = (double)et.tv_sec + (double)et.tv_usec/1000000.0f;
    return ts2 - ts1;
}

static int track_schedule_info (rmatch_ctx_t *ctx, int64_t id, int64_t at,
                                stringstream &R, double elapse)
{
    job_state_t st = job_state_t::INIT;

    if (id < 0 || at < 0) {
        errno = EINVAL;
        return -1;
    }

    st = (at == 0)? job_state_t::ALLOCATED : job_state_t::RESERVED;
    ctx->jobs[id] = new job_info_t (id, st, at, "", jobspect_str, elapse);
    if (at == 0)
        ctx->allocations[id] = id;
    else
        ctx->reservations[id] = id;

    return 0;
}

static int run_match (rmatch_ctx_t *ctx, int64_t jobid, const char *cmd,
                      string jobspec_str, int64_t *at, stringstream &R)
{
    int rc = 0;
    double elapse = 0.0f;
    struct timeval start, end;

    gettimeofday (&start, NULL);

    if (strcmp ("allocate", cmd) == 0) {
        Flux::Jobspec::Jobspec spec {jobspec_str};
        if ((rc = ctx->traverser->run (job,
                                       match_op_t::MATCH_ALLOCATE,
                                       jobid,
                                       at,
                                       R)) != 0) {
            flux_log (h, LOG_INFO,
                      "allocate could not find matching resources for %lld",
                      (intmax_t)jobid);
            goto done;
        }
    } else if (strcmp ("allocate_orelse_reserve", cmd) == 0) {
        Flux::Jobspec::Jobspec spec {jobspec_str};
        if ((rc = ctx->traverser.run (job,
                                      match_op_t::MATCH_ALLOCATE_ORELSE_RESERVE,
                                      jobid,
                                      at,
                                      R)) != 0) {
            flux_log (h, LOG_INFO,
                      "allocate_orelse_reserve could not find matching "
                      "resources for jobid (%lld)", (intmax_t)jobid);
            goto done;
        }
    } else {
        rc = -1;
        errno = EINVAL;
        goto done;
    }

    gettimeofday (&end, NULL);
    elapse = get_elapse_time (start, end);

    if ((rc = track_schedule_info (ctx, jobid, at, R, elapse)) != 0) {
        flux_log (h, LOG_ERR, "failed to track the schedule for %lld",
                  (intmax_t)jobid);
        goto done;
    }

done:
    return rc;
}

static int run_remove (rmatch_ctx_t *ctx, int64_t jobid)
{
    int rc = 0;
    if ((rc = ctx->traverser.remove (jobid)) == 0) {
        if (ctx->jobs.find (jobid) != ctx->jobs.end ()) {
           job_info_t *info = ctx->jobs[jobid];
           info->state = job_state_t::CANCELLED;
        }
    } else {
        if (ctx->jobs.find (jobid) != ctx->jobs.end ()) {
           job_info_t *info = ctx->jobs[jobid];
           info->state = job_state_t::ERROR;
           flux_log (h, LOG_INFO, "could not remove jobid (%lld).",
                    (intmax_t)jobid);
        }
    }
    return rc;
}

static void match_request_cb (flux_t *h, flux_msg_handler_t *w,
                              const flux_msg_t *msg, void *arg)
{
    int64_t at = 0;
    uint32_t userid = 0;
    int64_t jobid = -1;
    const char *cmd = NULL;
    const char *js_str = NULL;
    stringstream R;
    rmatch_ctx_t *ctx = getctx ((flux_t *)arg);

    if (flux_msg_get_userid (msg, &userid) < 0)
        goto error;

    flux_log (h, LOG_INFO, "match requested by user (%u).", userid);

    if (flux_request_unpack (msg, NULL, "{s:s}", "cmd", &cmd) < 0)
        goto error;
    if (flux_request_unpack (msg, NULL, "{s:I}", "jobid", &jobid) < 0)
        goto error;
    if (flux_request_unpack (msg, NULL, "{s:s}", "jobspec", &js_str) < 0)
        goto error;
    if (run_match (ctx, jobid, cmd, js_str, &at, R) < 0) {
        flux_log (h, LOG_INFO, "could not resolve match %s for jobid (%lld).",
                  cmd, (intmax_t)jobid);
        goto error;
    }
    if (flux_repond_pack (h, msg, "{s:s}", "R", R.str ().c_str ()) < 0)
        flux_log_error (h, "%s", __FUNCTION__);

    flux_log (h, LOG_INFO, "match request succeeded.");
    return;

error:
    if (flux_respond (h, msg, errno, NULL) < 0)
        flux_log_error (h, "%s", __FUNCTION__);
}

static void cancel_request_cb (flux_t *h, flux_msg_handler_t *w,
                               const flux_msg_t *msg, void *arg)
{
    rmatch_ctx_t *ctx = getctx ((flux_t *)arg);
    uint32_t userid = 0;
    int64_t jobid = -1;

    if (flux_msg_get_userid (msg, &userid) < 0)
        goto error;

    flux_log (h, LOG_INFO, "cancel requested by user (%u).", userid);

    if (flux_request_unpack (msg, NULL, "{s:I}", "jobid", &jobid) < 0)
        goto error;

    if (ctx->allocations.find (jobid) != ctx->allocations.end ()) {
        run_remove (ctx, jobid);
        ctx->allocations.erase (jobid);
    } else if (ctx->reservations.find (jobid) != ctx->reservations.end ()) {
        run_remove (ctx, jobid);
        ctx->reservations.erase (jobid);
    } else {
        errno = ENOENT;
        flux_log (h, LOG_ERR, "cannot find jobid (%lld)", (intmax_t)jobid);
        goto error;
    }
    if (flux_repond_pack (h, msg, "{}") < 0)
        flux_log_error (h, "%s", __FUNCTION__);

    flux_log (h, LOG_INFO, "cancel request succeeded.");
    return;

error:
    if (flux_respond (h, msg, errno, NULL) < 0)
        flux_log_error (h, "%s", __FUNCTION__);
}

static void info_request_cb (flux_t *h, flux_msg_handler_t *w,
                             const flux_msg_t *msg, void *arg)
{
    rmatch_ctx_t *ctx = getctx ((flux_t *)arg);
    uint32_t userid = 0;
    int64_t jobid == -1;

    if (flux_msg_get_userid (msg, &userid) < 0)
        goto error;

    flux_log (h, LOG_INFO, "info requested by user (%u).", userid);

    if (flux_request_unpack (msg, NULL, "{s:I}", "jobid", &jobid) < 0)
        goto error;
    if (ctx->jobs.find (jobid) == ctx->jobs.end ()) {
        errno = ENOENT;
        flux_log (h, LOG_ERR, "cannot find jobid (%lld)", (intmax_t)jobid);
        goto error;
    }

    string mode;
    job_info_t *info = ctx->jobs[jobid];
    get_jobstate_str (info->state, mode);
    if (flux_repond_pack (h, msg, "{s:I s:s s:I s:f}",
                          "jobid", jobid, "mode", mode.c_str (),
                          "scheduled_at", info->scheduled_at,
                          "schedule_overhead", info->overhead) < 0)
        flux_log_error (h, "%s", __FUNCTION__);

    flux_log (h, LOG_INFO, "info request succeeded.");
    return;

error:
    if (flux_respond (h, msg, errno, NULL) < 0)
        flux_log_error (h, "%s", __FUNCTION__);
}


/******************************************************************************
 *                                                                            *
 *                               Module Main                                  *
 *                                                                            *
 ******************************************************************************/

static const struct flux_msg_handler_spec htab[] = {
    { FLUX_MSGTYPE_REQUEST, "resource-query.match",  match_request_cb, 0},
    { FLUX_MSGTYPE_REQUEST, "resource-query.cancel", cancel_request_cb, 0},
    { FLUX_MSGTYPE_REQUEST, "resource-query.info",   info_request_cb, 0},
    FLUX_MSGHANDLER_TABLE_END
};

extern "C" int mod_main (flux_t *h, int argc, char **argv)
{
    int rc = -1;
    rmatch_ctx_t *ctx = NULL;
    uint32_t rank = 1;

    if (!(ctx = init_module (h, argc, argv))) {
        flux_log (h, LOG_ERR, "can't initialize resource-query module");
        goto done;
    }
    flux_log (h, LOG_INFO, "resource-query module starting...")

    if ((rc = init_resource_graph (ctx)) != 0) {
        flux_log (h, LOG_ERR, "can't initialize resource graph database");
        goto done;
    }
    flux_log (h, LOG_INFO, "resource graph database loaded");

    if (flux_reactor_run (flux_get_reactor (h), 0) < 0) {
        flux_log (h, LOG_ERR, "flux_reactor_run: %s", strerror (errno));
        goto done;
    }

done:
    free (ctx);
    return rc;
}

MOD_NAME ("resource-query")

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

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
#include <sstream>
#include <cerrno>
#include <map>
#include <cinttypes>

extern "C" {
#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <flux/core.h>
#include <jansson.h>
#include "src/common/libutil/shortjansson.h"
}

#include "resource/schema/resource_graph.hpp"
#include "resource/generators/gen.hpp"
#include "resource/traversers/dfu.hpp"
#include "resource/jobinfo/jobinfo.hpp"
#include "resource/policies/dfu_match_policy_factory.hpp"

using namespace std;
using namespace Flux::resource_model;

/******************************************************************************
 *                                                                            *
 *                Resource Matching Service Module Context                    *
 *                                                                            *
 ******************************************************************************/

struct resource_args_t {
    string grug;
    string hwloc_xml;
    string match_subsystems;
    string match_policy;
    string prune_filters;
    string R_format;
};

struct resource_ctx_t {
    flux_t *h;                     /* Flux handle */
    flux_msg_handler_t **handlers; /* Message handlers */
    resource_args_t args;          /* Module load options */
    dfu_match_cb_t *matcher;       /* Match callback object */
    dfu_traverser_t *traverser;    /* Graph traverser object */
    resource_graph_db_t db;        /* Resource graph data store */
    f_resource_graph_t *fgraph;    /* Graph filtered by subsystems to use */
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

static void cancel_request_cb (flux_t *h, flux_msg_handler_t *w,
                               const flux_msg_t *msg, void *arg);

static void info_request_cb (flux_t *h, flux_msg_handler_t *w,
                             const flux_msg_t *msg, void *arg);

static void next_jobid_request_cb (flux_t *h, flux_msg_handler_t *w,
                                   const flux_msg_t *msg, void *arg);

static const struct flux_msg_handler_spec htab[] = {
    { FLUX_MSGTYPE_REQUEST, "resource.match", match_request_cb, 0},
    { FLUX_MSGTYPE_REQUEST, "resource.cancel", cancel_request_cb, 0},
    { FLUX_MSGTYPE_REQUEST, "resource.info", info_request_cb, 0},
    { FLUX_MSGTYPE_REQUEST, "resource.next_jobid", next_jobid_request_cb, 0},
    FLUX_MSGHANDLER_TABLE_END
};

/******************************************************************************
 *                                                                            *
 *                   Module Initialization Routines                           *
 *                                                                            *
 ******************************************************************************/

static void freectx (void *arg)
{
    resource_ctx_t *ctx = (resource_ctx_t *)arg;
    if (ctx) {
        flux_msg_handler_delvec (ctx->handlers);
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

static void set_default_args (resource_args_t &args)
{
    args.grug = "";
    args.hwloc_xml = "";
    args.match_subsystems = "containment";
    args.match_policy = "high";
    args.prune_filters = "ALL:core";
    args.R_format = "R_NATIVE";
}

static resource_ctx_t *getctx (flux_t *h)
{
    resource_ctx_t *ctx = (resource_ctx_t *)flux_aux_get (h, "resource");
    if (!ctx) {
        ctx = new (nothrow)resource_ctx_t;
        if (!ctx) {
            errno = ENOMEM;
            goto done;
        }
        ctx->h = h;
        ctx->handlers = NULL;
        set_default_args (ctx->args);
        ctx->matcher = create_match_cb (ctx->args.match_policy);
        ctx->traverser = new (nothrow)dfu_traverser_t ();
        if (!ctx->traverser) {
            errno = ENOMEM;
            goto done;
        }
        ctx->fgraph = NULL; /* Cannot be allocated at this point */
        flux_aux_set (h, "resource", ctx, freectx);
    }

done:
    return ctx;
}

static int process_args (resource_ctx_t *ctx, int argc, char **argv)
{
    int rc = 0;
    resource_args_t &args = ctx->args;
    string dflt = "";

    for (int i = 0; i < argc; i++) {
        if (!strncmp ("grug-conf=", argv[i], sizeof ("grug-conf"))) {
            args.grug = strstr (argv[i], "=") + 1;
        } else if (!strncmp ("hwloc-xml=", argv[i], sizeof ("hwloc-xml"))) {
            args.hwloc_xml = strstr (argv[i], "=") + 1;
        } else if (!strncmp ("subsystems=", argv[i], sizeof ("subsystems"))) {
            dflt = args.match_subsystems;
            args.match_subsystems = strstr (argv[i], "=") + 1;
        } else if (!strncmp ("policy=", argv[i], sizeof ("policy"))) {
            dflt = args.match_policy;
            args.match_policy = strstr (argv[i], "=") + 1;
            if (!known_match_policy (args.match_policy)) {
                flux_log (ctx->h, LOG_ERR,
                          "Unknown match policy (%s)! Use default (%s).",
                           args.match_policy.c_str (), dflt.c_str ());
                args.match_policy = dflt;
            }
        } else if (!strncmp ("prune-filters=", argv[i], sizeof ("prune-filters"))) {
            dflt = args.prune_filters;
            args.prune_filters = strstr (argv[i], "=") + 1;
        } else if (!strncmp ("R-format=", argv[i], sizeof ("R-format"))) {
            dflt = args.R_format;
            args.R_format = strstr (argv[i], "=") + 1;
            if (!known_R_format (args.R_format)) {
                flux_log (ctx->h, LOG_ERR,
                          "Unknown R format (%s)! Use default (%s).",
                           args.R_format.c_str (), dflt.c_str ());
                args.R_format = dflt;
            }
        } else {
            rc = -1;
            errno = EINVAL;
            flux_log (ctx->h, LOG_ERR, "Unknown option `%s'", argv[i]);
        }
    }

    return rc;
}

static resource_ctx_t *init_module (flux_t *h, int argc, char **argv)
{
    resource_ctx_t *ctx = NULL;
    uint32_t rank = 1;

    if (!(ctx = getctx (h))) {
        flux_log (h, LOG_ERR, "can't allocate the context");
        goto error;
    }
    if (flux_get_rank (h, &rank) < 0) {
        flux_log (h, LOG_ERR, "can't determine rank");
        goto error;
    }
    if (rank) {
        flux_log (h, LOG_ERR, "resource module must only run on rank 0");
        goto error;
    }
    process_args (ctx, argc, argv);
    if (flux_msg_handler_addvec (h, htab, (void *)h, &ctx->handlers) < 0) {
        flux_log (h, LOG_ERR, "error registering resource event handler");
        goto error;
    }
    return ctx;

error:
    freectx (ctx);
    return NULL;
}


/******************************************************************************
 *                                                                            *
 *              Resource Graph and Traverser Initialization                   *
 *                                                                            *
 ******************************************************************************/

/* Block until value of 'key' becomes non-NULL.
 * It is an EPROTO error if value is type other than json_type_string.
 * On success returns value, otherwise NULL with errno set.
 */
static json_t *get_string_blocking (flux_t *h, const char *key)
{
    char *json_str = NULL; /* initial value for watch */
    json_t *o = NULL;
    int saved_errno;

    if (flux_kvs_watch_once (h, key, &json_str) < 0) {
        saved_errno = errno;
        goto error;
    }

    if (!json_str || !(o = Jfromstr (json_str))
                  || !json_is_string (o)) {
        saved_errno = EPROTO;
        goto error;
    }
    free (json_str);
    return o;
error:
    free (json_str);
    Jput (o);
    errno = saved_errno;
    return NULL;
}


/*
 * Read the hwloc xml stored in Flux's KVS and populate the resource db
 *
 * \param rgen   resource generator
 * \param h      flux handle
 * \param db     graph database consisting of resource graph and various indices
 * \return       0 on success; non-zero integer on an error
 */
int read_flux_hwloc (resource_generator_t &rgen, flux_t *h, resource_graph_db_t &db)
{
    int rc = -1;
    uint32_t rank = 0, size = 0;

    if (flux_get_size (h, &size) == -1) {
        flux_log (h, LOG_ERR, "%s: error with flux_get_size", __FUNCTION__);
        return -1;
    }

    ggv_t cluster_vertex = rgen.create_cluster_vertex (db);

    for (rank=0; rank < size; rank++) {
        char k[64];
        int n = snprintf (k, sizeof (k), "resource.hwloc.xml.%" PRIu32 "", rank);
        if ((n < 0) || ((unsigned int) n > sizeof (k))) {
          errno = ENOMEM;
          return -1;
        }
        json_t *o = get_string_blocking (h, k);

        const char *hwloc_xml = json_string_value (o);
        rgen.read_ranked_hwloc_xml (hwloc_xml, rank, cluster_vertex, db);
        Jput (o);
    }

    return 0;
}

static int populate_resource_db (resource_ctx_t *ctx)
{
    int rc = 0;
    resource_generator_t rgen;
    // TODO: include rgen.err_message()
    if (ctx->args.grug != "") {
        if (ctx->args.hwloc_xml != "") {
            flux_log (ctx->h, LOG_WARNING, "multiple resource inputs provided, using grug");
        }
        if ((rc = rgen.read_graphml (ctx->args.grug, ctx->db)) != 0) {
            errno = EINVAL;
            rc = -1;
            flux_log (ctx->h, LOG_ERR, "error in generating resources");
            goto done;
        }
        flux_log (ctx->h, LOG_INFO, "loaded resources from grug");
    } else if (ctx->args.hwloc_xml != "") {
        if ( (rc = rgen.read_hwloc_xml_file (ctx->args.hwloc_xml.c_str(), ctx->db)) != 0) {
            errno = EINVAL;
            rc = -1;
            flux_log (ctx->h, LOG_ERR, "error in generating resources");
            goto done;
        }
        flux_log (ctx->h, LOG_INFO, "loaded resources from hwloc xml");
    } else {
        // gather hwloc from Flux's KVS
        if ( (rc = read_flux_hwloc (rgen, ctx->h, ctx->db)) != 0) {
            errno = EINVAL;
            rc = -1;
            flux_log (ctx->h, LOG_ERR, "error in generating resources");
            goto done;
        }
        flux_log (ctx->h, LOG_INFO, "loaded resources from the hwloc xml in the KVS");
    }

done:
    return rc;
}

static int select_subsystems (resource_ctx_t *ctx)
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
        if (found == std::string::npos) {
            subsystem = token;
            if (!ctx->db.known_subsystem (subsystem)) {
                rc = -1;
                errno = EINVAL;
                goto done;
            }
            ctx->matcher->add_subsystem (subsystem, "*");
        } else {
            subsystem = token.substr (0, found);
            if (!ctx->db.known_subsystem (subsystem)) {
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

static int init_resource_graph (resource_ctx_t *ctx)
{
    int rc = 0;

    if ((rc = populate_resource_db (ctx)) != 0) {
        flux_log (ctx->h, LOG_ERR, "can't populate graph resource database");
        return rc;
    }
    if ((rc = select_subsystems (ctx)) != 0) {
        flux_log (ctx->h, LOG_ERR, "error processing subsystems %s",
                  ctx->args.match_subsystems.c_str ());
        return rc;
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
    ctx->fgraph = new (nothrow)f_resource_graph_t (g, edgsel, vtxsel);
    if (!ctx->fgraph) {
        errno = ENOMEM;
        return -1;
     }

    if (ctx->matcher->set_pruning_types_w_spec (ctx->matcher->dom_subsystem (),
                                                ctx->args.prune_filters) < 0) {
        flux_log (ctx->h, LOG_ERR, "error setting pruning types with: %s",
                  ctx->args.prune_filters.c_str ());
        return -1;
    }

    // Initialize the DFU traverser
    ctx->traverser->initialize (ctx->fgraph, &(ctx->db.roots), ctx->matcher);
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

static inline string get_status_string (int64_t at)
{
    return (at == 0)? "ALLOCATED" : "RESERVED";
}

static int track_schedule_info (resource_ctx_t *ctx, int64_t id, int64_t at,
                                string &jspec, stringstream &R, double elapse)
{
    job_lifecycle_t state = job_lifecycle_t::INIT;

    if (id < 0 || at < 0) {
        errno = EINVAL;
        return -1;
    }

    state = (at == 0)? job_lifecycle_t::ALLOCATED : job_lifecycle_t::RESERVED;
    ctx->jobs[id] = new job_info_t (id, state, at, "", jspec, R.str (), elapse);
    if (at == 0)
        ctx->allocations[id] = id;
    else
        ctx->reservations[id] = id;

    return 0;
}

static int run_match (resource_ctx_t *ctx, int64_t jobid, const char *cmd,
                      string jstr, int64_t *at, double *ov, stringstream &R)
{
    int rc = 0;
    double elapse = 0.0f;
    struct timeval start;
    struct timeval end;
    dfu_traverser_t &tr = *(ctx->traverser);

    gettimeofday (&start, NULL);
    if (string ("allocate") == cmd) {
        Flux::Jobspec::Jobspec j {jstr};
        rc = tr.run (j, match_op_t::MATCH_ALLOCATE, jobid, at, R);
        if (rc != 0) {
            flux_log (ctx->h, LOG_INFO,
                      "%s can't find resources for %ld.", cmd, (intmax_t)jobid);
            goto done;
        }
    } else if (string ("allocate_orelse_reserve") == cmd) {
        Flux::Jobspec::Jobspec j {jstr};
        rc = tr.run (j, match_op_t::MATCH_ALLOCATE_ORELSE_RESERVE,jobid, at, R);
        if (rc != 0) {
            flux_log (ctx->h, LOG_INFO,
                      "%s can't find resources for %ld.", cmd, (intmax_t)jobid);
            goto done;
        }
    } else {
        rc = -1;
        errno = EINVAL;
        flux_log (ctx->h, LOG_ERR, "unknown cmd: %s", cmd);
        goto done;
    }
    gettimeofday (&end, NULL);
    *ov = get_elapse_time (start, end);

    if ((rc = track_schedule_info (ctx, jobid, *at, jstr, R, *ov)) != 0) {
        flux_log (ctx->h, LOG_ERR, "can't add info for %ld.", (intmax_t)jobid);
        goto done;
    }

done:
    return rc;
}

static inline bool is_existent_jobid (const resource_ctx_t *ctx, uint64_t jobid)
{
    return (ctx->jobs.find (jobid) != ctx->jobs.end ())? true : false;
}

static int run_remove (resource_ctx_t *ctx, int64_t jobid)
{
    int rc = 0;
    dfu_traverser_t &tr = *(ctx->traverser);
    if ((rc = tr.remove (jobid)) == 0) {
        if (is_existent_jobid (ctx, jobid)) {
           job_info_t *info = ctx->jobs[jobid];
           info->state = job_lifecycle_t::CANCELLED;
        }
    } else {
        if (is_existent_jobid (ctx, jobid)) {
           job_info_t *info = ctx->jobs[jobid];
           info->state = job_lifecycle_t::ERROR;
           flux_log (ctx->h, LOG_INFO, "can't remove %ld.", (intmax_t)jobid);
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
    double ov = 0.0f;
    string status = "";
    const char *cmd = NULL;
    const char *js_str = NULL;
    stringstream R;
    resource_ctx_t *ctx = getctx ((flux_t *)arg);

    if (flux_msg_get_userid (msg, &userid) < 0)
        goto error;

    flux_log (h, LOG_INFO, "match requested by user (%u).", userid);

    if (flux_request_unpack (msg, NULL, "{s:s s:I s:s}", "cmd", &cmd,
                             "jobid", &jobid, "jobspec", &js_str) < 0)
        goto error;

    if (is_existent_jobid (ctx, jobid)) {
        flux_log (h, LOG_INFO, "invalid jobid (%ld).", (intmax_t)jobid);
        errno = EINVAL;
        goto error;
    }

    if (run_match (ctx, jobid, cmd, js_str, &at, &ov, R) < 0) {
        flux_log (h, LOG_INFO, "could not resolve match %s for jobid (%ld).",
                  cmd, (intmax_t)jobid);
        flux_log (h, LOG_INFO, "error string: %s.", strerror (errno));
        goto error;
    }

    status = get_status_string (at);
    if (flux_respond_pack (h, msg, "{s:I s:s s:f s:s s:I}",
                                   "jobid", jobid,
                                   "status", status.c_str (),
                                   "overhead", ov,
                                   "R", R.str ().c_str (),
                                   "at", at) < 0)
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
    resource_ctx_t *ctx = getctx ((flux_t *)arg);
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
        flux_log (h, LOG_ERR, "cannot find jobid (%ld)", (intmax_t)jobid);
        goto error;
    }
    if (flux_respond_pack (h, msg, "{}") < 0)
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
    resource_ctx_t *ctx = getctx ((flux_t *)arg);
    uint32_t userid = 0;
    int64_t jobid = -1;
    job_info_t *info = NULL;
    string status = "";

    if (flux_msg_get_userid (msg, &userid) < 0)
        goto error;

    flux_log (h, LOG_INFO, "info requested by user (%u).", userid);

    if (flux_request_unpack (msg, NULL, "{s:I}", "jobid", &jobid) < 0)
        goto error;
    if (!is_existent_jobid (ctx, jobid)) {
        errno = ENOENT;
        flux_log (h, LOG_ERR, "cannot find jobid (%ld)", (intmax_t)jobid);
        goto error;
    }

    info = ctx->jobs[jobid];
    get_jobstate_str (info->state, status);
    if (flux_respond_pack (h, msg, "{s:I s:s s:I s:f}",
                                   "jobid", jobid,
                                   "status", status.c_str (),
                                   "at", info->scheduled_at,
                                   "overhead", info->overhead) < 0)
        flux_log_error (h, "%s", __FUNCTION__);

    flux_log (h, LOG_INFO, "info request succeeded.");
    return;

error:
    if (flux_respond (h, msg, errno, NULL) < 0)
        flux_log_error (h, "%s", __FUNCTION__);
}

static inline int64_t next_jobid (const std::map<uint64_t, job_info_t *> &m)
{
    int64_t jobid = -1;
    if (m.empty ())
        jobid = 0;
    else if (m.rbegin ()->first < INT64_MAX)
        jobid = m.rbegin ()->first + 1;
    return jobid;
}

/* Needed for testing only */
static void next_jobid_request_cb (flux_t *h, flux_msg_handler_t *w,
                                   const flux_msg_t *msg, void *arg)
{
    resource_ctx_t *ctx = getctx ((flux_t *)arg);
    uint32_t userid = 0;
    int64_t jobid = -1;

    if (flux_msg_get_userid (msg, &userid) < 0)
        goto error;

    flux_log (h, LOG_INFO, "next jobid requested by user (%u).", userid);

    if ((jobid = next_jobid (ctx->jobs)) < 0) {
        errno = ERANGE;
        goto error;
    }

    if (flux_respond_pack (h, msg, "{s:I}", "jobid", jobid) < 0)
        flux_log_error (h, "%s", __FUNCTION__);

    flux_log (h, LOG_INFO, "next_jobid request succeeded.");
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

extern "C" int mod_main (flux_t *h, int argc, char **argv)
{
    int rc = -1;
    resource_ctx_t *ctx = NULL;
    uint32_t rank = 1;

    if (!(ctx = init_module (h, argc, argv))) {
        flux_log (h, LOG_ERR, "can't initialize resource module");
        goto done;
    }
    flux_log (h, LOG_INFO, "resource module starting...");

    if ((rc = init_resource_graph (ctx)) != 0) {
        flux_log (h, LOG_ERR, "can't initialize resource graph database");
        goto done;
    }
    flux_log (h, LOG_INFO, "resource graph database loaded");

    if ((rc = flux_reactor_run (flux_get_reactor (h), 0)) < 0) {
        flux_log (h, LOG_ERR, "flux_reactor_run: %s", strerror (errno));
        goto done;
    }

done:
    return rc;
}

MOD_NAME ("resource");

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

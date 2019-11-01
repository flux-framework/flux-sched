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

#include <cerrno>
#include <cinttypes>
#include <cstdint>
#include <limits>
#include <map>
#include <sstream>
#include <vector>

extern "C" {
#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <flux/core.h>
#include <jansson.h>
#include "src/common/libutil/shortjansson.h"
}

#include "src/common/libutil/flux.hpp"

#include "resource/jobinfo/jobinfo.hpp"
#include "resource/policies/dfu_match_policy_factory.hpp"
#include "resource/readers/resource_reader_factory.hpp"
#include "resource/schema/resource_graph.hpp"
#include "resource/traversers/dfu.hpp"

using namespace std;
using namespace Flux::resource_model;

/******************************************************************************
 *                                                                            *
 *                Resource Matching Service Module Context                    *
 *                                                                            *
 ******************************************************************************/

struct resource_args_t {
    string load_file{""};        /* load file name */
    string load_format{"hwloc"}; /* load reader format */
    string load_whitelist{""};   /* load resource whitelist */
    string match_subsystems{"containment"};
    string match_policy{"high"};
    string prune_filters{"ALL:core"};
    string match_format{"rv1_nosched"};
    int reserve_vtx_vec{0}; /* Allow for reserving vertex vector size */
};

struct match_perf_t {
    double load = 0;                                  /* Graph load time */
    uint64_t njobs = 0;                               /* Total match count */
    double min = std::numeric_limits<double>::max (); /* Min match time */
    double max = 0;                                   /* Max match time */
    double accum = 0;                                 /* Total match time accumulated */
};

struct resource_ctx_t {
    resource_ctx_t (flux_t *h_in) : h (h_in)
    {
    }
    ~resource_ctx_t ()
    {
        delete this->matcher;
        delete this->fgraph;
        for (auto &kv : this->jobs) {
            delete kv.second; /* job_info_t* type */
            this->jobs.erase (kv.first);
        }
        delete this->writers;
        this->jobs.clear ();
        this->allocations.clear ();
        this->reservations.clear ();
    }

    flux_t *h{nullptr};               /* Flux handle */
    resource_args_t args{};           /* Module load options */
    dfu_match_cb_t *matcher{nullptr}; /* Match callback object */
    std::unique_ptr<dfu_traverser_t> traverser{
        new dfu_traverser_t ()};          /* Graph traverser object */
    resource_graph_db_t db;               /* Resource graph data store */
    f_resource_graph_t *fgraph{nullptr};  /* Graph filtered by subsystems to use */
    match_writers_t *writers{nullptr};    /* Vertex/Edge writers for a match */
    match_perf_t perf{};                  /* Match performance stats */
    map<uint64_t, job_info_t *> jobs;     /* Jobs table */
    map<uint64_t, uint64_t> allocations;  /* Allocation table */
    map<uint64_t, uint64_t> reservations; /* Reservation table */
};

/******************************************************************************
 *                                                                            *
 *                          Request Handler Prototypes                        *
 *                                                                            *
 ******************************************************************************/

static void match_request_cb (flux_t *h,
                              flux_msg_handler_t *w,
                              const flux_msg_t *msg,
                              void *arg);

static void cancel_request_cb (flux_t *h,
                               flux_msg_handler_t *w,
                               const flux_msg_t *msg,
                               void *arg);

static void info_request_cb (flux_t *h,
                             flux_msg_handler_t *w,
                             const flux_msg_t *msg,
                             void *arg);

static void stat_request_cb (flux_t *h,
                             flux_msg_handler_t *w,
                             const flux_msg_t *msg,
                             void *arg);

static void next_jobid_request_cb (flux_t *h,
                                   flux_msg_handler_t *w,
                                   const flux_msg_t *msg,
                                   void *arg);

static void set_property_request_cb (flux_t *h,
                                     flux_msg_handler_t *w,
                                     const flux_msg_t *msg,
                                     void *arg);

static void get_property_request_cb (flux_t *h,
                                     flux_msg_handler_t *w,
                                     const flux_msg_t *msg,
                                     void *arg);

// static const struct flux_msg_handler_spec htab[] = {
//     { FLUX_MSGTYPE_REQUEST, "resource.match", match_request_cb, 0},
//     { FLUX_MSGTYPE_REQUEST, "resource.cancel", cancel_request_cb, 0},
//     { FLUX_MSGTYPE_REQUEST, "resource.info", info_request_cb, 0},
//     { FLUX_MSGTYPE_REQUEST, "resource.stat", stat_request_cb, 0},
//     { FLUX_MSGTYPE_REQUEST, "resource.next_jobid", next_jobid_request_cb, 0},
//     { FLUX_MSGTYPE_REQUEST, "resource.set_property", set_property_request_cb,
//     0}, { FLUX_MSGTYPE_REQUEST, "resource.get_property",
//     get_property_request_cb, 0}, FLUX_MSGHANDLER_TABLE_END
// };

static double get_elapse_time (timeval &st, timeval &et)
{
    double ts1 = (double)st.tv_sec + (double)st.tv_usec / 1000000.0f;
    double ts2 = (double)et.tv_sec + (double)et.tv_usec / 1000000.0f;
    return ts2 - ts1;
}

/******************************************************************************
 *                                                                            *
 *                   Module Initialization Routines                           *
 *                                                                            *
 ******************************************************************************/

static resource_ctx_t *getctx (void *arg)
{
    return reinterpret_cast<resource_ctx_t *> (arg);
}

// in stl in 20, *sigh*
inline bool str_starts_with (std::string const &s, const char *o)
{
    return s.rfind (o, 0) == 0;
}
inline std::string substr_after_eq (std::string const &s)
{
    return s.substr (s.find_first_of ("=") + 1);
}

static int process_args (resource_ctx_t *ctx, std::vector<std::string> const &av)
{
    int rc = 0;
    resource_args_t &args = ctx->args;
    string dflt = "";

    for (auto const &a : av) {
        flux_log_error (ctx->h, "got arg %s", a.c_str ());
        if (str_starts_with (a, "load-file=")) {
            args.load_file = substr_after_eq (a);
        } else if (str_starts_with (a, "load-format=")) {
            dflt = args.load_format;
            args.load_format = substr_after_eq (a);
            if (!known_resource_reader (args.load_format)) {
                flux_log (ctx->h,
                          LOG_ERR,
                          "unknown resource reader (%s)! Use default (%s).",
                          args.load_format.c_str (),
                          dflt.c_str ());
                args.load_format = dflt;
            }
            args.load_format = substr_after_eq (a);
        } else if (str_starts_with (a, "load-whitelist=")) {
            args.load_whitelist = substr_after_eq (a);
        } else if (str_starts_with (a, "subsystems=")) {
            dflt = args.match_subsystems;
            args.match_subsystems = substr_after_eq (a);
        } else if (str_starts_with (a, "policy=")) {
            dflt = args.match_policy;
            args.match_policy = substr_after_eq (a);
            if (!known_match_policy (args.match_policy)) {
                flux_log (ctx->h,
                          LOG_ERR,
                          "Unknown match policy (%s)! Use default (%s).",
                          args.match_policy.c_str (),
                          dflt.c_str ());
                args.match_policy = dflt;
            }
        } else if (str_starts_with (a, "prune-filters=")) {
            std::string token = substr_after_eq (a);
            if (token.find_first_not_of (' ') != std::string::npos) {
                args.prune_filters += ",";
                args.prune_filters += token;
            }
        } else if (str_starts_with (a, "match-format=")) {
            dflt = args.match_format;
            args.match_format = substr_after_eq (a);
            if (!known_match_format (args.match_format)) {
                args.match_format = dflt;
                flux_log (ctx->h,
                          LOG_ERR,
                          "Unknown match format (%s)! Use default (%s).",
                          args.match_format.c_str (),
                          dflt.c_str ());
                args.match_format = dflt;
            }
        } else if (str_starts_with (a, "reserve-vtx-vec=")) {
            args.reserve_vtx_vec = std::stoi (substr_after_eq (a));
            if (args.reserve_vtx_vec <= 0 || args.reserve_vtx_vec > 2000000) {
                flux_log (ctx->h,
                          LOG_ERR,
                          "out of range specified for reserve-vtx-vec (%d)",
                          args.reserve_vtx_vec);
                args.reserve_vtx_vec = 0;
            }
        } else {
            rc = -1;
            errno = EINVAL;
            flux_log (ctx->h, LOG_ERR, "Unknown option `%s'", a.c_str ());
        }
    }

    return rc;
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
    flux_future_t *f = NULL;
    const char *json_str;
    json_t *o = NULL;
    int saved_errno;

    if (!(f = flux_kvs_lookup (h, NULL, FLUX_KVS_WAITCREATE, key))) {
        saved_errno = errno;
        goto error;
    }

    if (flux_kvs_lookup_get (f, &json_str) < 0) {
        saved_errno = errno;
        goto error;
    }

    if (!json_str || !(o = Jfromstr (json_str)) || !json_is_string (o)) {
        saved_errno = EPROTO;
        goto error;
    }

    flux_future_destroy (f);
    return o;
error:
    flux_future_destroy (f);
    Jput (o);
    errno = saved_errno;
    return NULL;
}

static int populate_resource_db_file (resource_ctx_t *ctx,
                                      std::shared_ptr<resource_reader_base_t> rd)
{
    int rc = -1;
    ifstream in_file;
    std::stringstream buffer{};

    in_file.open (ctx->args.load_file.c_str (), std::ifstream::in);
    if (!in_file.good ()) {
        errno = EIO;
        flux_log (ctx->h, LOG_ERR, "opening %s", ctx->args.load_file.c_str ());
        goto done;
    }
    buffer << in_file.rdbuf ();
    in_file.close ();
    if ((rc = ctx->db.load (buffer.str (), rd)) < 0) {
        flux_log (ctx->h, LOG_ERR, "reader: %s", rd->err_message ().c_str ());
        goto done;
    }
    rc = 0;

done:
    return rc;
}

static int populate_resource_db_kvs (resource_ctx_t *ctx,
                                     std::shared_ptr<resource_reader_base_t> rd)
{
    int n = -1;
    int rc = -1;
    char k[64] = {0};
    uint32_t rank = 0;
    uint32_t size = 0;
    json_t *o = NULL;
    flux_t *h = ctx->h;
    const char *hwloc_xml = NULL;
    resource_graph_db_t &db = ctx->db;
    vtx_t v = boost::graph_traits<resource_graph_t>::null_vertex ();

    if (flux_get_size (h, &size) == -1) {
        flux_log (h, LOG_ERR, "%s: flux_get_size", __FUNCTION__);
        goto done;
    }

    // For 0th rank -- special case to use rd->unpack
    rank = 0;
    n = snprintf (k, sizeof (k), "resource.hwloc.xml.%" PRIu32 "", rank);
    if ((n < 0) || ((unsigned int)n > sizeof (k))) {
        errno = ENOMEM;
        goto done;
    }
    o = get_string_blocking (h, k);
    hwloc_xml = json_string_value (o);
    if ((rc = ctx->db.load (hwloc_xml, rd, rank)) < 0) {
        flux_log (ctx->h, LOG_ERR, "reader: %s", rd->err_message ().c_str ());
        goto done;
    }
    Jput (o);
    if (db.metadata.roots.find ("containment") == db.metadata.roots.end ()) {
        flux_log (ctx->h, LOG_ERR, "cluster vertex is unavailable");
        goto done;
    }
    v = db.metadata.roots["containment"];

    // For the rest of the ranks -- general case
    for (rank = 1; rank < size; rank++) {
        n = snprintf (k, sizeof (k), "resource.hwloc.xml.%" PRIu32 "", rank);
        if ((n < 0) || ((unsigned int)n > sizeof (k))) {
            errno = ENOMEM;
            goto done;
        }
        o = get_string_blocking (h, k);
        hwloc_xml = json_string_value (o);
        if ((rc = ctx->db.load (hwloc_xml, rd, v, rank)) < 0) {
            flux_log (ctx->h, LOG_ERR, "reader: %s", rd->err_message ().c_str ());
            goto done;
        }
        Jput (o);
    }
    rc = 0;

done:
    return rc;
}

static int populate_resource_db (resource_ctx_t *ctx)
{
    int rc = -1;
    double elapse;
    struct timeval st, et;
    std::shared_ptr<resource_reader_base_t> rd;

    if (ctx->args.reserve_vtx_vec != 0)
        ctx->db.resource_graph.m_vertices.reserve (ctx->args.reserve_vtx_vec);
    if ((rd = create_resource_reader (ctx->args.load_format)) == nullptr) {
        flux_log (ctx->h, LOG_ERR, "Can't create load reader");
        goto done;
    }
    if (ctx->args.load_whitelist != "") {
        if (rd->set_whitelist (ctx->args.load_whitelist) < 0)
            flux_log (ctx->h, LOG_ERR, "setting whitelist");
        if (!rd->is_whitelist_supported ())
            flux_log (ctx->h, LOG_WARNING, "whitelist unsupported");
    }

    gettimeofday (&st, NULL);
    if (ctx->args.load_file != "") {
        if (populate_resource_db_file (ctx, rd) < 0) {
            flux_log (ctx->h, LOG_ERR, "error loading resources from file");
            goto done;
        }
        flux_log (ctx->h,
                  LOG_INFO,
                  "loaded resources from %s",
                  ctx->args.load_file.c_str ());
    } else {
        if (populate_resource_db_kvs (ctx, rd) < 0) {
            flux_log (ctx->h, LOG_ERR, "loading resources from the KVS");
            goto done;
        }
        flux_log (ctx->h, LOG_INFO, "loaded resources from hwloc in the KVS");
    }
    gettimeofday (&et, NULL);
    ctx->perf.load = get_elapse_time (st, et);
    rc = 0;

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
            stringstream relations (token.substr (found + 1, std::string::npos));
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

    // Select the appropriate matcher based on CLI policy.
    ctx->matcher = create_match_cb (ctx->args.match_policy);

    if ((rc = populate_resource_db (ctx)) != 0) {
        flux_log (ctx->h, LOG_ERR, "can't populate graph resource database");
        return rc;
    }
    if ((rc = select_subsystems (ctx)) != 0) {
        flux_log (ctx->h,
                  LOG_ERR,
                  "error processing subsystems %s",
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
    if (!(ctx->fgraph = new (nothrow) f_resource_graph_t (g, edgsel, vtxsel))) {
        errno = ENOMEM;
        return -1;
    }

    // Create a writers object for matched vertices and edges
    match_format_t format =
        match_writers_factory_t::get_writers_type (ctx->args.match_format);
    if (!(ctx->writers = match_writers_factory_t::create (format)))
        return -1;

    if (ctx->args.prune_filters != ""
        && ctx->matcher->set_pruning_types_w_spec (ctx->matcher->dom_subsystem (),
                                                   ctx->args.prune_filters)
               < 0) {
        flux_log (ctx->h,
                  LOG_ERR,
                  "error setting pruning types with: %s",
                  ctx->args.prune_filters.c_str ());
        return -1;
    }

    // Initialize the DFU traverser
    if (ctx->traverser->initialize (ctx->fgraph,
                                    &(ctx->db.metadata.roots),
                                    ctx->matcher)
        < 0) {
        flux_log (ctx->h, LOG_ERR, "traverser initialization");
        return -1;
    }
    return 0;
}

/******************************************************************************
 *                                                                            *
 *                        Request Handler Routines                            *
 *                                                                            *
 ******************************************************************************/

static void update_match_perf (resource_ctx_t *ctx, double elapse)
{
    ctx->perf.njobs++;
    ctx->perf.min = (ctx->perf.min > elapse) ? elapse : ctx->perf.min;
    ctx->perf.max = (ctx->perf.max < elapse) ? elapse : ctx->perf.max;
    ctx->perf.accum += elapse;
}

static inline string get_status_string (int64_t now, int64_t at)
{
    return (at == now) ? "ALLOCATED" : "RESERVED";
}

static int track_schedule_info (resource_ctx_t *ctx,
                                int64_t id,
                                int64_t now,
                                int64_t at,
                                string &jspec,
                                stringstream &R,
                                double elapse)
{
    job_lifecycle_t state = job_lifecycle_t::INIT;

    if (id < 0 || now < 0 || at < 0) {
        errno = EINVAL;
        return -1;
    }

    state = (at == now) ? job_lifecycle_t::ALLOCATED : job_lifecycle_t::RESERVED;
    if (!(ctx->jobs[id] = new ((nothrow))
              job_info_t (id, state, at, "", jspec, R.str (), elapse))) {
        errno = ENOMEM;
        return -1;
    }

    if (at == now)
        ctx->allocations[id] = id;
    else
        ctx->reservations[id] = id;

    return 0;
}

static int run (resource_ctx_t *ctx,
                int64_t jobid,
                const char *cmd,
                string jstr,
                int64_t *at)
{
    int rc = 0;
    Flux::Jobspec::Jobspec j{jstr};
    dfu_traverser_t &tr = *(ctx->traverser);

    if (string ("allocate") == cmd)
        rc = tr.run (j, ctx->writers, match_op_t::MATCH_ALLOCATE, jobid, at);
    else if (string ("allocate_with_satisfiability") == cmd)
        rc = tr.run (j,
                     ctx->writers,
                     match_op_t::MATCH_ALLOCATE_W_SATISFIABILITY,
                     jobid,
                     at);
    else if (string ("allocate_orelse_reserve") == cmd)
        rc = tr.run (j,
                     ctx->writers,
                     match_op_t::MATCH_ALLOCATE_ORELSE_RESERVE,
                     jobid,
                     at);
    return rc;
}

static int run_match (resource_ctx_t *ctx,
                      int64_t jobid,
                      const char *cmd,
                      string jstr,
                      int64_t *now,
                      int64_t *at,
                      double *ov,
                      stringstream &o)
{
    int rc = 0;
    double elapse = 0.0f;
    struct timeval start;
    struct timeval end;

    gettimeofday (&start, NULL);
    ctx->writers->reset ();

    if (strcmp ("allocate", cmd) != 0 && strcmp ("allocate_orelse_reserve", cmd) != 0
        && strcmp ("allocate_with_satisfiability", cmd) != 0) {
        errno = EINVAL;
        flux_log_error (ctx->h, "unknown cmd: %s", cmd);
        goto done;
    }

    *at = *now = (int64_t)start.tv_sec;
    if ((rc = run (ctx, jobid, cmd, jstr, at)) < 0)
        goto done;

    ctx->writers->emit (o);
    gettimeofday (&end, NULL);
    *ov = get_elapse_time (start, end);
    update_match_perf (ctx, *ov);

    if ((rc = track_schedule_info (ctx, jobid, *now, *at, jstr, o, *ov)) != 0) {
        errno = EINVAL;
        flux_log_error (ctx->h, "can't add job info (id=%jd)", (intmax_t)jobid);
        goto done;
    }

done:
    return rc;
}

static inline bool is_existent_jobid (const resource_ctx_t *ctx, uint64_t jobid)
{
    return (ctx->jobs.find (jobid) != ctx->jobs.end ()) ? true : false;
}

static int run_remove (resource_ctx_t *ctx, int64_t jobid)
{
    int rc = -1;
    dfu_traverser_t &tr = *(ctx->traverser);

    if ((rc = tr.remove (jobid)) < 0) {
        if (is_existent_jobid (ctx, jobid)) {
            // When this condition arises, we will be less likely
            // to be able to reuse this jobid. Having the errored job
            // in the jobs map will prevent us from reusing the jobid
            // up front.  Note that a same jobid can be reserved and
            // removed multiple times by the upper queuing layer
            // as part of providing advanced queueing policies
            // (e.g., conservative backfill).
            job_info_t *info = ctx->jobs[jobid];
            info->state = job_lifecycle_t::ERROR;
        }
        goto out;
    }
    if (is_existent_jobid (ctx, jobid))
        ctx->jobs.erase (jobid);

    rc = 0;
out:
    return rc;
}

static void match_request_cb (flux_t *h,
                              flux_msg_handler_t *w,
                              const flux_msg_t *msg,
                              void *arg)
{
    int64_t at = 0;
    int64_t now = 0;
    int64_t jobid = -1;
    double ov = 0.0f;
    string status = "";
    const char *cmd = NULL;
    const char *js_str = NULL;
    stringstream R;

    resource_ctx_t *ctx = getctx ((flux_t *)arg);
    if (flux_request_unpack (msg,
                             NULL,
                             "{s:s s:I s:s}",
                             "cmd",
                             &cmd,
                             "jobid",
                             &jobid,
                             "jobspec",
                             &js_str)
        < 0)
        goto error;
    if (is_existent_jobid (ctx, jobid)) {
        errno = EINVAL;
        flux_log_error (h, "existent job (%jd).", (intmax_t)jobid);
        goto error;
    }
    if (run_match (ctx, jobid, cmd, js_str, &now, &at, &ov, R) < 0) {
        if (errno != EBUSY && errno != ENODEV)
            flux_log_error (ctx->h,
                            "match failed due to match error (id=%jd)",
                            (intmax_t)jobid);
        goto error;
    }

    status = get_status_string (now, at);
    if (flux_respond_pack (h,
                           msg,
                           "{s:I s:s s:f s:s s:I}",
                           "jobid",
                           jobid,
                           "status",
                           status.c_str (),
                           "overhead",
                           ov,
                           "R",
                           R.str ().c_str (),
                           "at",
                           at)
        < 0)
        flux_log_error (h, "%s", __FUNCTION__);

    return;

error:
    if (flux_respond_error (h, msg, errno, NULL) < 0)
        flux_log_error (h, "%s: flux_respond_error", __FUNCTION__);
}

static void cancel_request_cb (flux_t *h,
                               flux_msg_handler_t *w,
                               const flux_msg_t *msg,
                               void *arg)
{
    resource_ctx_t *ctx = getctx ((flux_t *)arg);
    int64_t jobid = -1;

    if (flux_request_unpack (msg, NULL, "{s:I}", "jobid", &jobid) < 0)
        goto error;
    if (ctx->allocations.find (jobid) != ctx->allocations.end ())
        ctx->allocations.erase (jobid);
    else if (ctx->reservations.find (jobid) != ctx->reservations.end ())
        ctx->reservations.erase (jobid);
    else {
        errno = ENOENT;
        flux_log (h, LOG_DEBUG, "nonexistent job (id=%jd)", (intmax_t)jobid);
        goto error;
    }

    if (run_remove (ctx, jobid) < 0) {
        flux_log_error (h, "remove fails due to match error (id=%jd)", (intmax_t)jobid);
        goto error;
    }
    if (flux_respond_pack (h, msg, "{}") < 0)
        flux_log_error (h, "%s", __FUNCTION__);

    return;

error:
    if (flux_respond_error (h, msg, errno, NULL) < 0)
        flux_log_error (h, "%s: flux_respond_error", __FUNCTION__);
}

static void info_request_cb (flux_t *h,
                             flux_msg_handler_t *w,
                             const flux_msg_t *msg,
                             void *arg)
{
    resource_ctx_t *ctx = getctx ((flux_t *)arg);
    int64_t jobid = -1;
    job_info_t *info = NULL;
    string status = "";

    if (flux_request_unpack (msg, NULL, "{s:I}", "jobid", &jobid) < 0)
        goto error;
    if (!is_existent_jobid (ctx, jobid)) {
        errno = ENOENT;
        flux_log (h, LOG_DEBUG, "nonexistent job (id=%jd)", (intmax_t)jobid);
        goto error;
    }

    info = ctx->jobs[jobid];
    get_jobstate_str (info->state, status);
    if (flux_respond_pack (h,
                           msg,
                           "{s:I s:s s:I s:f}",
                           "jobid",
                           jobid,
                           "status",
                           status.c_str (),
                           "at",
                           info->scheduled_at,
                           "overhead",
                           info->overhead)
        < 0)
        flux_log_error (h, "%s", __FUNCTION__);

    return;

error:
    if (flux_respond_error (h, msg, errno, NULL) < 0)
        flux_log_error (h, "%s: flux_respond_error", __FUNCTION__);
}

static void stat_request_cb (flux_t *h,
                             flux_msg_handler_t *w,
                             const flux_msg_t *msg,
                             void *arg)
{
    resource_ctx_t *ctx = getctx ((flux_t *)arg);
    double avg = 0.0f;
    double min = 0.0f;

    if (ctx->perf.njobs) {
        avg = ctx->perf.accum / (double)ctx->perf.njobs;
        min = ctx->perf.min;
    }
    if (flux_respond_pack (h,
                           msg,
                           "{s:I s:I s:f s:I s:f s:f s:f}",
                           "V",
                           num_vertices (ctx->db.resource_graph),
                           "E",
                           num_edges (ctx->db.resource_graph),
                           "load-time",
                           ctx->perf.load,
                           "njobs",
                           ctx->perf.njobs,
                           "min-match",
                           min,
                           "max-match",
                           ctx->perf.max,
                           "avg-match",
                           avg)
        < 0)
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
static void next_jobid_request_cb (flux_t *h,
                                   flux_msg_handler_t *w,
                                   const flux_msg_t *msg,
                                   void *arg)
{
    resource_ctx_t *ctx = getctx ((flux_t *)arg);
    int64_t jobid = -1;

    if ((jobid = next_jobid (ctx->jobs)) < 0) {
        errno = ERANGE;
        goto error;
    }
    if (flux_respond_pack (h, msg, "{s:I}", "jobid", jobid) < 0)
        flux_log_error (h, "%s", __FUNCTION__);

    return;

error:
    if (flux_respond_error (h, msg, errno, NULL) < 0)
        flux_log_error (h, "%s: flux_respond_error", __FUNCTION__);
}

static void set_property_request_cb (flux_t *h,
                                     flux_msg_handler_t *w,
                                     const flux_msg_t *msg,
                                     void *arg)
{
    const char *rp = NULL, *kv = NULL;
    string resource_path = "", keyval = "";
    string property_key = "", property_value = "";
    size_t pos;
    resource_ctx_t *ctx = getctx ((flux_t *)arg);
    std::map<std::string, vtx_t>::const_iterator it;
    std::pair<std::map<std::string, std::string>::iterator, bool> ret;
    vtx_t v;

    if (flux_request_unpack (msg,
                             NULL,
                             "{s:s s:s}",
                             "sp_resource_path",
                             &rp,
                             "sp_keyval",
                             &kv)
        < 0)
        goto error;

    resource_path = rp;
    keyval = kv;

    pos = keyval.find ('=');

    if (pos == 0 || (pos == keyval.size () - 1) || pos == string::npos) {
        errno = EINVAL;
        flux_log_error (h,
                        "Incorrect format.\nUse set-property <resource> "
                        "PROPERTY=VALUE");
        goto error;
    }

    property_key = keyval.substr (0, pos);
    property_value = keyval.substr (pos + 1);

    it = ctx->db.metadata.by_path.find (resource_path);

    if (it == ctx->db.metadata.by_path.end ()) {
        errno = ENOENT;
        flux_log_error (h,
                        "Couldn't find %s in resource graph.",
                        resource_path.c_str ());
        goto error;
    }

    v = it->second;

    ret = ctx->db.resource_graph[v].properties.insert (
        std::pair<std::string, std::string> (property_key, property_value));

    if (ret.second == false) {
        ctx->db.resource_graph[v].properties.erase (property_key);
        ctx->db.resource_graph[v].properties.insert (
            std::pair<std::string, std::string> (property_key, property_value));
    }

    if (flux_respond_pack (h, msg, "{}") < 0)
        flux_log_error (h, "%s", __FUNCTION__);

    return;

error:
    if (flux_respond_error (h, msg, errno, NULL) < 0)
        flux_log_error (h, "%s: flux_respond_error", __FUNCTION__);
}

static void get_property_request_cb (flux_t *h,
                                     flux_msg_handler_t *w,
                                     const flux_msg_t *msg,
                                     void *arg)
{
    const char *rp = NULL, *gp_key = NULL;
    string resource_path = "", property_key = "";
    resource_ctx_t *ctx = getctx ((flux_t *)arg);
    std::map<std::string, vtx_t>::const_iterator it;
    std::map<std::string, std::string>::const_iterator p_it;
    vtx_t v;
    string resp_value = "";

    if (flux_request_unpack (msg,
                             NULL,
                             "{s:s s:s}",
                             "gp_resource_path",
                             &rp,
                             "gp_key",
                             &gp_key)
        < 0)
        goto error;

    resource_path = rp;
    property_key = gp_key;

    it = ctx->db.metadata.by_path.find (resource_path);

    if (it == ctx->db.metadata.by_path.end ()) {
        errno = ENOENT;
        flux_log_error (h,
                        "Couldn't find %s in resource graph.",
                        resource_path.c_str ());
        goto error;
    }

    v = it->second;

    for (p_it = ctx->db.resource_graph[v].properties.begin ();
         p_it != ctx->db.resource_graph[v].properties.end ();
         p_it++) {
        if (property_key.compare (p_it->first) == 0)
            resp_value = p_it->second;
    }

    if (resp_value.empty ()) {
        errno = ENOENT;
        flux_log_error (h,
                        "Property %s was not found for resource %s.",
                        property_key.c_str (),
                        resource_path.c_str ());
        goto error;
    }

    if (flux_respond_pack (h, msg, "{s:s}", "value", resp_value.c_str ()) < 0)
        flux_log_error (h, "%s", __FUNCTION__);

    return;

error:
    if (flux_respond_error (h, msg, errno, NULL) < 0)
        flux_log_error (h, "%s: flux_respond_error", __FUNCTION__);
}

/******************************************************************************
 *                                                                            *
 *                               Module Main                                  *
 *                                                                            *
 ******************************************************************************/

extern "C" int mod_main (flux_t *h, int argc, char **argv)
{
    uint32_t rank = 1;
    int rc = -1;

    try {
        if (flux_get_rank (h, &rank) < 0) {
            flux_log (h, LOG_ERR, "can't determine rank");
            return rc;
        }
        if (rank) {
            flux_log (h, LOG_ERR, "resource module must only run on rank 0");
            return rc;
        }

        resource_ctx_t ctx{h};
        std::vector<std::string> args (argv, argv + argc);
        process_args (&ctx, args);

        flux::msg_handler_wrapper handlers[] =
            {{h, "resource.match", match_request_cb, &ctx},
             {h, "resource.cancel", cancel_request_cb, &ctx},
             {h, "resource.info", info_request_cb, &ctx},
             {h, "resource.stat", stat_request_cb, &ctx},
             {h, "resource.next_jobid", next_jobid_request_cb, &ctx},
             {h, "resource.set_property", set_property_request_cb, &ctx},
             {h, "resource.get_property", get_property_request_cb, &ctx}};

        flux_log (h, LOG_DEBUG, "resource module starting...");

        if ((rc = init_resource_graph (&ctx)) != 0) {
            flux_log (h, LOG_ERR, "can't initialize resource graph database");
            return rc;
        }
        flux_log (h, LOG_INFO, "resource graph database loaded");

        if ((rc = flux_reactor_run (flux_get_reactor (h), 0)) < 0) {
            flux_log (h, LOG_ERR, "flux_reactor_run: %s", strerror (errno));
            return rc;
        }
    } catch (const std::bad_alloc &e) {
        flux_log_error (h, "Allocation failed: %s", e.what ());
    } catch (const std::system_error &e) {
        errno = e.code ().value ();
        flux_log_error (h,
                        "system_error with code %d meaning: %s",
                        e.code ().value (),
                        e.what ());
    } catch (...) {
        flux_log_error (h, "unknown exception thrown in resource_match service");
    }

    return 0;
}

MOD_NAME ("resource");

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

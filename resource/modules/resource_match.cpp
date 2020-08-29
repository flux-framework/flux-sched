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
#include <flux/idset.h>
#include <jansson.h>
#include "src/common/libutil/shortjansson.h"
}

#include "resource/schema/resource_graph.hpp"
#include "resource/readers/resource_reader_factory.hpp"
#include "resource/traversers/dfu.hpp"
#include "resource/jobinfo/jobinfo.hpp"
#include "resource/policies/dfu_match_policy_factory.hpp"

using namespace Flux::resource_model;

/******************************************************************************
 *                                                                            *
 *                Resource Matching Service Module Context                    *
 *                                                                            *
 ******************************************************************************/

struct resource_args_t {
    std::string load_file;          /* load file name */
    std::string load_format;        /* load reader format */
    std::string load_allowlist;     /* load resource allowlist */
    std::string match_subsystems;
    std::string match_policy;
    std::string prune_filters;
    std::string match_format;
    int reserve_vtx_vec;           /* Allow for reserving vertex vector size */
};

struct match_perf_t {
    double load;                   /* Graph load time */
    uint64_t njobs;                /* Total match count */
    double min;                    /* Min match time */
    double max;                    /* Max match time */
    double accum;                  /* Total match time accumulated */
};

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
    resource_args_t args;          /* Module load options */
    std::shared_ptr<dfu_match_cb_t> matcher; /* Match callback object */
    std::shared_ptr<dfu_traverser_t> traverser; /* Graph traverser object */
    std::shared_ptr<resource_graph_db_t> db;    /* Resource graph data store */
    std::shared_ptr<f_resource_graph_t> fgraph; /* Filtered graph */
    std::shared_ptr<match_writers_t> writers;   /* Vertex/Edge writers */
    std::shared_ptr<resource_reader_base_t> reader; /* resource reader */
    match_perf_t perf;             /* Match performance stats */
    std::map<uint64_t, std::shared_ptr<job_info_t>> jobs; /* Jobs table */
    std::map<uint64_t, uint64_t> allocations;  /* Allocation table */
    std::map<uint64_t, uint64_t> reservations; /* Reservation table */
    std::map<std::string, std::shared_ptr<msg_wrap_t>> notify_msgs;
};


msg_wrap_t::msg_wrap_t (const msg_wrap_t &o)
{
    m_msg = flux_msg_incref (o.m_msg);
}

msg_wrap_t &msg_wrap_t::operator= (const msg_wrap_t &o)
{
    m_msg = flux_msg_incref (o.m_msg);
    return *this;
}

msg_wrap_t::~msg_wrap_t ()
{
    flux_msg_decref (m_msg);
}

const flux_msg_t *msg_wrap_t::get_msg () const
{
    return m_msg;
}

void msg_wrap_t::set_msg (const flux_msg_t *msg)
{
    if (m_msg)
        flux_msg_decref (m_msg);
    m_msg = flux_msg_incref (msg);
}

resource_interface_t::~resource_interface_t ()
{
    flux_future_decref (update_f);
}

resource_interface_t::resource_interface_t (const resource_interface_t &o)
{
    m_ups = o.m_ups;
    m_update_rc = o.m_update_rc;
    update_f = o.update_f;
    flux_future_incref (update_f);
}

resource_interface_t &resource_interface_t::operator= (
                                                const resource_interface_t &o)
{
    m_ups = o.m_ups;
    m_update_rc = o.m_update_rc;
    update_f = o.update_f;
    flux_future_incref (update_f);
    return *this;
}

int resource_interface_t::fetch_and_reset_update_rc ()
{
    int rc = m_update_rc;
    m_update_rc = 0;
    return rc;
}

int resource_interface_t::get_update_rc () const
{
    return m_update_rc;
}

void resource_interface_t::set_update_rc (int rc)
{
    m_update_rc = rc;
}

const std::string &resource_interface_t::get_ups () const
{
    return m_ups;
}

bool resource_interface_t::is_ups_set () const
{
    return m_ups != "";
}

void resource_interface_t::set_ups (const char *ups)
{
    m_ups = ups;
}

resource_ctx_t::~resource_ctx_t ()
{
    flux_msg_handler_delvec (handlers);
    for (auto &t : notify_msgs) {
        if (flux_respond_error (h, t.second->get_msg (), ECANCELED, NULL) < 0) {
            flux_log_error (h, "%s: flux_respond_error", __FUNCTION__);
        }
    }
}


/******************************************************************************
 *                                                                            *
 *                          Request Handler Prototypes                        *
 *                                                                            *
 ******************************************************************************/

static void match_request_cb (flux_t *h, flux_msg_handler_t *w,
                              const flux_msg_t *msg, void *arg);

static void update_request_cb (flux_t *h, flux_msg_handler_t *w,
                               const flux_msg_t *msg, void *arg);

static void cancel_request_cb (flux_t *h, flux_msg_handler_t *w,
                               const flux_msg_t *msg, void *arg);

static void info_request_cb (flux_t *h, flux_msg_handler_t *w,
                             const flux_msg_t *msg, void *arg);

static void stat_request_cb (flux_t *h, flux_msg_handler_t *w,
                             const flux_msg_t *msg, void *arg);

static void next_jobid_request_cb (flux_t *h, flux_msg_handler_t *w,
                                   const flux_msg_t *msg, void *arg);

static void set_property_request_cb (flux_t *h, flux_msg_handler_t *w,
                                   const flux_msg_t *msg, void *arg);

static void get_property_request_cb (flux_t *h, flux_msg_handler_t *w,
                                   const flux_msg_t *msg, void *arg);

static void notify_request_cb (flux_t *h, flux_msg_handler_t *w,
                               const flux_msg_t *msg, void *arg);

static void disconnect_request_cb (flux_t *h, flux_msg_handler_t *w,
                                   const flux_msg_t *msg, void *arg);

static void find_request_cb (flux_t *h, flux_msg_handler_t *w,
                             const flux_msg_t *msg, void *arg);

static void status_request_cb (flux_t *h, flux_msg_handler_t *w,
                               const flux_msg_t *msg, void *arg);

static const struct flux_msg_handler_spec htab[] = {
    { FLUX_MSGTYPE_REQUEST,
      "sched-fluxion-resource.match", match_request_cb, 0 },
    { FLUX_MSGTYPE_REQUEST,
      "sched-fluxion-resource.update", update_request_cb, 0},
    { FLUX_MSGTYPE_REQUEST,
      "sched-fluxion-resource.cancel", cancel_request_cb, 0 },
    { FLUX_MSGTYPE_REQUEST,
      "sched-fluxion-resource.info", info_request_cb, 0 },
    { FLUX_MSGTYPE_REQUEST,
      "sched-fluxion-resource.stat", stat_request_cb, 0 },
    { FLUX_MSGTYPE_REQUEST,
      "sched-fluxion-resource.next_jobid", next_jobid_request_cb, 0 },
    { FLUX_MSGTYPE_REQUEST,
      "sched-fluxion-resource.set_property", set_property_request_cb, 0 },
    { FLUX_MSGTYPE_REQUEST,
      "sched-fluxion-resource.get_property", get_property_request_cb, 0 },
    { FLUX_MSGTYPE_REQUEST,
      "sched-fluxion-resource.notify", notify_request_cb, 0 },
    { FLUX_MSGTYPE_REQUEST,
      "sched-fluxion-resource.disconnect", disconnect_request_cb, 0 },
    { FLUX_MSGTYPE_REQUEST,
      "sched-fluxion-resource.find", find_request_cb, 0 },
    { FLUX_MSGTYPE_REQUEST,
      "sched-fluxion-resource.status", status_request_cb, 0 },
    FLUX_MSGHANDLER_TABLE_END
};

static double get_elapse_time (timeval &st, timeval &et)
{
    double ts1 = (double)st.tv_sec + (double)st.tv_usec/1000000.0f;
    double ts2 = (double)et.tv_sec + (double)et.tv_usec/1000000.0f;
    return ts2 - ts1;
}

/******************************************************************************
 *                                                                            *
 *                   Module Initialization Routines                           *
 *                                                                            *
 ******************************************************************************/

static void set_default_args (resource_args_t &args)
{
    args.load_file = "";
    args.load_format = "hwloc";
    args.load_allowlist = "";
    args.match_subsystems = "containment";
    args.match_policy = "high";
    args.prune_filters = "ALL:core";
    args.match_format = "rv1_nosched";
    args.reserve_vtx_vec = 0;
}

static std::shared_ptr<resource_ctx_t> getctx (flux_t *h)
{
    void *d = NULL;
    std::shared_ptr<resource_ctx_t> ctx = nullptr;

    if ( (d = flux_aux_get (h, "sched-fluxion-resource")) != NULL)
        ctx = *(static_cast<std::shared_ptr<resource_ctx_t> *>(d));
    if (!ctx) {
        try {
            ctx = std::make_shared<resource_ctx_t> ();
            ctx->traverser = std::make_shared<dfu_traverser_t> ();
            ctx->db = std::make_shared<resource_graph_db_t> ();
        } catch (std::bad_alloc &e) {
            errno = ENOMEM;
            goto done;
        }
        ctx->h = h;
        ctx->handlers = NULL;
        set_default_args (ctx->args);
        ctx->perf.load = 0.0f;
        ctx->perf.njobs = 0;
        ctx->perf.min = DBL_MAX;
        ctx->perf.max = 0.0f;
        ctx->perf.accum = 0.0f;
        ctx->matcher = nullptr; /* Cannot be allocated at this point */
        ctx->fgraph = nullptr;  /* Cannot be allocated at this point */
        ctx->writers = nullptr; /* Cannot be allocated at this point */
        ctx->reader = nullptr;  /* Cannot be allocated at this point */
    }

done:
    return ctx;
}

static int process_args (std::shared_ptr<resource_ctx_t> &ctx,
                         int argc, char **argv)
{
    int rc = 0;
    resource_args_t &args = ctx->args;
    std::string dflt = "";

    for (int i = 0; i < argc; i++) {
        if (!strncmp ("load-file=", argv[i], sizeof ("load-file"))) {
            args.load_file = strstr (argv[i], "=") + 1;
        } else if (!strncmp ("load-format=", argv[i], sizeof ("load-format"))) {
            dflt = args.load_format;
            args.load_format = strstr (argv[i], "=") + 1;
            if (!known_resource_reader (args.load_format)) {
                flux_log (ctx->h, LOG_ERR,
                          "%s: unknown resource reader (%s)! use default (%s).",
                          __FUNCTION__,
                           args.load_format.c_str (), dflt.c_str ());
                args.load_format = dflt;
            }
            args.load_format = strstr (argv[i], "=") + 1;
        } else if (!strncmp ("load-allowlist=",
                             argv[i], sizeof ("load-allowlist"))) {
            args.load_allowlist = strstr (argv[i], "=") + 1;
        } else if (!strncmp ("subsystems=", argv[i], sizeof ("subsystems"))) {
            dflt = args.match_subsystems;
            args.match_subsystems = strstr (argv[i], "=") + 1;
        } else if (!strncmp ("policy=", argv[i], sizeof ("policy"))) {
            dflt = args.match_policy;
            args.match_policy = strstr (argv[i], "=") + 1;
            if (!known_match_policy (args.match_policy)) {
                flux_log (ctx->h, LOG_ERR,
                          "%s: unknown match policy (%s)! use default (%s).",
                           __FUNCTION__,
                           args.match_policy.c_str (), dflt.c_str ());
                args.match_policy = dflt;
            }
        } else if (!strncmp ("prune-filters=",
                             argv[i], sizeof ("prune-filters"))) {
            std::string token = strstr (argv[i], "=") + 1;
            if(token.find_first_not_of(' ') != std::string::npos) {
                args.prune_filters += ",";
                args.prune_filters += token;
            }
        } else if (!strncmp ("match-format=",
                             argv[i], sizeof ("match-format"))) {
            dflt = args.match_format;
            args.match_format = strstr (argv[i], "=") + 1;
            if (!known_match_format (args.match_format)) {
                args.match_format = dflt;
                flux_log (ctx->h, LOG_ERR,
                          "%s: unknown match format (%s)! use default (%s).",
                          __FUNCTION__,
                          args.match_format.c_str (), dflt.c_str ());
                args.match_format = dflt;
            }
        } else if (!strncmp ("reserve-vtx-vec=",
                             argv[i], sizeof ("reserve-vtx-vec"))) {
            args.reserve_vtx_vec = atoi (strstr (argv[i], "=") + 1);
            if ( args.reserve_vtx_vec <= 0 || args.reserve_vtx_vec > 2000000) {
                flux_log (ctx->h, LOG_ERR,
                          "%s: out of range specified for reserve-vtx-vec (%d)",
                          __FUNCTION__, args.reserve_vtx_vec);
                args.reserve_vtx_vec = 0;
            }
        } else {
            rc = -1;
            errno = EINVAL;
            flux_log (ctx->h, LOG_ERR, "%s: unknown option `%s'",
                      __FUNCTION__, argv[i]);
        }
    }

    return rc;
}

static std::shared_ptr<resource_ctx_t> init_module (flux_t *h,
                                                    int argc, char **argv)
{
    std::shared_ptr<resource_ctx_t> ctx = nullptr;
    uint32_t rank = 1;

    if (!(ctx = getctx (h))) {
        flux_log (h, LOG_ERR, "%s: can't allocate the context",
                  __FUNCTION__);
        return nullptr;
    }
    if (flux_get_rank (h, &rank) < 0) {
        flux_log (h, LOG_ERR, "%s: can't determine rank",
                  __FUNCTION__);
        goto error;
    }
    if (rank) {
        flux_log (h, LOG_ERR, "%s: resource module must only run on rank 0",
                  __FUNCTION__);
        goto error;
    }
    process_args (ctx, argc, argv);
    if (flux_msg_handler_addvec (h, htab, (void *)h, &ctx->handlers) < 0) {
        flux_log_error (h, "%s: error registering resource event handler",
                        __FUNCTION__);
        goto error;
    }
    return ctx;

error:
    return nullptr;
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

    if (!json_str || !(o = Jfromstr (json_str))
                  || !json_is_string (o)) {
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

static int populate_resource_db_file (std::shared_ptr<resource_ctx_t> &ctx)
{
    int rc = -1;
    int saved_errno;
    std::ifstream in_file;
    std::stringstream buffer{};

    saved_errno = errno;
    errno = 0;
    in_file.open (ctx->args.load_file.c_str (), std::ifstream::in);
    if (!in_file.good ()) {
        if (errno == 0) {
            // C++ standard doesn't guarantee to set errno but
            // many of the underlying system call set it appropriately.
            // we manually set errno only when it is not set at all.
            errno = EIO;
        }
        flux_log_error (ctx->h, "%s: opening %s",
                  __FUNCTION__, ctx->args.load_file.c_str ());
        goto done;
    }
    errno = saved_errno;
    buffer << in_file.rdbuf ();
    in_file.close ();
    if ( (rc = ctx->db->load (buffer.str (), ctx->reader)) < 0) {
        flux_log (ctx->h, LOG_ERR, "%s: reader: %s",
                  __FUNCTION__, ctx->reader->err_message ().c_str ());
        goto done;
    }
    rc = 0;

done:
    return rc;
}

static int grow (std::shared_ptr<resource_ctx_t> &ctx,
                 vtx_t v, unsigned int rank)
{
    int n = -1;
    int rc = -1;
    char k[64] = {0};
    json_t *o = NULL;
    int saved_errno = 0;
    const char *hwloc_xml = NULL;
    resource_graph_db_t &db = *(ctx->db);

    if (rank == IDSET_INVALID_ID) {
        // Grow cluster vertex and leave
        if ( (rc = db.load ("", ctx->reader, rank)) < 0) {
            flux_log (ctx->h, LOG_ERR, "%s: reader: %s",
                      __FUNCTION__,  ctx->reader->err_message ().c_str ());
        }
        goto ret;
    }

    n = snprintf (k, sizeof (k), "resource.hwloc.xml.%" PRIu32 "", rank);
    if ((n < 0) || ((unsigned int) n > sizeof (k))) {
        errno = ENOMEM;
        goto ret;
    }
    if ( !(o = get_string_blocking (ctx->h, k))) {
        flux_log_error (ctx->h, "%s: get_string_blocking", __FUNCTION__);
        goto ret;
    }
    hwloc_xml = json_string_value (o);
    if (v == boost::graph_traits<resource_graph_t>::null_vertex ()) {
        if ( (rc = db.load (hwloc_xml, ctx->reader, rank)) < 0) {
            flux_log (ctx->h, LOG_ERR, "%s: reader: %s",
                      __FUNCTION__,  ctx->reader->err_message ().c_str ());
            goto freemem_ret;
        }
    } else {
        if ( (rc = db.load (hwloc_xml, ctx->reader, v, rank)) < 0) {
            flux_log (ctx->h, LOG_ERR, "%s: reader: %s",
                      __FUNCTION__,  ctx->reader->err_message ().c_str ());
            goto freemem_ret;
        }
   }

freemem_ret:
    saved_errno = errno;
    json_decref (o);
    errno = saved_errno;
ret:
    return rc;
}

static int grow_resource_db (std::shared_ptr<resource_ctx_t> &ctx,
                             struct idset *ids)
{
    int rc = -1;
    resource_graph_db_t &db = *(ctx->db);
    unsigned int rank = idset_first (ids);
    vtx_t v = boost::graph_traits<resource_graph_t>::null_vertex ();

    if (db.metadata.roots.find ("containment") == db.metadata.roots.end ()) {
        if ( (rc = grow (ctx, v, rank)) < 0)
            goto done;
    }

    // If the above grow() does not grow resources in the "containment"
    // subsystem, this condition can still be false
    if (db.metadata.roots.find ("containment") == db.metadata.roots.end ()) {
        rc = -1;
        errno = EINVAL;
        flux_log (ctx->h, LOG_ERR, "%s: cluster vertex is unavailable",
                  __FUNCTION__);
        goto done;
    }
    v = db.metadata.roots.at ("containment");

    rank = idset_next (ids, rank);
    while (rank != IDSET_INVALID_ID) {
        // For the rest of the ranks -- general case
        if ( (rc = grow (ctx, v, rank)) < 0)
            goto done;
        rank = idset_next (ids, rank);
    }

done:
    return rc;
}

static int decode_all (std::shared_ptr<resource_ctx_t> &ctx,
                       std::set<int64_t> &ranks)
{
    unsigned int size = 0;
    unsigned int rank = 0;
    if (flux_get_size (ctx->h, &size) < -1) {
        flux_log (ctx->h, LOG_ERR, "%s: flux_get_size", __FUNCTION__);
        return -1;
    }
    for (rank = 0; rank < size; ++rank) {
        auto ret = ranks.insert (static_cast<int64_t> (rank));
        if (!ret.second) {
            errno = EEXIST;
            return -1;
        }
    }
    return 0;
}

static int decode_rankset (std::shared_ptr<resource_ctx_t> &ctx,
                           const char *ids, std::set<int64_t> &ranks)
{
    int rc = -1;
    unsigned int rank;
    struct idset *idset = NULL;

    if (!ids) {
        errno = EINVAL;
        goto done;
    }
    if (std::string ("all") == ids) {
        if ( (rc = decode_all (ctx, ranks)) < 0)
            goto done;
    } else {
        if ( !(idset = idset_decode (ids)))
            goto done;
        for (rank = idset_first (idset);
             rank != IDSET_INVALID_ID; rank = idset_next (idset, rank)) {
            auto ret = ranks.insert (static_cast<int64_t> (rank));
            if (!ret.second) {
                errno = EEXIST;
                goto done;
            }
        }
    }
    rc = 0;

done:
    idset_destroy (idset);
    return rc;
}

static int mark_lazy (std::shared_ptr<resource_ctx_t> &ctx,
                      const char *ids, resource_pool_t::status_t status)
{
    int rc = 0;
    switch (status) {
    case resource_pool_t::status_t::UP:
        ctx->set_ups (ids);
        break;

    case resource_pool_t::status_t::DOWN:
    default:
        // "down" shouldn't be a part of the first response of resource.acquire
        errno = EINVAL;
        rc = -1;
    }
    return rc;
}

static int mark_now (std::shared_ptr<resource_ctx_t> &ctx,
                     const char *ids, resource_pool_t::status_t status)
{
    int rc = -1;
    std::set <int64_t> ranks;
    if (!ids) {
        errno = EINVAL;
        goto done;
    }
    if ( (rc = decode_rankset (ctx, ids, ranks)) < 0)
        goto done;
    if ( (rc = ctx->traverser->mark (ranks, status)) < 0) {
        flux_log_error (ctx->h, "%s: traverser::mark: %s", __FUNCTION__,
                                ctx->traverser->err_message ().c_str ());
        ctx->traverser->clear_err_message ();
        goto done;
    }
    flux_log (ctx->h, LOG_DEBUG,
              "resource status changed (rankset=[%s] status=%s)",
              ids, resource_pool_t::status_to_str (status).c_str ());
done:
    return rc;
}

static int mark (std::shared_ptr<resource_ctx_t> &ctx,
                 const char *ids, resource_pool_t::status_t status)
{
    return (ctx->traverser->is_initialized ())? mark_now (ctx, ids, status)
                                              : mark_lazy (ctx, ids, status);
}

static int update_resource_db (std::shared_ptr<resource_ctx_t> &ctx,
                               struct idset *grow_set,
                               const char *up,
                               const char *down)
{
    int rc = 0;
    if (grow_set && (rc = grow_resource_db (ctx, grow_set)) < 0) {
        flux_log_error (ctx->h, "%s: grow_resource_db", __FUNCTION__);
        goto done;
    }
    if (up && (rc = mark (ctx, up, resource_pool_t::status_t::UP)) < 0) {
        flux_log_error (ctx->h, "%s: mark (up)", __FUNCTION__);
        goto done;
    }
    if (down && (rc = mark (ctx, down, resource_pool_t::status_t::DOWN)) < 0) {
        flux_log_error (ctx->h, "%s: mark (down)", __FUNCTION__);
        goto done;
    }
done:
    return rc;
}

// Directly copy form rutil_idset_from_resobj function
// from flux-core's src/modules/resource/rutil.c.
static struct idset *get_grow_idset (const json_t *resobj)
{
    struct idset *ids;
    const char *key;
    json_t *val;

    if (!(ids = idset_create (0, IDSET_FLAG_AUTOGROW)))
        return NULL;
    if (resobj) {
        json_object_foreach ((json_t *)resobj, key, val) {
            struct idset *valset;
            unsigned long id;

            if (!(valset = idset_decode (key)))
                goto error;
            id = idset_first (valset);
            while (id != IDSET_INVALID_ID) {
                if (idset_set (ids, id) < 0) {
                    idset_destroy (valset);
                    goto error;
                }
                id = idset_next (valset, id);
            }
            idset_destroy (valset);
        }
    }
    return ids;
error:
    idset_destroy (ids);
    return NULL;
}

static void update_resource (flux_future_t *f, void *arg)
{
    int rc = -1;
    const char *up = NULL;
    const char *down = NULL;
    json_t *grows = NULL;
    struct idset *grow_set = NULL;
    std::shared_ptr<resource_ctx_t> ctx = getctx ((flux_t *)arg);

    if ( (rc = flux_rpc_get_unpack (f, "{s?:o s?:s s?:s}",
                                           "resources", &grows,
                                           "up", &up,
                                           "down", &down)) < 0) {
        flux_log_error (ctx->h, "%s: exiting due to resource.acquire failure",
                        __FUNCTION__);
        flux_reactor_stop (flux_get_reactor (ctx->h));
        goto done;
    }
    if (grows) {
        if ( !(grow_set = get_grow_idset (grows))) {
            rc = -1;
            flux_log_error (ctx->h, "%s: get_grow_idset", __FUNCTION__);
            goto done;
        }
    }
    if ( (rc = update_resource_db (ctx, grow_set, up, down)) < 0) {
        flux_log_error (ctx->h, "%s: update_resource_db", __FUNCTION__);
        goto done;
    }
    for (auto &kv : ctx->notify_msgs) {
        if ( (rc += flux_respond (ctx->h, kv.second->get_msg (), NULL)) < 0) {
            flux_log_error (ctx->h, "%s: flux_respond", __FUNCTION__);
        }
    }
done:
    idset_destroy (grow_set);
    flux_future_reset (f);
    ctx->set_update_rc (rc);
}

static int populate_resource_db_kvs (std::shared_ptr<resource_ctx_t> &ctx)
{
    int rc = -1;
    json_t *o = NULL;

    if ( !(ctx->update_f = flux_rpc (ctx->h, "resource.acquire", NULL,
                                     FLUX_NODEID_ANY, FLUX_RPC_STREAMING))) {
        flux_log_error (ctx->h, "%s: flux_rpc", __FUNCTION__);
        goto done;
    }

    update_resource (ctx->update_f, static_cast<void *> (ctx->h));
    if ( (rc = ctx->fetch_and_reset_update_rc ()) < 0) {
        flux_log_error (ctx->h, "%s: update_resource", __FUNCTION__);
        goto done;
    }

    if ( (rc = flux_future_then (ctx->update_f, -1.0, update_resource,
                                 static_cast<void *> (ctx->h))) < 0) {
        flux_log_error (ctx->h, "%s: flux_future_then", __FUNCTION__);
        goto done;
    }
done:
    return rc;
}

static int populate_resource_db (std::shared_ptr<resource_ctx_t> &ctx)
{
    int rc = -1;
    double elapse;
    struct timeval st, et;

    if (ctx->args.reserve_vtx_vec != 0)
        ctx->db->resource_graph.m_vertices.reserve (ctx->args.reserve_vtx_vec);
    if ( (ctx->reader = create_resource_reader (
                            ctx->args.load_format)) == nullptr) {
        flux_log (ctx->h, LOG_ERR, "%s: can't create load reader",
                  __FUNCTION__);
        goto done;
    }
    if (ctx->args.load_allowlist != "") {
        if (ctx->reader->set_allowlist (ctx->args.load_allowlist) < 0)
            flux_log (ctx->h, LOG_ERR, "%s: setting allowlist", __FUNCTION__);
        if (!ctx->reader->is_allowlist_supported ())
            flux_log (ctx->h, LOG_WARNING, "%s: allowlist unsupported",
                      __FUNCTION__);
    }
    if ( (rc = gettimeofday (&st, NULL)) < 0) {
        flux_log_error (ctx->h, "%s: gettimeofday", __FUNCTION__);
        goto done;
    }
    if (ctx->args.load_file != "") {
        if (populate_resource_db_file (ctx) < 0)
            goto done;
        flux_log (ctx->h, LOG_INFO, "%s: loaded resources from %s",
                  __FUNCTION__,  ctx->args.load_file.c_str ());
    } else {
        if (populate_resource_db_kvs (ctx) < 0) {
            flux_log (ctx->h, LOG_ERR, "%s: loading resources from the KVS",
                      __FUNCTION__);
            goto done;
        }
        flux_log (ctx->h, LOG_INFO,
                  "%s: loaded resources from hwloc in the KVS",
                  __FUNCTION__);
    }
    if ( (rc = gettimeofday (&et, NULL)) < 0) {
        flux_log_error (ctx->h, "%s: gettimeofday", __FUNCTION__);
        goto done;
    }
    ctx->perf.load = get_elapse_time (st, et);
    rc = 0;

done:
    return rc;
}

static int select_subsystems (std::shared_ptr<resource_ctx_t> &ctx)
{
    /*
     * Format of match_subsystems
     * subsystem1[:relation1[:relation2...]],subsystem2[...
     */
    int rc = 0;
    std::stringstream ss (ctx->args.match_subsystems);
    subsystem_t subsystem;
    std::string token;

    while (getline (ss, token, ',')) {
        size_t found = token.find_first_of (":");
        if (found == std::string::npos) {
            subsystem = token;
            if (!ctx->db->known_subsystem (subsystem)) {
                rc = -1;
                errno = EINVAL;
                goto done;
            }
            ctx->matcher->add_subsystem (subsystem, "*");
        } else {
            subsystem = token.substr (0, found);
            if (!ctx->db->known_subsystem (subsystem)) {
                rc = -1;
                errno = EINVAL;
                goto done;
            }
            std::stringstream relations (token.substr (found+1,
                                                       std::string::npos));
            std::string relation;
            while (getline (relations, relation, ':'))
                ctx->matcher->add_subsystem (subsystem, relation);
        }
    }

done:
    return rc;
}

static std::shared_ptr<f_resource_graph_t> create_filtered_graph (
                                               std::shared_ptr<
                                                   resource_ctx_t> &ctx)
{
    std::shared_ptr<f_resource_graph_t> fg = nullptr;
    resource_graph_t &g = ctx->db->resource_graph;

    try {
        // Set vertex and edge maps
        vtx_infra_map_t vmap = get (&resource_pool_t::idata, g);
        edg_infra_map_t emap = get (&resource_relation_t::idata, g);

        // Set vertex and edge filters based on subsystems to use
        const multi_subsystemsS &filter = ctx->matcher->subsystemsS ();
        subsystem_selector_t<vtx_t, f_vtx_infra_map_t> vtxsel (vmap, filter);
        subsystem_selector_t<edg_t, f_edg_infra_map_t> edgsel (emap, filter);
        fg = std::make_shared<f_resource_graph_t> (g, edgsel, vtxsel);
    } catch (std::bad_alloc &e) {
        errno = ENOMEM;
        fg = nullptr;
    }

    return fg;
}

static int init_resource_graph (std::shared_ptr<resource_ctx_t> &ctx)
{
    int rc = 0;

    // Select the appropriate matcher based on CLI policy.
    if ( !(ctx->matcher = create_match_cb (ctx->args.match_policy))) {
        flux_log (ctx->h, LOG_ERR, "%s: can't create match callback",
                  __FUNCTION__);
        return -1;

    }
    if ( (rc = populate_resource_db (ctx)) != 0) {
        flux_log (ctx->h, LOG_ERR,
                  "%s: can't populate graph resource database",
                  __FUNCTION__);
        return rc;
    }
    if ( (rc = select_subsystems (ctx)) != 0) {
        flux_log (ctx->h, LOG_ERR, "%s: error processing subsystems %s",
                  __FUNCTION__, ctx->args.match_subsystems.c_str ());
        return rc;
    }
    if ( !(ctx->fgraph = create_filtered_graph (ctx)))
        return -1;

    // Create a writers object for matched vertices and edges
    match_format_t format = match_writers_factory_t::
                                get_writers_type (ctx->args.match_format);
    if ( !(ctx->writers = match_writers_factory_t::create (format)))
        return -1;

    if (ctx->args.prune_filters != ""
        && ctx->matcher->set_pruning_types_w_spec (ctx->matcher->dom_subsystem (),
                                                   ctx->args.prune_filters) < 0) {
        flux_log (ctx->h, LOG_ERR, "%s: error setting pruning types with: %s",
                  __FUNCTION__, ctx->args.prune_filters.c_str ());
        return -1;
    }

    // Initialize the DFU traverser
    if (ctx->traverser->initialize (ctx->fgraph, ctx->db, ctx->matcher) < 0) {
        flux_log (ctx->h, LOG_ERR, "%s: traverser initialization",
                  __FUNCTION__);
        return -1;

    }

    // Perform the initial status marking only when "up" rankset is available
    // Rankless reader cases (required for testing e.g., GRUG) must not
    // exectute the following branch.
    if (ctx->is_ups_set ()) {
        if (mark (ctx, "all", resource_pool_t::status_t::DOWN) < 0) {
            flux_log (ctx->h, LOG_ERR, "%s: mark (down)", __FUNCTION__);
            return -1;
        }
        if (mark (ctx, ctx->get_ups ().c_str (),
                  resource_pool_t::status_t::UP) < 0) {
            flux_log (ctx->h, LOG_ERR, "%s: mark (up)", __FUNCTION__);
            return -1;
        }
    }
    return 0;
}


/******************************************************************************
 *                                                                            *
 *                        Request Handler Routines                            *
 *                                                                            *
 ******************************************************************************/

static void update_match_perf (std::shared_ptr<resource_ctx_t> &ctx,
                               double elapse)
{
    ctx->perf.njobs++;
    ctx->perf.min = (ctx->perf.min > elapse)? elapse : ctx->perf.min;
    ctx->perf.max = (ctx->perf.max < elapse)? elapse : ctx->perf.max;
    ctx->perf.accum += elapse;
}

static inline std::string get_status_string (int64_t now, int64_t at)
{
    return (at == now)? "ALLOCATED" : "RESERVED";
}

static inline bool is_existent_jobid (
                       const std::shared_ptr<resource_ctx_t> &ctx,
                       uint64_t jobid)
{
    return (ctx->jobs.find (jobid) != ctx->jobs.end ())? true : false;
}

static int track_schedule_info (std::shared_ptr<resource_ctx_t> &ctx,
                                int64_t id, bool reserved, int64_t at,
                                const std::string &jspec,
                                const std::stringstream &R, double elapse)
{
    if (id < 0 || at < 0) {
        errno = EINVAL;
        return -1;
    }
    try {
        job_lifecycle_t state = (!reserved)? job_lifecycle_t::ALLOCATED
                                           : job_lifecycle_t::RESERVED;
        ctx->jobs[id] = std::make_shared<job_info_t> (id, state, at, "",
                                                      jspec, R.str (), elapse);
        if (!reserved)
            ctx->allocations[id] = id;
        else
            ctx->reservations[id] = id;
    }
    catch (std::bad_alloc &e) {
        errno = ENOMEM;
        return -1;
    }
    return 0;
}

static int parse_R (std::shared_ptr<resource_ctx_t> &ctx, const char *R,
                    std::string &jgf, int64_t &starttime, uint64_t &duration)
{
    int rc = 0;
    int version = 0;
    int saved_errno;
    uint64_t st = 0;
    uint64_t et = 0;
    json_t *o = NULL;
    json_t *graph = NULL;
    json_error_t error;
    char *jgf_str = NULL;

    if ( (o = json_loads (R, 0, &error)) == NULL) {
        rc = -1;
        flux_log (ctx->h, LOG_ERR, "%s: %s", __FUNCTION__, error.text);
        errno = EINVAL;
        goto out;
    }
    if ( (rc = json_unpack (o, "{s:i s:{s:I s:I} s?:o}",
                                   "version", &version,
                                   "execution",
                                       "starttime", &st,
                                       "expiration", &et,
                                   "scheduling", &graph)) < 0) {
        errno = EINVAL;
        flux_log (ctx->h, LOG_ERR, "%s: json_unpack", __FUNCTION__);
        goto freemem_out;
    }
    if (version != 1 || st < 0 || et < st) {
        rc = -1;
        errno = EPROTO;
        flux_log (ctx->h, LOG_ERR,
                  "%s: version=%d, starttime=%jd, expiration=%jd",
                  __FUNCTION__, version,
                  static_cast<intmax_t> (st), static_cast<intmax_t> (et));
        goto freemem_out;
    }
    if (graph == NULL) {
        rc = -1;
        errno = ENOENT;
        flux_log (ctx->h, LOG_ERR, "%s: no scheduling key in R", __FUNCTION__);
        goto freemem_out;
    }
    if ( !(jgf_str = json_dumps (graph, JSON_INDENT (0)))) {
        rc = -1;
        errno = ENOMEM;
        flux_log (ctx->h, LOG_ERR, "%s: json_dumps", __FUNCTION__);
        goto freemem_out;
    }
    jgf = jgf_str;
    free (jgf_str);
    starttime = static_cast<int64_t> (st);
    duration = et - st;

freemem_out:
    saved_errno = errno;
    json_decref (o);
    errno = saved_errno;
out:
    return rc;
}

static int Rlite_equal (const std::shared_ptr<resource_ctx_t> &ctx,
                        const char *R1, const char *R2)
{
    int rc = -1;
    int saved_errno;
    json_t *o1 = NULL;
    json_t *o2 = NULL;
    json_t *rlite1 = NULL;
    json_t *rlite2 = NULL;
    json_error_t error1;
    json_error_t error2;

    if ( (o1 = json_loads (R1, 0, &error1)) == NULL) {
        errno = EINVAL;
        flux_log (ctx->h, LOG_ERR, "%s: %s", __FUNCTION__, error1.text);
        goto out;
    }
    if ( (rc = json_unpack (o1, "{s:{s:o}}",
                                    "execution",
                                    "R_lite", &rlite1)) < 0) {
        errno = EINVAL;
        goto out;
    }
    if ( (o2 = json_loads (R2, 0, &error2)) == NULL) {
        errno = EINVAL;
        flux_log (ctx->h, LOG_ERR, "%s: %s", __FUNCTION__, error2.text);
        goto out;
    }
    if ( (rc = json_unpack (o2, "{s:{s:o}}",
                                    "execution",
                                    "R_lite", &rlite2)) < 0) {
        errno = EINVAL;
        goto out;
    }
    rc = (json_equal (rlite1, rlite2) == 1)? 0 : 1;

out:
    saved_errno = errno;
    json_decref (o1);
    json_decref (o2);
    errno = saved_errno;
    return rc;
}

static int run (std::shared_ptr<resource_ctx_t> &ctx, int64_t jobid,
                const char *cmd, const std::string &jstr, int64_t *at)
{
    int rc = 0;
    Flux::Jobspec::Jobspec j {jstr};
    dfu_traverser_t &tr = *(ctx->traverser);

    if (std::string ("allocate") == cmd)
        rc = tr.run (j, ctx->writers, match_op_t::MATCH_ALLOCATE, jobid, at);
    else if (std::string ("allocate_with_satisfiability") == cmd)
        rc = tr.run (j, ctx->writers, match_op_t::
                     MATCH_ALLOCATE_W_SATISFIABILITY, jobid, at);
    else if (std::string ("allocate_orelse_reserve") == cmd)
        rc = tr.run (j, ctx->writers, match_op_t::MATCH_ALLOCATE_ORELSE_RESERVE,
                     jobid, at);
   return rc;
}

static int run (std::shared_ptr<resource_ctx_t> &ctx, int64_t jobid,
                const std::string &jgf, int64_t at, uint64_t duration)
{
    int rc = 0;
    dfu_traverser_t &tr = *(ctx->traverser);
    std::shared_ptr<resource_reader_base_t> rd;
    if ((rd = create_resource_reader ("jgf")) == nullptr) {
        rc = -1;
        flux_log (ctx->h, LOG_ERR, "%s: create_resource_reader (id=%jd)",
                  __FUNCTION__, static_cast<intmax_t> (jobid));
        goto out;
    }
    if ((rc = tr.run (jgf, ctx->writers, rd, jobid, at, duration)) < 0) {
        flux_log (ctx->h, LOG_ERR, "%s: dfu_traverser_t::run (id=%jd): %s",
                  __FUNCTION__, static_cast<intmax_t> (jobid),
                  ctx->traverser->err_message ().c_str ());
        goto out;
    }

out:
   return rc;
}

static int run_match (std::shared_ptr<resource_ctx_t> &ctx, int64_t jobid,
                      const char *cmd, const std::string &jstr, int64_t *now,
                      int64_t *at, double *ov, std::stringstream &o)
{
    int rc = 0;
    double elapse = 0.0f;
    struct timeval start;
    struct timeval end;
    bool rsv = false;

    if ( (rc = gettimeofday (&start, NULL)) < 0) {
        flux_log_error (ctx->h, "%s: gettimeofday", __FUNCTION__);
        goto done;
    }
    if (strcmp ("allocate", cmd) != 0
        && strcmp ("allocate_orelse_reserve", cmd) != 0
        && strcmp ("allocate_with_satisfiability", cmd) != 0) {
        rc = -1;
        errno = EINVAL;
        flux_log (ctx->h, LOG_ERR, "%s: unknown cmd: %s", __FUNCTION__, cmd);
        goto done;
    }

    *at = *now = (int64_t)start.tv_sec;
    if ( (rc = run (ctx, jobid, cmd, jstr, at)) < 0) {
        goto done;
    }
    if ( (rc = ctx->writers->emit (o)) < 0) {
        flux_log_error (ctx->h, "%s: writer can't emit", __FUNCTION__);
        goto done;
    }

    rsv = (*now != *at)? true : false;
    if ( (rc = gettimeofday (&end, NULL)) < 0) {
        flux_log_error (ctx->h, "%s: gettimeofday", __FUNCTION__);
        goto done;
    }
    *ov = get_elapse_time (start, end);
    update_match_perf (ctx, *ov);

    if ( (rc = track_schedule_info (ctx, jobid, rsv, *at, jstr, o, *ov)) != 0) {
        flux_log_error (ctx->h, "%s: can't add job info (id=%jd)",
                        __FUNCTION__, (intmax_t)jobid);
        goto done;
    }

done:
    return rc;
}

static int run_update (std::shared_ptr<resource_ctx_t> &ctx, int64_t jobid,
                       const char *R, int64_t &at, double &ov,
                       std::stringstream &o)
{
    int rc = 0;
    uint64_t duration = 0;
    double elapse = 0.0f;
    struct timeval start;
    struct timeval end;
    std::string jgf;
    std::string R2;

    if ( (rc = gettimeofday (&start, NULL)) < 0) {
        flux_log_error (ctx->h, "%s: gettimeofday", __FUNCTION__);
        goto done;
    }
    if ( (rc = parse_R (ctx, R, jgf, at, duration)) < 0) {
        flux_log_error (ctx->h, "%s: parsing R", __FUNCTION__);
        goto done;
    }
    if ( (rc = run (ctx, jobid, jgf, at, duration)) < 0) {
        flux_log_error (ctx->h, "%s: run", __FUNCTION__);
        goto done;
    }
    if ( (rc = ctx->writers->emit (o)) < 0) {
        flux_log_error (ctx->h, "%s: writers->emit", __FUNCTION__);
        goto done;
    }
    if ( (rc = gettimeofday (&end, NULL)) < 0) {
        flux_log_error (ctx->h, "%s: gettimeofday", __FUNCTION__);
        goto done;
    }
    ov = get_elapse_time (start, end);
    update_match_perf (ctx, ov);
    if ( (rc = track_schedule_info (ctx, jobid, false, at, "", o, ov)) != 0) {
        flux_log_error (ctx->h, "%s: can't add job info (id=%jd)",
                        __FUNCTION__, (intmax_t)jobid);
        goto done;
    }

done:
    return rc;
}

static void update_request_cb (flux_t *h, flux_msg_handler_t *w,
                               const flux_msg_t *msg, void *arg)
{
    char *R = NULL;
    int64_t at = 0;
    double ov = 0.0f;
    int64_t jobid = 0;
    uint64_t duration = 0;
    std::string status = "";
    std::stringstream o;

    std::shared_ptr<resource_ctx_t> ctx = getctx ((flux_t *)arg);
    if (flux_request_unpack (msg, NULL, "{s:I s:s}",
                                            "jobid", &jobid,
                                            "R", &R) < 0) {
        flux_log_error (ctx->h, "%s: flux_request_unpack", __FUNCTION__);
        goto error;
    }
    if (is_existent_jobid (ctx, jobid)) {
        int rc = 0;
        struct timeval st, et;
        if ( (rc = gettimeofday (&st, NULL)) < 0) {
            flux_log_error (ctx->h, "%s: gettimeofday", __FUNCTION__);
            goto error;
        }
        if ( (rc = Rlite_equal (ctx, R, ctx->jobs[jobid]->R.c_str ())) < 0) {
            flux_log_error (ctx->h, "%s: Rlite_equal", __FUNCTION__);
            goto error;
        } else if (rc == 1) {
            errno=EINVAL;
            flux_log (ctx->h, LOG_ERR,
                      "%s: jobid (%jd) with different R exists!",
                      __FUNCTION__, static_cast<intmax_t> (jobid));
            goto error;
        }
        if ( (rc = gettimeofday (&et, NULL)) < 0) {
            flux_log_error (ctx->h, "%s: gettimeofday", __FUNCTION__);
            goto error;
        }
        // If a jobid with matching R exists, no need to update
        ov = get_elapse_time (st, et);
        get_jobstate_str (ctx->jobs[jobid]->state, status);
        o << ctx->jobs[jobid]->R;
        at = ctx->jobs[jobid]->scheduled_at;
        flux_log (ctx->h, LOG_DEBUG, "%s: jobid (%jd) with matching R exists",
                  __FUNCTION__, static_cast<intmax_t> (jobid));
    } else if (run_update (ctx, jobid, R, at, ov, o) < 0) {
        flux_log_error (ctx->h,
                        "%s: update failed (id=%jd)",
                        __FUNCTION__, static_cast<intmax_t> (jobid));
        goto error;
    }

    if ( status == "")
        status = get_status_string (at, at);

    if (flux_respond_pack (h, msg, "{s:I s:s s:f s:s s:I}",
                                       "jobid", jobid,
                                       "status", status.c_str (),
                                       "overhead", ov,
                                       "R", o.str ().c_str (),
                                       "at", at) < 0)
        flux_log_error (h, "%s", __FUNCTION__);

    return;

error:
    if (flux_respond_error (h, msg, errno, NULL) < 0)
        flux_log_error (h, "%s: flux_respond_error", __FUNCTION__);
}

static int run_remove (std::shared_ptr<resource_ctx_t> &ctx, int64_t jobid)
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
           std::shared_ptr<job_info_t> info = ctx->jobs[jobid];
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

static void match_request_cb (flux_t *h, flux_msg_handler_t *w,
                              const flux_msg_t *msg, void *arg)
{
    int64_t at = 0;
    int64_t now = 0;
    int64_t jobid = -1;
    double ov = 0.0f;
    std::string status = "";
    const char *cmd = NULL;
    const char *js_str = NULL;
    std::stringstream R;

    std::shared_ptr<resource_ctx_t> ctx = getctx ((flux_t *)arg);
    if (flux_request_unpack (msg, NULL, "{s:s s:I s:s}", "cmd", &cmd,
                             "jobid", &jobid, "jobspec", &js_str) < 0)
        goto error;
    if (is_existent_jobid (ctx, jobid)) {
        errno = EINVAL;
        flux_log_error (h, "%s: existent job (%jd).",
                        __FUNCTION__, (intmax_t)jobid);
        goto error;
    }
    if (run_match (ctx, jobid, cmd, js_str, &now, &at, &ov, R) < 0) {
        if (errno != EBUSY && errno != ENODEV)
            flux_log_error (ctx->h,
                            "%s: match failed due to match error (id=%jd)",
                            __FUNCTION__, (intmax_t)jobid);
        goto error;
    }

    status = get_status_string (now, at);
    if (flux_respond_pack (h, msg, "{s:I s:s s:f s:s s:I}",
                                   "jobid", jobid,
                                   "status", status.c_str (),
                                   "overhead", ov,
                                   "R", R.str ().c_str (),
                                   "at", at) < 0)
        flux_log_error (h, "%s", __FUNCTION__);

    return;

error:
    if (flux_respond_error (h, msg, errno, NULL) < 0)
        flux_log_error (h, "%s: flux_respond_error", __FUNCTION__);
}

static void cancel_request_cb (flux_t *h, flux_msg_handler_t *w,
                               const flux_msg_t *msg, void *arg)
{
    std::shared_ptr<resource_ctx_t> ctx = getctx ((flux_t *)arg);
    int64_t jobid = -1;

    if (flux_request_unpack (msg, NULL, "{s:I}", "jobid", &jobid) < 0)
        goto error;
    if (ctx->allocations.find (jobid) != ctx->allocations.end ())
        ctx->allocations.erase (jobid);
    else if (ctx->reservations.find (jobid) != ctx->reservations.end ())
        ctx->reservations.erase (jobid);
    else {
        errno = ENOENT;
        flux_log (h, LOG_DEBUG, "%s: nonexistent job (id=%jd)",
                  __FUNCTION__, (intmax_t)jobid);
        goto error;
    }

    if (run_remove (ctx, jobid) < 0) {
        flux_log_error (h, "%s: remove fails due to match error (id=%jd)",
                        __FUNCTION__, (intmax_t)jobid);
        goto error;
    }
    if (flux_respond_pack (h, msg, "{}") < 0)
        flux_log_error (h, "%s", __FUNCTION__);

    return;

error:
    if (flux_respond_error (h, msg, errno, NULL) < 0)
        flux_log_error (h, "%s: flux_respond_error", __FUNCTION__);
}

static void info_request_cb (flux_t *h, flux_msg_handler_t *w,
                             const flux_msg_t *msg, void *arg)
{
    std::shared_ptr<resource_ctx_t> ctx = getctx ((flux_t *)arg);
    int64_t jobid = -1;
    std::shared_ptr<job_info_t> info = NULL;
    std::string status = "";

    if (flux_request_unpack (msg, NULL, "{s:I}", "jobid", &jobid) < 0)
        goto error;
    if (!is_existent_jobid (ctx, jobid)) {
        errno = ENOENT;
        flux_log (h, LOG_DEBUG, "%s: nonexistent job (id=%jd)",
                  __FUNCTION__,  (intmax_t)jobid);
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

    return;

error:
    if (flux_respond_error (h, msg, errno, NULL) < 0)
        flux_log_error (h, "%s: flux_respond_error", __FUNCTION__);
}

static int get_stat_by_rank (std::shared_ptr<resource_ctx_t>& ctx, json_t *o)
{
    int rc = -1;
    int saved_errno = 0;
    char *str = nullptr;
    struct idset *ids = nullptr;
    std::map<size_t, struct idset *> s2r;

    for (auto &kv : ctx->db->metadata.by_rank) {
        if (kv.first == -1)
            continue;
        if (s2r.find (kv.second.size ()) == s2r.end ()) {
            if ( !(ids = idset_create (0, IDSET_FLAG_AUTOGROW)))
                goto done;
            s2r[kv.second.size ()] = ids;
        }
        if ( (rc = idset_set (s2r[kv.second.size ()],
                              static_cast<unsigned int> (kv.first))) < 0)
            goto done;
    }

    for (auto &kv : s2r) {
        if ( !(str = idset_encode (kv.second,
                                   IDSET_FLAG_BRACKETS | IDSET_FLAG_RANGE))) {
            rc = -1;
            goto done;
        }
        if ( (rc = json_object_set_new (o, str,
                      json_integer (static_cast<json_int_t> (kv.first)))) < 0) {
            errno = ENOMEM;
            goto done;
        }
        saved_errno = errno;
        free (str);
        errno = saved_errno;
        str = nullptr;
    }

done:
    for (auto &kv : s2r)
        idset_destroy (kv.second);
    saved_errno = errno;
    s2r.clear ();
    free (str);
    errno = saved_errno;
    return rc;
}

static void stat_request_cb (flux_t *h, flux_msg_handler_t *w,
                             const flux_msg_t *msg, void *arg)
{
    std::shared_ptr<resource_ctx_t> ctx = getctx ((flux_t *)arg);
    int saved_errno;
    json_t *o = nullptr;
    double avg = 0.0f;
    double min = 0.0f;

    if (ctx->perf.njobs) {
        avg = ctx->perf.accum / (double)ctx->perf.njobs;
        min = ctx->perf.min;
    }
    if ( !(o = json_object ())) {
        errno = ENOMEM;
        goto error;
    }
    if (get_stat_by_rank (ctx, o) < 0) {
        flux_log_error (h, "%s: get_stat_by_rank", __FUNCTION__);
        goto error_free;
    }
    if (flux_respond_pack (h, msg, "{s:I s:I s:o s:f s:I s:f s:f s:f}",
                                   "V", num_vertices (ctx->db->resource_graph),
                                   "E", num_edges (ctx->db->resource_graph),
                                   "by_rank", o,
                                   "load-time", ctx->perf.load,
                                   "njobs", ctx->perf.njobs,
                                   "min-match", min,
                                   "max-match", ctx->perf.max,
                                   "avg-match", avg) < 0) {
        flux_log_error (h, "%s: flux_respond_pack", __FUNCTION__);
    }

    return;

error_free:
    saved_errno = errno;
    json_decref (o);
    errno = saved_errno;
error:
    if (flux_respond_error (h, msg, errno, NULL) < 0)
        flux_log_error (h, "%s: flux_respond_error", __FUNCTION__);
}

static inline int64_t next_jobid (const std::map<uint64_t,
                                            std::shared_ptr<job_info_t>> &m)
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
    std::shared_ptr<resource_ctx_t> ctx = getctx ((flux_t *)arg);
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

static void set_property_request_cb (flux_t *h, flux_msg_handler_t *w,
                                   const flux_msg_t *msg, void *arg)
{
    const char *rp = NULL, *kv = NULL;
    std::string resource_path = "", keyval = "";
    std::string property_key = "", property_value = "";
    size_t pos;
    std::shared_ptr<resource_ctx_t> ctx = getctx ((flux_t *)arg);
    std::map<std::string, vtx_t>::const_iterator it;
    std::pair<std::map<std::string, std::string>::iterator, bool> ret;
    vtx_t v;

    if (flux_request_unpack (msg, NULL, "{s:s s:s}",
                                        "sp_resource_path", &rp,
                                        "sp_keyval", &kv) < 0)
        goto error;

    resource_path = rp;
    keyval = kv;

    pos = keyval.find ('=');

    if (pos == 0 || (pos == keyval.size () - 1) || pos == std::string::npos) {
        errno = EINVAL;
        flux_log_error (h, "%s: Incorrect format.", __FUNCTION__);
        flux_log_error (h, "%s: Use set-property <resource> PROPERTY=VALUE",
                        __FUNCTION__);
        goto error;
    }

    property_key = keyval.substr (0, pos);
    property_value = keyval.substr (pos + 1);

    it = ctx->db->metadata.by_path.find (resource_path);

    if (it == ctx->db->metadata.by_path.end ()) {
        errno = ENOENT;
        flux_log_error (h, "%s: Couldn't find %s in resource graph.",
                        __FUNCTION__, resource_path.c_str ());
        goto error;
     }

    v = it->second;

    ret = ctx->db->resource_graph[v].properties.insert (
        std::pair<std::string, std::string> (property_key,property_value));

    if (ret.second == false) {
        ctx->db->resource_graph[v].properties.erase (property_key);
        ctx->db->resource_graph[v].properties.insert (
            std::pair<std::string, std::string> (property_key,property_value));
    }

    if (flux_respond_pack (h, msg, "{}") < 0)
        flux_log_error (h, "%s", __FUNCTION__);

    return;

error:
    if (flux_respond_error (h, msg, errno, NULL) < 0)
        flux_log_error (h, "%s: flux_respond_error", __FUNCTION__);
}

static void get_property_request_cb (flux_t *h, flux_msg_handler_t *w,
                                     const flux_msg_t *msg, void *arg)
{
    const char *rp = NULL, *gp_key = NULL;
    std::string resource_path = "", property_key = "";
    std::shared_ptr<resource_ctx_t> ctx = getctx ((flux_t *)arg);
    std::map<std::string, vtx_t>::const_iterator it;
    std::map<std::string, std::string>::const_iterator p_it;
    vtx_t v;
    std::string resp_value = "";

    if (flux_request_unpack (msg, NULL, "{s:s s:s}",
                                        "gp_resource_path", &rp,
                                        "gp_key", &gp_key) < 0)
        goto error;

    resource_path = rp;
    property_key = gp_key;

    it = ctx->db->metadata.by_path.find (resource_path);

    if (it == ctx->db->metadata.by_path.end ()) {
        errno = ENOENT;
        flux_log_error (h, "%s: Couldn't find %s in resource graph.",
                        __FUNCTION__, resource_path.c_str ());
        goto error;
     }

    v = it->second;

    for (p_it = ctx->db->resource_graph[v].properties.begin ();
         p_it != ctx->db->resource_graph[v].properties.end (); p_it++) {

         if (property_key.compare (p_it->first) == 0)
             resp_value = p_it->second;
     }

     if (resp_value.empty ()) {
         errno = ENOENT;
         flux_log_error (h, "%s: Property %s was not found for resource %s.",
                         __FUNCTION__, property_key.c_str (),
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

static void disconnect_request_cb (flux_t *h, flux_msg_handler_t *w,
                                   const flux_msg_t *msg, void *arg)
{
    char *route = NULL;
    std::shared_ptr<resource_ctx_t> ctx = getctx ((flux_t *)arg);

    if (flux_msg_get_route_first (msg, &route) < 0) {
        flux_log_error (h, "%s: flux_msg_get_route_first", __FUNCTION__);
        goto error;
    }
    if (ctx->notify_msgs.find (route) != ctx->notify_msgs.end ()) {
        ctx->notify_msgs.erase (route);
        flux_log (h, LOG_DEBUG, "%s: a notify request aborted", __FUNCTION__);
    }

error:
    free (route);
    return;
}

static void notify_request_cb (flux_t *h, flux_msg_handler_t *w,
                               const flux_msg_t *msg, void *arg)
{
    try {
        char *route = NULL;
        std::shared_ptr<resource_ctx_t> ctx = getctx ((flux_t *)arg);
        std::shared_ptr<msg_wrap_t> m = std::make_shared<msg_wrap_t> ();

        if (flux_request_decode (msg, NULL, NULL) < 0) {
            flux_log_error (h, "%s: flux_request_decode", __FUNCTION__);
            goto error;
        }
        if (!flux_msg_is_streaming (msg)) {
            errno = EPROTO;
            flux_log_error (h, "%s: streaming flag not set", __FUNCTION__);
            goto error;
        }
        if (flux_msg_get_route_first (msg, &route) < 0) {
            free (route);
            flux_log_error (h, "%s: flux_msg_get_route_first", __FUNCTION__);
            goto error;
        }

        m->set_msg (msg);
        auto ret = ctx->notify_msgs.insert (
                       std::pair<std::string,
                                 std::shared_ptr<msg_wrap_t>> (route, m));
        free (route);
        if (!ret.second) {
            errno = EEXIST;
            flux_log_error (h, "%s: insert", __FUNCTION__);
            goto error;
        }

        if (flux_respond (ctx->h, msg, NULL) < 0) {
            flux_log_error (ctx->h, "%s: flux_respond", __FUNCTION__);
            goto error;
        }
    } catch (std::bad_alloc &e) {
        errno = ENOMEM;
    }
    return;

error:
    if (flux_respond_error (h, msg, errno, NULL) < 0)
        flux_log_error (h, "%s: flux_respond_error", __FUNCTION__);
}

static int run_find (std::shared_ptr<resource_ctx_t>& ctx,
                     const std::string &criteria, json_t **R)
{
    int rc = -1;
    json_t *o = nullptr;
    std::shared_ptr<match_writers_t> w = nullptr;

    if ( !(w = match_writers_factory_t::create (match_format_t::RV1_NOSCHED)))
        goto error;
    if ( (rc = ctx->traverser->find (w, criteria)) < 0) {
        if (ctx->traverser->err_message () != "") {
            flux_log_error (ctx->h, "%s: %s",
                            __FUNCTION__,
                            ctx->traverser->err_message ().c_str ());
            ctx->traverser->clear_err_message ();
        }
        goto error;
    }
    if ( (rc = w->emit_json (&o)) < 0) {
        flux_log_error (ctx->h, "%s: emit", __FUNCTION__);
        goto error;
    }
    if (o)
        *R = o;

error:
    return rc;
}

static void find_request_cb (flux_t *h, flux_msg_handler_t *w,
                             const flux_msg_t *msg, void *arg)
{
    json_t *R = nullptr;
    int saved_errno;
    const char *criteria = nullptr;
    std::shared_ptr<resource_ctx_t> ctx = getctx ((flux_t *)arg);

    if (flux_request_unpack (msg, nullptr, "{s:s}",
                                               "criteria", &criteria) < 0)
        goto error;
    if (run_find (ctx, criteria, &R) < 0)
        goto error;
    if (flux_respond_pack (h, msg, "{s:o?}",
                                       "R", R) < 0) {
        flux_log_error (h, "%s: flux_respond_pack", __FUNCTION__);
        goto error;
    }

    flux_log (h, LOG_DEBUG, "%s: find succeeded", __FUNCTION__);
    return;

error:
    saved_errno = errno;
    json_decref (R);
    errno = saved_errno;
    if (flux_respond_error (h, msg, errno, nullptr) < 0)
        flux_log_error (h, "%s: flux_respond_error", __FUNCTION__);
}

static void status_request_cb (flux_t *h, flux_msg_handler_t *w,
                               const flux_msg_t *msg, void *arg)
{
    int saved_errno;
    json_t *R_all = nullptr;
    json_t *R_down = nullptr;
    json_t *R_alloc = nullptr;
    std::shared_ptr<resource_ctx_t> ctx = getctx ((flux_t *)arg);

    if (run_find (ctx, "status=up or status=down", &R_all) < 0)
        goto error;
    if (run_find (ctx, "status=down", &R_down) < 0)
        goto error;
    if (run_find (ctx, "sched-now=allocated", &R_alloc) < 0)
        goto error;
    if (flux_respond_pack (h, msg, "{s:o? s:o? s:o?}",
                                       "all", R_all,
                                       "down", R_down,
                                       "allocated", R_alloc) < 0) {
        flux_log_error (h, "%s: flux_respond_pack", __FUNCTION__);
        goto error;
    }

    flux_log (h, LOG_DEBUG, "%s: status succeeded", __FUNCTION__);
    return;

error:
    saved_errno = errno;
    json_decref (R_all);
    json_decref (R_alloc);
    json_decref (R_down);
    errno = saved_errno;
    if (flux_respond_error (h, msg, errno, nullptr) < 0)
        flux_log_error (h, "%s: flux_respond_error", __FUNCTION__);
}


/******************************************************************************
 *                                                                            *
 *                               Module Main                                  *
 *                                                                            *
 ******************************************************************************/

extern "C" int mod_main (flux_t *h, int argc, char **argv)
{
    int rc = -1;
    try {
        std::shared_ptr<resource_ctx_t> ctx = nullptr;
        uint32_t rank = 1;

        if ( !(ctx = init_module (h, argc, argv))) {
            flux_log (h, LOG_ERR, "%s: can't initialize resource module",
                      __FUNCTION__);
            goto done;
        }
        // Because mod_main is always active, the following is safe.
        flux_aux_set (h, "sched-fluxion-resource", &ctx, NULL);
        flux_log (h, LOG_DEBUG, "%s: resource module starting", __FUNCTION__);

        /* Before beginning synchronous resource.acquire RPC, set module status
         * to 'running' to let flux module load return success.
         */
        if ( (rc = flux_module_set_running (ctx->h)) < 0) {
            flux_log_error (ctx->h, "%s: flux_module_set_running",
                            __FUNCTION__);
            goto done;
        }
        if ( (rc = init_resource_graph (ctx)) != 0) {
            flux_log (h, LOG_ERR,
                      "%s: can't initialize resource graph database",
                      __FUNCTION__);
            goto done;
        }
        flux_log (h, LOG_DEBUG, "%s: resource graph database loaded",
                  __FUNCTION__);

        if (( rc = flux_reactor_run (flux_get_reactor (h), 0)) < 0) {
            flux_log (h, LOG_ERR, "%s: flux_reactor_run: %s",
                      __FUNCTION__, strerror (errno));
            goto done;
        }
    }
    catch (std::exception &e) {
        flux_log_error (h, "%s: %s", __FUNCTION__, e.what ());
    }

done:
    return rc;
}

MOD_NAME ("sched-fluxion-resource");

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

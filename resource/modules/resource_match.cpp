/*****************************************************************************\
 * Copyright 2014 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#include <cstdint>
#include <limits>
#include <sstream>
#include <cerrno>
#include <map>
#include <cinttypes>
#include <chrono>

extern "C" {
#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <flux/core.h>
#include <flux/idset.h>
#include <jansson.h>
}

#include "resource/schema/resource_graph.hpp"
#include "resource/readers/resource_reader_factory.hpp"
#include "resource/traversers/dfu.hpp"
#include "resource/jobinfo/jobinfo.hpp"
#include "resource/policies/dfu_match_policy_factory.hpp"
#include "resource_match_opts.hpp"

using namespace Flux::resource_model;
using namespace Flux::opts_manager;

/******************************************************************************
 *                                                                            *
 *                Resource Matching Service Module Context                    *
 *                                                                            *
 ******************************************************************************/

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
    Flux::opts_manager::optmgr_composer_t<
        Flux::opts_manager::resource_opts_t> opts; /* Option manager */
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

static void match_multi_request_cb (flux_t *h, flux_msg_handler_t *w,
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

static void ns_info_request_cb (flux_t *h, flux_msg_handler_t *w,
                                const flux_msg_t *msg, void *arg);

static void satisfiability_request_cb (flux_t *h, flux_msg_handler_t *w,
                                       const flux_msg_t *msg, void *arg);

static void params_request_cb (flux_t *h, flux_msg_handler_t *w,
                               const flux_msg_t *msg, void *arg);

static const struct flux_msg_handler_spec htab[] = {
    { FLUX_MSGTYPE_REQUEST,
      "sched-fluxion-resource.match", match_request_cb, 0 },
    { FLUX_MSGTYPE_REQUEST,
       "sched-fluxion-resource.match_multi", match_multi_request_cb, 0 },
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
    { FLUX_MSGTYPE_REQUEST,
      "sched-fluxion-resource.ns-info", ns_info_request_cb, 0 },
    { FLUX_MSGTYPE_REQUEST,
      "sched-fluxion-resource.satisfiability", satisfiability_request_cb, 0 },
    { FLUX_MSGTYPE_REQUEST,
      "sched-fluxion-resource.params", params_request_cb, 0 },
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

static void set_default_args (std::shared_ptr<resource_ctx_t> &ctx)
{
    resource_opts_t ct_opts;
    ct_opts.set_load_format ("rv1exec");
    ct_opts.set_match_subsystems ("containment");
    ct_opts.set_match_policy ("first");
    ct_opts.set_prune_filters ("ALL:core");
    ct_opts.set_match_format ("rv1_nosched");
    ctx->opts += ct_opts;
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
        set_default_args (ctx);
        ctx->perf.load = 0.0f;
        ctx->perf.njobs = 0;
        ctx->perf.min = std::numeric_limits<double>::max();
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
    optmgr_kv_t<resource_opts_t> opts_store;
    std::string info_str = "";

    for (int i = 0; i < argc; i++) {
        const std::string kv (argv[i]);
        if ( (rc = opts_store.put (kv)) < 0) {
            flux_log_error (ctx->h, "%s: optmgr_kv_t::put (%s)",
                             __FUNCTION__, argv[i]);
            return rc;
        }
    }
    if ( (rc = opts_store.parse (info_str)) < 0) {
        flux_log_error (ctx->h, "%s: optmgr_kv_t::parse: %s",
                        __FUNCTION__, info_str.c_str ());
        return rc;
    }
    if (info_str != "") {
        flux_log (ctx->h, LOG_DEBUG, "%s: %s", __FUNCTION__, info_str.c_str ());
    }
    ctx->opts += opts_store.get_opt ();
    return rc;
}

static int process_config_file (std::shared_ptr<resource_ctx_t> &ctx)
{
    int rc = 0;
    json_t *conf = nullptr;

    if ( (rc = flux_conf_unpack (flux_get_conf (ctx->h), nullptr,
                                     "{ s?:o }",
                                         "sched-fluxion-resource",
                                         &conf)) < 0) {
        flux_log_error (ctx->h, "%s: flux_conf_unpack", __FUNCTION__);
        return rc;
    }

    const char *k = nullptr;
    char *tmp = nullptr;
    json_t *v = nullptr;
    optmgr_kv_t<resource_opts_t> opts_store;
    std::string info_str = "";
    json_object_foreach (conf, k, v) {
        std::string value;
        if (!(tmp  = json_dumps (v, JSON_ENCODE_ANY|JSON_COMPACT))) {
            errno = ENOMEM;
            return -1;
        }
        value = tmp;
        free (tmp);
        tmp = nullptr;
        if (json_typeof (v) == JSON_STRING)
            value = value.substr (1, value.length () - 2);
        if ( (rc = opts_store.put (k, value)) < 0) {
            flux_log_error (ctx->h, "%s: optmgr_kv_t::put (%s, %s)",
                             __FUNCTION__, k, value.c_str ());
            return rc;
        }
    }
    if ( (rc = opts_store.parse (info_str)) < 0) {
        flux_log_error (ctx->h, "%s: optmgr_kv_t::parse: %s",
                        __FUNCTION__, info_str.c_str ());
        return rc;
    }
    if (info_str != "") {
        flux_log (ctx->h, LOG_DEBUG, "%s: %s", __FUNCTION__, info_str.c_str ());
    }
    ctx->opts += opts_store.get_opt ();
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
    if (process_config_file (ctx) < 0) {
        flux_log_error (h, "%s: config file parsing", __FUNCTION__);
        goto error;
    }
    if (process_args (ctx, argc, argv) < 0) {
        flux_log_error (h, "%s: load line argument parsing", __FUNCTION__);
        goto error;
    }
    ctx->opts.canonicalize ();
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

static int create_reader (std::shared_ptr<resource_ctx_t> &ctx,
                          const std::string &format)
{
    if ( (ctx->reader = create_resource_reader (format)) == nullptr)
        return -1;
    if (ctx->opts.get_opt ().is_load_allowlist_set ()) {
        if (ctx->reader->set_allowlist (
                             ctx->opts.get_opt ().get_load_allowlist ()) < 0)
            flux_log (ctx->h, LOG_ERR, "%s: setting allowlist", __FUNCTION__);
        if (!ctx->reader->is_allowlist_supported ())
            flux_log (ctx->h, LOG_WARNING, "%s: allowlist unsupported",
                      __FUNCTION__);
    }
    return 0;
}

static int populate_resource_db_file (std::shared_ptr<resource_ctx_t> &ctx)
{
    int rc = -1;
    int saved_errno;
    std::ifstream in_file;
    std::stringstream buffer{};
    graph_duration_t duration;

    if (ctx->reader == nullptr
        && create_reader (ctx, ctx->opts.get_opt ().get_load_format ()) < 0) {
        flux_log (ctx->h, LOG_ERR, "%s: can't create reader", __FUNCTION__);
        goto done;
    }

    saved_errno = errno;
    errno = 0;
    in_file.open (ctx->opts.get_opt ().get_load_file ().c_str (),
                  std::ifstream::in);
    if (!in_file.good ()) {
        if (errno == 0) {
            // C++ standard doesn't guarantee to set errno but
            // many of the underlying system call set it appropriately.
            // we manually set errno only when it is not set at all.
            errno = EIO;
        }
        flux_log_error (ctx->h, "%s: opening %s", __FUNCTION__,
                                ctx->opts.get_opt ().get_load_file ().c_str ());
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
    ctx->db->metadata.set_graph_duration (duration);

    rc = 0;

done:
    return rc;
}

/* Add resources associated with 'rank' execution target,
 * defined by hwloc_xml.  This function may be called with
 * rank == IDSET_INVALID_ID, to instantiate an empty graph.
 */
static int grow (std::shared_ptr<resource_ctx_t> &ctx,
                 vtx_t v, unsigned int rank, const char *hwloc_xml)
{
    int rc = -1;
    resource_graph_db_t &db = *(ctx->db);

    if (rank == IDSET_INVALID_ID) {
        // Grow cluster vertex and leave
        if ( (rc = db.load ("", ctx->reader, rank)) < 0) {
            flux_log (ctx->h, LOG_ERR, "%s: reader: %s",
                      __FUNCTION__,  ctx->reader->err_message ().c_str ());
        }
        goto ret;
    }

    if (v == boost::graph_traits<resource_graph_t>::null_vertex ()) {
        if ( (rc = db.load (hwloc_xml, ctx->reader, rank)) < 0) {
            flux_log (ctx->h, LOG_ERR, "%s: reader: %s",
                      __FUNCTION__,  ctx->reader->err_message ().c_str ());
            goto ret;
        }
    } else {
        if ( (rc = db.load (hwloc_xml, ctx->reader, v, rank)) < 0) {
            flux_log (ctx->h, LOG_ERR, "%s: reader: %s",
                      __FUNCTION__,  ctx->reader->err_message ().c_str ());
            goto ret;
        }
   }

ret:
    return rc;
}

static const char *get_array_string (json_t *array, size_t index)
{
    json_t *entry;
    const char *s;

    if (!(entry = json_array_get (array, index))
            || !(s = json_string_value (entry))) {
        errno = EINVAL;
        return NULL;
    }
    return s;
}

static int expand_ids (const char *resources, std::vector<uint64_t> &id_vec)
{
    int rc = -1;
    struct idset *ids = NULL;
    try {
        unsigned int id;
        if ( !(ids = idset_decode (resources)))
            goto inval;
        if ( (id = idset_first (ids)) == IDSET_INVALID_ID)
            goto inval;
        id_vec.push_back (id);
        while ( (id = idset_next (ids, id)) != IDSET_INVALID_ID)
            id_vec.push_back (id);
        rc = 0;
    } catch (std::bad_alloc &) {
        errno = ENOMEM;
        goto ret;
    }

inval:
    errno = EINVAL;
ret:
    idset_destroy (ids);
    return rc;
}

/* Given 'resobj' in Rv1 form, decode the set of execution target ranks
 * contained in it as well as r_lite key.
 */
static int unpack_resources (json_t *resobj, struct idset **idset,
                             json_t **r_lite_p, json_t **jgf_p,
                             graph_duration_t &duration)
{
    int rc = 0;
    struct idset *ids;
    int version;
    double start = 0.0, end = 0.0;
    json_t *r_lite = NULL;
    json_t *jgf = NULL;
    size_t index;
    json_t *val;

    if (!(ids = idset_create (0, IDSET_FLAG_AUTOGROW)))
        return -1;
    if (resobj) {
        if (json_unpack (resobj,
                         "{s:i s:{s:o s?F s?F} s?:o}",
                         "version", &version,
                         "execution",
                           "R_lite", &r_lite,
                           "starttime", &start,
                           "expiration", &end,
                         "scheduling", &jgf) < 0)
            goto inval;
        // flux-core validates these numbers, but checking here
        // in case Fluxin is plugged into another resource manager
        if (version != 1)
            goto inval;
        if (start != 0 && (start == end))
            goto inval;
        if (start < 0.0 || end < 0.0)
            goto inval;
        if (start > end)
            goto inval;
        // Greatest lower bound is int64_t in the rest of Fluxion
        if (start > static_cast<double> (std::numeric_limits<int64_t>::max ())
                  || end > static_cast<double>
                                       (std::numeric_limits<int64_t>::max ()))
            goto inval;
        // Ensure start and end are representable in system clock
        if (start > static_cast<double>
            (std::chrono::duration_cast<std::chrono::seconds>
                (std::chrono::time_point<std::chrono::system_clock>::max ().time_since_epoch ()).count ())
              || end > static_cast<double>
                (std::chrono::duration_cast<std::chrono::seconds>
                    (std::chrono::time_point<std::chrono::system_clock>::max ().time_since_epoch ()).count ()))
            goto inval;
        // Expects int type argument
        duration.graph_start = std::chrono::system_clock::from_time_t (static_cast<int64_t> (start));
        duration.graph_end = std::chrono::system_clock::from_time_t (static_cast<int64_t> (end));
        // Ensure there is no overflow in system clock representation
        // (should be handled by previous check).
        if (std::chrono::duration_cast<std::chrono::seconds>
                        (duration.graph_start.time_since_epoch ()).count () < 0
            || std::chrono::duration_cast<std::chrono::seconds>
                       (duration.graph_end.time_since_epoch ()).count () < 0) {
            goto inval;
        }
        json_array_foreach (r_lite, index, val) {
            struct idset *r_ids;
            unsigned long id;
            const char *rank;

            if (json_unpack (val, "{s:s}", "rank", &rank) < 0)
                goto inval;
            if (!(r_ids = idset_decode (rank)))
                goto error;
            id = idset_first (r_ids);
            while (id != IDSET_INVALID_ID) {
                if (idset_set (ids, id) < 0) {
                    idset_destroy (r_ids);
                    goto error;
                }
                id = idset_next (r_ids, id);
            }
            idset_destroy (r_ids);
        }
    }
    *idset = ids;
    *r_lite_p = r_lite;
    *jgf_p = jgf;
    return 0;
inval:
    errno = EINVAL;
error:
    idset_destroy (ids);
    return -1;
}

static int unpack_resobj (json_t *resobj,
                          std::map<const distinct_range_t,
                                   std::shared_ptr<resobj_t>> &out)
{
    if (!resobj)
        goto inval;
    try {
        size_t index;
        json_t *val;

        json_array_foreach (resobj, index, val) {
            std::string range;
            std::istringstream istr;
            const char *rank = NULL, *core = NULL, *gpu = NULL;
            if (json_unpack (val, "{s:s s:{s?:s s?:s}}",
                                    "rank", &rank,
                                    "children",
                                      "core", &core,
                                      "gpu", &gpu) < 0)
                goto inval;
            // Split the rank idset in resobj, convert each entry into
            // distict_range_t and use it as the key to std::map.
            // The value is the pointer to resobj_t.
            // The distinct_range_t class provides ordering logic such that
            // you will be able to iterate through ranges in strictly
            // ascending order.
            istr.str (rank);
            while (std::getline (istr, range, ',')) {
                uint64_t low, high;
                std::pair<std::map<const distinct_range_t,
                                   std::shared_ptr<resobj_t>>::iterator, bool> res;
                std::shared_ptr<resobj_t> robj = std::make_shared<resobj_t> ();
                if (distinct_range_t::get_low_high (range, low, high) < 0)
                    goto error;
                robj->exec_target_range = range;
                if (core && expand_ids (core, robj->core) < 0)
                    goto error;
                if (gpu && expand_ids (gpu, robj->gpu) < 0)
                    goto error;
                res = out.insert (std::pair<const distinct_range_t,
                                  std::shared_ptr<resobj_t>> (
                                      distinct_range_t{low, high}, robj));
                if (res.second == false) {
                    errno = EEXIST;
                    goto error;
                }
            }
        }
        return 0;
    } catch (std::bad_alloc &) {
        errno = ENOMEM;
        goto error;
    } catch (std::invalid_argument &) {
        goto inval;
    }
inval:
    errno = EINVAL;
error:
    return -1;
}

static int remap_hwloc_namespace (std::shared_ptr<resource_ctx_t> &ctx,
                                  json_t *r_lite)
{
    std::map<const distinct_range_t, std::shared_ptr<resobj_t>> resobjs;
    if (unpack_resobj (r_lite, resobjs) < 0)
        return -1;
    for (auto &kv : resobjs) {
        /* hwloc reader only needs to remap gpu IDs */
        size_t logical;
        for (logical = 0; logical < kv.second->gpu.size (); logical++) {
            if (ctx->reader->namespace_remapper.add (
                                 kv.second->exec_target_range,
                                 "gpu",
                                 logical,
                                 kv.second->gpu[logical]) < 0)
                return -1;
        }
    }
    return 0;
}

static int remap_jgf_namespace (std::shared_ptr<resource_ctx_t> &ctx,
                                json_t *resobj, json_t *p_resobj)
{
    size_t i, j;
    uint64_t cur_rank = 0;
    std::map<const distinct_range_t, std::shared_ptr<resobj_t>> robjs, p_robjs;

    if (unpack_resobj (resobj, robjs) < 0)
        goto error;
    if (unpack_resobj (p_resobj, p_robjs) < 0)
        goto error;

    try {
        // Add remap rule for rank and compute core Ids. JGF reader only
        // needs to remap them (e.g., GPU uses Id in global scope).
        // This loop iterates through the distinct rank ranges of R_lite in
        // the parent's namespace in ascending order.
        // We first translate the ascending rank range sequence into
        // 0-based monotonically increasing rank range sequence, for example
        // A: <"3", "5", "6", "20-21"> --> B: <"1", "2", "3", "4-5">.
        // Now, per core resource.acquire's remapping rule, for each range found
        // in the parent namespace's R_lite, you should be able to find a
        // corresponding range in the current namespace's R_lite.
        // The current R_lite's range sequence for the above example might
        // be C: <"1-2", "3", "4-5">. Then, B's "1" and "2" will be resolved
        // to C's "1-2" range. B's "3" to C's "3". And B's "4-5" to C's "4-5".
        for (auto kv : p_robjs) {
            distinct_range_t new_range{cur_rank,
                                       kv.first.get_high ()
                                           - kv.first.get_low () + cur_rank};
            std::shared_ptr<resobj_t> entry = nullptr;
            std::shared_ptr<resobj_t> p_entry = kv.second;
            if (robjs.find (new_range) == robjs.end ()) {
                // Can't find the rank range in the current namespace
                // corresponding to the (remapped) range of the parent namespace!
                errno = ENOENT;
                goto error;
            }
            entry = robjs[new_range];
            if (ctx->reader->namespace_remapper.add_exec_target_range (
                                                    p_entry->exec_target_range,
                                                    new_range) < 0)
                goto error;
            for (j = 0; j < p_entry->core.size (); j++) {
                if (ctx->reader->namespace_remapper.add (
                                     p_entry->exec_target_range, "core",
                                     p_entry->core[j], entry->core[j]) < 0)
                    goto error;
            }
            cur_rank += (kv.first.get_high () - kv.first.get_low () + 1);
        }
    } catch (std::invalid_argument &) {
        errno = EINVAL;
        goto error;
    }
    return 0;
error:
    return -1;
}

/* Grow resources for execution targets 'ids', fetching resource
 * details in hwloc XML form from the core resource module.
 * If 'ids' is the empty set, an empty resource vertex will be instantiated.
 */
static int grow_resource_db_hwloc (std::shared_ptr<resource_ctx_t> &ctx,
                                   struct idset *ids, json_t *resobj)
{
    int rc = -1;
    resource_graph_db_t &db = *(ctx->db);
    unsigned int rank = idset_first (ids);
    vtx_t v = boost::graph_traits<resource_graph_t>::null_vertex ();
    flux_future_t *f = NULL;
    json_t *xml_array;
    const char *hwloc_xml;

    if (!(f = flux_rpc (ctx->h, "resource.get-xml", NULL, 0, 0)))
        goto done;
    if (flux_rpc_get_unpack (f, "{s:o}", "xml", &xml_array) < 0) {
        flux_log (ctx->h, LOG_ERR, "%s", future_strerror (f, errno));
        goto done;
    }
    if (db.metadata.roots.find ("containment") == db.metadata.roots.end ()) {
        if (rank != IDSET_INVALID_ID) {
            if (!(hwloc_xml = get_array_string (xml_array, rank)))
                goto done;
        }
        else
            hwloc_xml = NULL;
        // before hwloc reader is used, set remap
        if ( (rc = remap_hwloc_namespace (ctx, resobj)) < 0)
            goto done;
        if ( (rc = grow (ctx, v, rank, hwloc_xml)) < 0)
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
        if (!(hwloc_xml = get_array_string (xml_array, rank)))
            goto done;
        if ( (rc = grow (ctx, v, rank, hwloc_xml)) < 0)
            goto done;
        rank = idset_next (ids, rank);
    }

    flux_log (ctx->h, LOG_DEBUG,
              "resource graph datastore loaded with hwloc reader");

done:
    flux_future_destroy (f);
    return rc;
}

static int grow_resource_db_rv1exec (std::shared_ptr<resource_ctx_t> &ctx,
                                     struct idset *ids, json_t *resobj)
{
    int rc = -1;
    int saved_errno;
    resource_graph_db_t &db = *(ctx->db);
    char *rv1_str = nullptr;

    if (db.metadata.roots.find ("containment") == db.metadata.roots.end ()) {
        if ( (rv1_str = json_dumps (resobj, JSON_INDENT (0))) == NULL) {
            errno = ENOMEM;
            goto done;
        }
        if ( (rc = db.load (rv1_str, ctx->reader, -1)) < 0) {
            flux_log_error (ctx->h, "%s: db.load: %s",
                            __FUNCTION__, ctx->reader->err_message ().c_str ());
            goto done;
        }
        flux_log (ctx->h, LOG_DEBUG,
                  "resource graph datastore loaded with rv1exec reader");
    }
done:
    saved_errno = errno;
    free (rv1_str);
    errno = saved_errno;
    return rc;
}

static int get_parent_job_resources (std::shared_ptr<resource_ctx_t> &ctx,
                                     json_t **resobj_p)
{
    int rc = -1;
    json_t *resobj;
    json_error_t json_err;
    flux_jobid_t id;
    flux_future_t *f = NULL;
    flux_t *parent_h = NULL;
    const char *uri, *jobid, *resobj_str;

    if ( !(uri = flux_attr_get (ctx->h, "parent-uri")))
        return 0;
    if ( !(jobid = flux_attr_get (ctx->h, "jobid")))
        return 0;
    if (flux_job_id_parse (jobid, &id) < 0) {
        flux_log_error (ctx->h, "%s: parsing jobid %s", __FUNCTION__, jobid);
        return -1;
    }
    if ( !(parent_h = flux_open (uri, 0))) {
        flux_log_error (ctx->h, "%s: flux_open (%s)", __FUNCTION__, uri);
        goto done;
    }
    if ( !(f = flux_rpc_pack (parent_h, "job-info.lookup", FLUX_NODEID_ANY, 0,
                                          "{s:I s:[s] s:i}",
                                            "id", id,
                                            "keys", "R",
                                            "flags", 0))) {
        flux_log_error (ctx->h, "%s: flux_rpc_pack (R)", __FUNCTION__);
        goto done;
    }
    if (flux_rpc_get_unpack (f, "{s:s}", "R", &resobj_str) < 0) {
        flux_log_error (ctx->h, "%s: flux_rpc_get_unpack (R)", __FUNCTION__);
        goto done;
    }
    if ( !(resobj = json_loads (resobj_str, 0, &json_err))) {
        errno = ENOMEM;
        flux_log (ctx->h, LOG_ERR, "%s: json_loads", __FUNCTION__);
        goto done;
    }
    *resobj_p = resobj;
    rc = 0;
done:
    flux_future_destroy (f);
    flux_close (parent_h);
    return rc;
}

static int unpack_parent_job_resources (std::shared_ptr<resource_ctx_t> &ctx,
                                        json_t **p_r_lite_p)
{
    int rc = 0;
    int saved_errno;
    // Unused: necessary to avoid function overload and duplicated code
    graph_duration_t duration;
    json_t *p_jgf = NULL;
    json_t *p_r_lite = NULL;
    json_t *p_resources = NULL;
    struct idset *p_grow_set = NULL;
    if ( (rc = get_parent_job_resources (ctx, &p_resources)) < 0
         || !p_resources)
        goto done;
    if ( (rc = unpack_resources (p_resources, &p_grow_set, &p_r_lite,
                                 &p_jgf, duration)) < 0)
        goto done;
    if (!p_grow_set || !p_r_lite || !p_jgf) {
        errno = EINVAL;
        rc = -1;
        goto done;
    }
    if ( (*p_r_lite_p = json_deep_copy (p_r_lite)) == NULL) {
        errno = ENOMEM;
        rc = -1;
        goto done;
    }
    rc = 0;
done:
    saved_errno = errno;
    json_decref (p_resources);
    idset_destroy (p_grow_set);
    errno = saved_errno;
    return rc;
}

static int grow_resource_db_jgf (std::shared_ptr<resource_ctx_t> &ctx,
                                 json_t *r_lite, json_t *jgf)
{
    int rc = -1;
    int saved_errno;
    json_t *p_r_lite = NULL;
    resource_graph_db_t &db = *(ctx->db);
    char *jgf_str = NULL;
    vtx_t v = boost::graph_traits<resource_graph_t>::null_vertex ();

    if ( (rc = unpack_parent_job_resources (ctx, &p_r_lite)) < 0) {
        flux_log_error (ctx->h, "%s: unpack_parent_job_resources",
                        __FUNCTION__);
        goto done;
    }
    if (db.metadata.roots.find ("containment") == db.metadata.roots.end ()) {
        if ( p_r_lite
             && (rc = remap_jgf_namespace (ctx, r_lite, p_r_lite)) < 0) {
            flux_log_error (ctx->h, "%s: remap_jgf_namespace", __FUNCTION__);
            goto done;
        }
        if ( (jgf_str = json_dumps (jgf, JSON_INDENT (0))) == NULL) {
            rc = -1;
            errno = ENOMEM;
            goto done;
        }
        if ( (rc = db.load (jgf_str, ctx->reader, -1)) < 0) {
            flux_log_error (ctx->h, "%s: db.load: %s",
                            __FUNCTION__, ctx->reader->err_message ().c_str ());
            goto done;
        }
    }

    flux_log (ctx->h, LOG_DEBUG,
              "resource graph datastore loaded with JGF reader");

done:
    saved_errno = errno;
    json_decref (p_r_lite);
    free (jgf_str);
    errno = saved_errno;
    return rc;
}

static int grow_resource_db (std::shared_ptr<resource_ctx_t> &ctx,
                             json_t *resources)
{
    int rc = 0;
    graph_duration_t duration;
    struct idset *grow_set = NULL;
    json_t *r_lite = NULL;
    json_t *jgf = NULL;

    if ( (rc = unpack_resources (resources,
                                 &grow_set, &r_lite, &jgf, duration)) < 0) {
        flux_log_error (ctx->h, "%s: unpack_resources", __FUNCTION__);
        goto done;
    }
    if (jgf) {
        if (ctx->reader == nullptr && (rc = create_reader (ctx, "jgf")) < 0) {
            flux_log (ctx->h, LOG_ERR, "%s: can't create jgf reader",
                      __FUNCTION__);
            goto done;
        }
        rc = grow_resource_db_jgf (ctx, r_lite, jgf);
    } else {
        if (ctx->opts.get_opt ().get_load_format () == "hwloc") {
             if ( !ctx->reader && (rc = create_reader (ctx, "hwloc")) < 0) {
                 flux_log (ctx->h, LOG_ERR, "%s: can't create hwloc reader",
                           __FUNCTION__);
                 goto done;
             }
             rc = grow_resource_db_hwloc (ctx, grow_set, r_lite);
        } else if (ctx->opts.get_opt ().get_load_format () == "rv1exec") {
             if ( !ctx->reader && (rc = create_reader (ctx, "rv1exec")) < 0) {
                 flux_log (ctx->h, LOG_ERR, "%s: can't create rv1exec reader",
                           __FUNCTION__);
                 goto done;
             }
             rc = grow_resource_db_rv1exec (ctx, grow_set, resources);
        } else {
            errno = EINVAL;
            rc = -1;
        }
    }
    ctx->db->metadata.set_graph_duration (duration);

done:
    idset_destroy (grow_set);
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
                               json_t *resources,
                               const char *up, const char *down)
{
    int rc = 0;
    // Will need to get duration update and set graph metadata when
    // resource.acquire duration update is supported in the future.
    if (resources && (rc = grow_resource_db (ctx, resources)) < 0) {
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

static void update_resource (flux_future_t *f, void *arg)
{
    int rc = -1;
    const char *up = NULL;
    const char *down = NULL;
    json_t *resources = NULL;
    std::shared_ptr<resource_ctx_t> ctx = getctx ((flux_t *)arg);

    if ( (rc = flux_rpc_get_unpack (f, "{s?:o s?:s s?:s}",
                                           "resources", &resources,
                                           "up", &up,
                                           "down", &down)) < 0) {
        flux_log_error (ctx->h, "%s: exiting due to resource.acquire failure",
                        __FUNCTION__);
        flux_reactor_stop (flux_get_reactor (ctx->h));
        goto done;
    }
    if ( (rc = update_resource_db (ctx, resources, up, down)) < 0) {
        flux_log_error (ctx->h, "%s: update_resource_db", __FUNCTION__);
        goto done;
    }
    for (auto &kv : ctx->notify_msgs) {
        if ( (rc += flux_respond (ctx->h, kv.second->get_msg (), NULL)) < 0) {
            flux_log_error (ctx->h, "%s: flux_respond", __FUNCTION__);
        }
    }
done:
    flux_future_reset (f);
    ctx->set_update_rc (rc);
}

static int populate_resource_db_acquire (std::shared_ptr<resource_ctx_t> &ctx)
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

    if (ctx->opts.get_opt ().is_reserve_vtx_vec_set ())
        ctx->db->resource_graph.m_vertices.reserve (
            ctx->opts.get_opt ().get_reserve_vtx_vec ());
    if ( (rc = gettimeofday (&st, NULL)) < 0) {
        flux_log_error (ctx->h, "%s: gettimeofday", __FUNCTION__);
        goto done;
    }
    if (ctx->opts.get_opt ().is_load_file_set ()) {
        if (populate_resource_db_file (ctx) < 0)
            goto done;
        flux_log (ctx->h, LOG_INFO, "%s: loaded resources from %s",
                  __FUNCTION__,
                  ctx->opts.get_opt ().get_load_file ().c_str ());
    } else {
        if (populate_resource_db_acquire (ctx) < 0) {
            flux_log (ctx->h, LOG_ERR,
                      "%s: loading resources using resource.acquire",
                      __FUNCTION__);
            goto done;
        }
        flux_log (ctx->h, LOG_INFO,
                  "%s: loaded resources from core's resource.acquire",
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
    std::stringstream ss (ctx->opts.get_opt ().get_match_subsystems ());
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
    if ( !(ctx->matcher = create_match_cb (
                              ctx->opts.get_opt ().get_match_policy ()))) {
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
                  __FUNCTION__,
                  ctx->opts.get_opt ().get_match_subsystems ().c_str ());
        return rc;
    }
    if ( !(ctx->fgraph = create_filtered_graph (ctx)))
        return -1;

    // Create a writers object for matched vertices and edges
    match_format_t format = match_writers_factory_t::get_writers_type (
                                ctx->opts.get_opt ().get_match_format ());
    if ( !(ctx->writers = match_writers_factory_t::create (format)))
        return -1;

    if (ctx->opts.get_opt ().is_prune_filters_set ()
        && ctx->matcher->set_pruning_types_w_spec (
                             ctx->matcher->dom_subsystem (),
                             ctx->opts.get_opt ().get_prune_filters ()) < 0) {
        flux_log (ctx->h, LOG_ERR, "%s: error setting pruning types with: %s",
                  __FUNCTION__,
                  ctx->opts.get_opt ().get_prune_filters ().c_str ());
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
    // Use ctx->update_f != nullptr to differentiate
    if (ctx->update_f) {
        if (mark (ctx, "all", resource_pool_t::status_t::DOWN) < 0) {
            flux_log (ctx->h, LOG_ERR, "%s: mark (down)", __FUNCTION__);
            return -1;
        }
        if (ctx->is_ups_set ()) {
            if (mark (ctx, ctx->get_ups ().c_str (),
                      resource_pool_t::status_t::UP) < 0) {
                flux_log (ctx->h, LOG_ERR, "%s: mark (up)", __FUNCTION__);
                return -1;
            }
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
    int rc = -1;
    try {
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
        else if (std::string ("satisfiability") == cmd)
            rc = tr.run (j, ctx->writers, match_op_t::MATCH_SATISFIABILITY,
                         jobid, at);
        else
            errno = EINVAL;
    } catch (const Flux::Jobspec::parse_error& e) {
        errno = EINVAL;
    }

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
        && strcmp ("allocate_with_satisfiability", cmd) != 0
        && strcmp ("satisfiability", cmd) != 0) {
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

    if (cmd != std::string ("satisfiability")) {
        if ( (rc = track_schedule_info (ctx, jobid,
                                        rsv, *at, jstr, o, *ov)) != 0) {
            flux_log_error (ctx->h, "%s: can't add job info (id=%jd)",
                            __FUNCTION__, (intmax_t)jobid);
            goto done;
        }
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

static void match_multi_request_cb (flux_t *h, flux_msg_handler_t *w,
                                    const flux_msg_t *msg, void *arg)
{
    size_t index;
    json_t *value;
    json_error_t err;
    int saved_errno;
    json_t *jobs = nullptr;
    uint64_t jobid = 0;
    std::string errmsg;
    const char *cmd = nullptr;
    const char *jobs_str = nullptr;
    std::shared_ptr<resource_ctx_t> ctx = getctx ((flux_t *)arg);

    if (!flux_msg_is_streaming (msg)) {
        errno = EPROTO;
        goto error;
    }
    if (flux_request_unpack (msg, NULL, "{s:s s:s}",
                                          "cmd", &cmd,
                                          "jobs", &jobs_str) < 0)
        goto error;
    if ( !(jobs = json_loads (jobs_str, 0, &err))) {
        errno = ENOMEM;
        goto error;
    }

    json_array_foreach(jobs, index, value) {
        const char *js_str;
        int64_t at = 0;
        int64_t now = 0;
        double ov = 0.0f;
        std::string status = "";
        std::stringstream R;

        if (json_unpack (value, "{s:I s:s}",
                                  "jobid", &jobid,
                                  "jobspec", &js_str) < 0)
            goto error;
        if (is_existent_jobid (ctx, jobid)) {
            errno = EINVAL;
            flux_log_error (h, "%s: existent job (%jd).",
                            __FUNCTION__, static_cast<intmax_t> (jobid));
            goto error;
        }
        if (run_match (ctx, jobid, cmd, js_str, &now, &at, &ov, R) < 0) {
            if (errno != EBUSY && errno != ENODEV)
                flux_log_error (ctx->h,
                        "%s: match failed due to match error (id=%jd)",
                        __FUNCTION__, static_cast<intmax_t> (jobid));
            goto error;
        }

        status = get_status_string (now, at);
        if (flux_respond_pack (h, msg, "{s:I s:s s:f s:s s:I}",
                                         "jobid", jobid,
                                         "status", status.c_str (),
                                         "overhead", ov,
                                         "R", R.str ().c_str (),
                                         "at", at) < 0) {
            flux_log_error (h, "%s", __FUNCTION__);
            goto error;
        }
    }
    errno = ENODATA;
    jobid = 0;
error:
    if (jobs) {
        saved_errno = errno;
        json_decref (jobs);
        errno = saved_errno;
    }
    if (jobid != 0)
        errmsg += "jobid=" + std::to_string (jobid);
    if (flux_respond_error (h, msg, errno,
                            !errmsg.empty ()? errmsg.c_str () : nullptr) < 0)
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
    std::map<std::string, std::vector<vtx_t>>::const_iterator it;
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

    for (auto &v : it->second) {
        ret = ctx->db->resource_graph[v].properties.insert (
            std::pair<std::string, std::string> (property_key,
                                                 property_value));

        if (ret.second == false) {
            ctx->db->resource_graph[v].properties.erase (property_key);
            ctx->db->resource_graph[v].properties.insert (
                std::pair<std::string, std::string> (property_key,
                                                     property_value));
        }
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
    std::map<std::string, std::vector<vtx_t>>::const_iterator it;
    std::map<std::string, std::string>::const_iterator p_it;
    vtx_t v;
    std::vector<std::string> resp_values;
    json_t *resp_array = nullptr;

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

     for (auto &v : it->second) {
        for (p_it = ctx->db->resource_graph[v].properties.begin ();
             p_it != ctx->db->resource_graph[v].properties.end (); p_it++) {

             if (property_key.compare (p_it->first) == 0)
                 resp_values.push_back (p_it->second);
         }
     }
     if (resp_values.empty ()) {
         errno = ENOENT;
         flux_log_error (h, "%s: Property %s was not found for resource %s.",
                         __FUNCTION__, property_key.c_str (),
                          resource_path.c_str ());
         goto error;
     }

     if ( !(resp_array = json_array ())) {
         errno = ENOMEM;
         goto error;
     }
     for (auto &resp_value : resp_values) {
         json_t *value = nullptr;
         if ( !(value = json_string (resp_value.c_str ()))) {
             errno = EINVAL;
             goto error;
         }
         if (json_array_append_new (resp_array, value) < 0) {
             json_decref (value);
             errno = EINVAL;
             goto error;
         }
     }
     if (flux_respond_pack (h, msg, "{s:o}", "values", resp_array) < 0)
         flux_log_error (h, "%s", __FUNCTION__);

     return;

error:
    if (flux_respond_error (h, msg, errno, NULL) < 0)
        flux_log_error (h, "%s: flux_respond_error", __FUNCTION__);
}

static void disconnect_request_cb (flux_t *h, flux_msg_handler_t *w,
                                   const flux_msg_t *msg, void *arg)
{
    const char *route;
    std::shared_ptr<resource_ctx_t> ctx = getctx ((flux_t *)arg);

    if (!(route = flux_msg_route_first (msg))) {
        flux_log_error (h, "%s: flux_msg_route_first", __FUNCTION__);
        return;
    }
    if (ctx->notify_msgs.find (route) != ctx->notify_msgs.end ()) {
        ctx->notify_msgs.erase (route);
        flux_log (h, LOG_DEBUG, "%s: a notify request aborted", __FUNCTION__);
    }
}

static void notify_request_cb (flux_t *h, flux_msg_handler_t *w,
                               const flux_msg_t *msg, void *arg)
{
    try {
        const char *route;
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
        if (!(route = flux_msg_route_first (msg))) {
            flux_log_error (h, "%s: flux_msg_route_first", __FUNCTION__);
            goto error;
        }

        m->set_msg (msg);
        auto ret = ctx->notify_msgs.insert (
                       std::pair<std::string,
                                 std::shared_ptr<msg_wrap_t>> (route, m));
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
                     const std::string &criteria,
                     const std::string &format_str,
                     json_t **R)
{
    int rc = -1;
    json_t *o = nullptr;
    std::shared_ptr<match_writers_t> w = nullptr;

    match_format_t format = match_writers_factory_t::
        get_writers_type (format_str);
    if ( !(w = match_writers_factory_t::create (format)))
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
    const char *format_str = "rv1_nosched";
    std::shared_ptr<resource_ctx_t> ctx = getctx ((flux_t *)arg);

    if (flux_request_unpack (msg, nullptr,
                                  "{s:s, s?:s}",
                                  "criteria", &criteria,
                                  "format", &format_str) < 0)
        goto error;

    if (run_find (ctx, criteria, format_str, &R) < 0)
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

    if (run_find (ctx, "status=up or status=down", "rv1_nosched", &R_all) < 0)
        goto error;
    if (run_find (ctx, "status=down", "rv1_nosched", &R_down) < 0)
        goto error;
    if (run_find (ctx, "sched-now=allocated", "rv1_nosched", &R_alloc) < 0)
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

static void ns_info_request_cb (flux_t *h, flux_msg_handler_t *w,
                                const flux_msg_t *msg, void *arg)
{
    uint64_t rank, id, remapped_id;
    const char *type_name;
    std::shared_ptr<resource_ctx_t> ctx = getctx ((flux_t *)arg);

    if (flux_request_unpack (msg, nullptr,
                                  "{s:I s:s s:I}",
                                      "rank", &rank,
                                      "type-name", &type_name,
                                      "id", &id) < 0) {
        flux_log_error (h, "%s: flux_respond_unpack", __FUNCTION__);
        goto error;
    }
    if (ctx->reader->namespace_remapper.query (rank, type_name,
                                               id, remapped_id) < 0) {
        flux_log_error (h, "%s: namespace_remapper.query", __FUNCTION__);
        goto error;
    }
    if (remapped_id > std::numeric_limits<int64_t>::max ()) {
        errno = EOVERFLOW;
        flux_log_error (h, "%s: remapped id too large", __FUNCTION__);
        goto error;
    }
    if (flux_respond_pack (h, msg, "{s:I}",
                                       "id", static_cast<int64_t> (
                                                 remapped_id)) < 0) {
        flux_log_error (h, "%s: flux_respond_pack", __FUNCTION__);
        goto error;
    }
    return;

error:
    if (flux_respond_error (h, msg, errno, nullptr) < 0)
        flux_log_error (h, "%s: flux_respond_error", __FUNCTION__);
}

static void satisfiability_request_cb (flux_t *h, flux_msg_handler_t *w,
                                       const flux_msg_t *msg, void *arg)
{
    int64_t at = 0;
    int64_t now = 0;
    double ov = 0.0f;
    int saved_errno = 0;
    std::stringstream R;
    json_t *jobspec = nullptr;
    const char *js_str = nullptr;
    const char *respond_msg = nullptr;
    std::shared_ptr<resource_ctx_t> ctx = getctx ((flux_t *)arg);

    if (flux_request_unpack (msg, NULL, "{s:o}", "jobspec", &jobspec) < 0)
        goto error;
    if ( !(js_str = json_dumps (jobspec, JSON_INDENT (0)))) {
        errno = ENOMEM;
        goto error;
    }
    if (run_match (ctx, -1, "satisfiability", js_str, &now, &at, &ov, R) < 0) {
        if (errno == ENODEV) {
            respond_msg = "Unsatisfiable request";
        } else {
            respond_msg = "Internal match error";
            flux_log_error (ctx->h, "%s: %s", __FUNCTION__, respond_msg);
        }
        goto error_memfree;
    }
    free ((void *)js_str);
    if (flux_respond_pack (h, msg, "{s:i}", "errnum", 0) < 0)
        flux_log_error (h, "%s: flux_respond_pack", __FUNCTION__);
    return;

error_memfree:
    saved_errno = errno;
    free ((void *)js_str);
    errno = saved_errno;
error:
    if (flux_respond_error (h, msg, errno, respond_msg) < 0)
        flux_log_error (h, "%s: flux_respond_error", __FUNCTION__);
}

static void params_request_cb (flux_t *h, flux_msg_handler_t *w,
                                   const flux_msg_t *msg, void *arg)
{
    int saved_errno;
    json_error_t jerr;
    std::string params;
    json_t *o{nullptr};
    std::shared_ptr<resource_ctx_t> ctx = getctx ((flux_t *)arg);

    if (ctx->opts.jsonify (params) < 0)
        goto error;
    if (!(o = json_loads (params.c_str (), 0, &jerr))) {
        errno = ENOMEM;
        goto error;
    }
    if (flux_respond_pack (h, msg, "{s:o}", "params", o) < 0) {
        flux_log_error (h, "%s: flux_respond_pack", __FUNCTION__);
        goto error;
    }

    flux_log (h, LOG_DEBUG, "%s: params succeeded", __FUNCTION__);
    return;

error:
    if (o) {
        saved_errno = errno;
        json_decref (o);
        errno = saved_errno;
    }
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
        errno = ENOSYS;
        flux_log (h, LOG_ERR, "%s: %s", __FUNCTION__, e.what ());
        return -1;
    }
    catch (...) {
        errno = ENOSYS;
        flux_log (h, LOG_ERR, "%s: caught unknown exception", __FUNCTION__);
        return -1;
    }

done:
    return rc;
}

MOD_NAME ("sched-fluxion-resource");

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

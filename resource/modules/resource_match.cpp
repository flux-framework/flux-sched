/*****************************************************************************\
 * Copyright 2014 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

extern "C" {
#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <flux/core.h>
#include <flux/idset.h>
}

#include <cstdint>
#include <fstream>
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

// Global perf struct from schema
extern struct Flux::resource_model::match_perf_t Flux::resource_model::perf;

////////////////////////////////////////////////////////////////////////////////
// Resource Matching Service Module Context
////////////////////////////////////////////////////////////////////////////////

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

resource_interface_t &resource_interface_t::operator= (const resource_interface_t &o)
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
    idset_destroy (m_notify_lost);
}

////////////////////////////////////////////////////////////////////////////////
// Resource Graph and Traverser Initialization
////////////////////////////////////////////////////////////////////////////////

static int create_reader (std::shared_ptr<resource_ctx_t> &ctx, const std::string &format)
{
    if ((ctx->reader = create_resource_reader (format)) == nullptr)
        return -1;
    if (ctx->opts.get_opt ().is_load_allowlist_set ()) {
        if (ctx->reader->set_allowlist (ctx->opts.get_opt ().get_load_allowlist ()) < 0)
            flux_log (ctx->h, LOG_ERR, "%s: setting allowlist", __FUNCTION__);
        if (!ctx->reader->is_allowlist_supported ())
            flux_log (ctx->h, LOG_WARNING, "%s: allowlist unsupported", __FUNCTION__);
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
    in_file.open (ctx->opts.get_opt ().get_load_file ().c_str (), std::ifstream::in);
    if (!in_file.good ()) {
        if (errno == 0) {
            // C++ standard doesn't guarantee to set errno but
            // many of the underlying system call set it appropriately.
            // we manually set errno only when it is not set at all.
            errno = EIO;
        }
        flux_log_error (ctx->h,
                        "%s: opening %s",
                        __FUNCTION__,
                        ctx->opts.get_opt ().get_load_file ().c_str ());
        goto done;
    }
    errno = saved_errno;
    buffer << in_file.rdbuf ();
    in_file.close ();
    if ((rc = ctx->db->load (buffer.str (), ctx->reader)) < 0) {
        flux_log (ctx->h,
                  LOG_ERR,
                  "%s: reader: %s",
                  __FUNCTION__,
                  ctx->reader->err_message ().c_str ());
        goto done;
    }
    ctx->db->metadata.set_graph_duration (duration);

    rc = 0;

done:
    return rc;
}

static void update_resource (flux_future_t *f, void *arg)
{
    int rc = -1;

    char *acquire_str = NULL;
    size_t acquire_str_size;
    json_t *acquire_obj;
    json_error_t error;

    const char *up = NULL;
    const char *down = NULL;
    const char *lost = NULL;
    double expiration = -1.;
    json_t *resources = NULL;

    std::shared_ptr<resource_ctx_t> &ctx = *(static_cast<std::shared_ptr<resource_ctx_t> *> (arg));

    if ((rc = flux_rpc_get_raw (f, (const void **)&acquire_str, &acquire_str_size)) < 0) {
        flux_log_error (ctx->h,
                        ctx->m_acquire_resources_from_core ? "%s: exiting due to resource.acquire "
                                                             "failure"
                                                           : "%s: exiting due to "
                                                             "sched-fluxion-resource.notify "
                                                             "failure",
                        __FUNCTION__);
        flux_reactor_stop (flux_get_reactor (ctx->h)); /* Cancels notify msgs */
        goto done;
    }
    // Ignore empty payloads
    if (!acquire_str) {
        goto done;
    }
    if (!(acquire_obj = json_loadb ((const char *)acquire_str,
                                    acquire_str_size,
                                    JSON_DISABLE_EOF_CHECK,
                                    &error))) {
        rc = json_error_code ((const json_error_t *)&error);
        flux_log_error (ctx->h, "%s: json_loadb: %s", __FUNCTION__, error.text);
        goto done;
    }
    if ((rc = json_unpack_ex (acquire_obj,
                              &error,
                              0,
                              "{s?:o s?:s s?:s s?:s s?:F}",
                              "resources",
                              &resources,
                              "up",
                              &up,
                              "down",
                              &down,
                              "shrink",
                              &lost,
                              "expiration",
                              &expiration))
        < 0) {
        flux_log_error (ctx->h, "%s: json_unpack_ex: %s", __FUNCTION__, error.text);
        goto done;
    }
    if ((rc = update_resource_db (ctx, resources, up, down, lost)) < 0) {
        flux_log_error (ctx->h, "%s: update_resource_db", __FUNCTION__);
        goto done;
    }
    if (expiration > 0.) {
        /*  Update graph duration:
         */
        ctx->db->metadata.graph_duration.graph_end =
            std::chrono::system_clock::from_time_t ((time_t)expiration);
        flux_log (ctx->h, LOG_INFO, "resource expiration updated to %.2f", expiration);
    } else if (expiration == 0.) {
        /*  An expiration of 0 should be treated as "no expiration":
         */
        ctx->db->metadata.graph_duration.graph_end =
            std::chrono::system_clock::from_time_t (detail::SYSTEM_MAX_DURATION);
        flux_log (ctx->h, LOG_INFO, "resource expiration updated to 0. (unlimited)");
    }
    if (ctx->m_acquire_resources_from_core) {
        // Store initial set of resources to broadcast to other fluxion modules
        //  via sched-fluxion-resource.notify
        if (resources != NULL) {
            ctx->m_notify_resources = json::value (resources);
        }
        if (lost != NULL) {
            struct idset *lost_idset = idset_decode (lost);
            if (rc += idset_add (ctx->m_notify_lost, lost_idset)) {
                flux_log (ctx->h, LOG_ERR, "%s: idset_add (lost)", __FUNCTION__);
            }
            idset_destroy (lost_idset);
        }
        if (expiration > 0.) {
            ctx->m_notify_expiration = expiration;
        }

        // Broadcast UP/DOWN/SHRINK updates to subscribed fluxion modules.
        // There are no subscribers until the first notify_request_cb,
        //  which must happen after the first run of update_resource
        for (auto &kv : ctx->notify_msgs) {
            if (rc +=
                flux_respond_raw (ctx->h, kv.second->get_msg (), acquire_str, acquire_str_size)
                < 0) {
                flux_log_error (ctx->h, "%s: flux_respond_raw", __FUNCTION__);
            }
        }
    }
    json_decref (acquire_obj);
done:
    flux_future_reset (f);
    ctx->set_update_rc (rc);
}

static int populate_resource_db_acquire (std::shared_ptr<resource_ctx_t> &ctx)
{
    int rc = -1;
    json_t *o = NULL;

    // If this module is not getting resources from core, use
    //  sched-fluxion-resource.notify instead of resource.acquire to avoid
    //  using more than one resource.acquire RPC, which is not allowed
    if (!(ctx->update_f = flux_rpc (ctx->h,
                                    ctx->m_acquire_resources_from_core ? "resource.acquire"
                                                                       : "sched-fluxion-resource."
                                                                         "notify",
                                    NULL,
                                    FLUX_NODEID_ANY,
                                    FLUX_RPC_STREAMING))) {
        flux_log_error (ctx->h, "%s: flux_rpc", __FUNCTION__);
        goto done;
    }

    update_resource (ctx->update_f, static_cast<void *> (&ctx));

    if ((rc = ctx->fetch_and_reset_update_rc ()) < 0) {
        flux_log_error (ctx->h, "%s: update_resource", __FUNCTION__);
        goto done;
    }
    if (rc = flux_future_then (ctx->update_f, -1.0, update_resource, static_cast<void *> (&ctx))
             < 0) {
        flux_log_error (ctx->h, "%s: flux_future_then", __FUNCTION__);
        goto done;
    }

done:
    return rc;
}

int populate_resource_db (std::shared_ptr<resource_ctx_t> &ctx)
{
    int rc = -1;
    std::chrono::time_point<std::chrono::system_clock> start;
    std::chrono::duration<double> elapsed;

    if (ctx->opts.get_opt ().is_reserve_vtx_vec_set ())
        ctx->db->resource_graph.m_vertices.reserve (ctx->opts.get_opt ().get_reserve_vtx_vec ());

    start = std::chrono::system_clock::now ();
    if (ctx->opts.get_opt ().is_load_file_set ()) {
        if (populate_resource_db_file (ctx) < 0)
            goto done;
        flux_log (ctx->h,
                  LOG_INFO,
                  "%s: loaded resources from %s",
                  __FUNCTION__,
                  ctx->opts.get_opt ().get_load_file ().c_str ());
    } else {
        if (populate_resource_db_acquire (ctx) < 0) {
            flux_log (ctx->h, LOG_ERR, "%s: populate_resource_db_acquire", __FUNCTION__);
            goto done;
        }
        flux_log (ctx->h,
                  LOG_INFO,
                  ctx->m_acquire_resources_from_core ? "%s: loaded resources from core's "
                                                       "resource.acquire"
                                                     : "%s: loaded resources from "
                                                       "sched-fluxion-resource.notify",
                  __FUNCTION__);
    }

    elapsed = std::chrono::system_clock::now () - start;
    perf.load = elapsed.count ();
    perf.graph_uptime = std::chrono::system_clock::now ();
    rc = 0;

done:
    return rc;
}

/* Add resources associated with 'rank' execution target,
 * defined by hwloc_xml.  This function may be called with
 * rank == IDSET_INVALID_ID, to instantiate an empty graph.
 */
static int grow (std::shared_ptr<resource_ctx_t> &ctx,
                 vtx_t v,
                 unsigned int rank,
                 const char *hwloc_xml)
{
    int rc = -1;
    resource_graph_db_t &db = *(ctx->db);

    if (rank == IDSET_INVALID_ID) {
        // Grow cluster vertex and leave
        if ((rc = db.load ("", ctx->reader, rank)) < 0) {
            flux_log (ctx->h,
                      LOG_ERR,
                      "%s: reader: %s",
                      __FUNCTION__,
                      ctx->reader->err_message ().c_str ());
        }
        goto ret;
    }

    if (v == boost::graph_traits<resource_graph_t>::null_vertex ()) {
        if ((rc = db.load (hwloc_xml, ctx->reader, rank)) < 0) {
            flux_log (ctx->h,
                      LOG_ERR,
                      "%s: reader: %s",
                      __FUNCTION__,
                      ctx->reader->err_message ().c_str ());
            goto ret;
        }
    } else {
        if ((rc = db.load (hwloc_xml, ctx->reader, v, rank)) < 0) {
            flux_log (ctx->h,
                      LOG_ERR,
                      "%s: reader: %s",
                      __FUNCTION__,
                      ctx->reader->err_message ().c_str ());
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

    if (!(entry = json_array_get (array, index)) || !(s = json_string_value (entry))) {
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
        if (!(ids = idset_decode (resources)))
            goto inval;
        if ((id = idset_first (ids)) == IDSET_INVALID_ID)
            goto inval;
        id_vec.push_back (id);
        while ((id = idset_next (ids, id)) != IDSET_INVALID_ID)
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
static int unpack_resources (json_t *resobj,
                             struct idset **idset,
                             json_t **r_lite_p,
                             json_t **jgf_p,
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
    double max_clock = static_cast<double> (
        std::chrono::duration_cast<std::chrono::seconds> (
            std::chrono::time_point<std::chrono::system_clock>::max ().time_since_epoch ())
            .count ());

    if (!(ids = idset_create (0, IDSET_FLAG_AUTOGROW)))
        return -1;
    if (resobj) {
        if (json_unpack (resobj,
                         "{s:i s:{s:o s?F s?F} s?:o}",
                         "version",
                         &version,
                         "execution",
                         "R_lite",
                         &r_lite,
                         "starttime",
                         &start,
                         "expiration",
                         &end,
                         "scheduling",
                         &jgf)
            < 0)
            goto inval;
        // RFC 20 states that end == 0.0 means "unset". Set to max_clock if so:
        if (end == 0.)
            end = max_clock;
        // flux-core validates these numbers, but checking here
        // in case Fluxion is plugged into another resource manager
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
            || end > static_cast<double> (std::numeric_limits<int64_t>::max ()))
            goto inval;
        // Ensure start and end are representable in system clock
        if (start > static_cast<double> (
                std::chrono::duration_cast<std::chrono::seconds> (
                    std::chrono::time_point<std::chrono::system_clock>::max ().time_since_epoch ())
                    .count ())
            || end > static_cast<double> (
                   std::chrono::duration_cast<std::chrono::seconds> (
                       std::chrono::time_point<std::chrono::system_clock>::max ()
                           .time_since_epoch ())
                       .count ()))
            goto inval;
        // Expects int type argument
        duration.graph_start =
            std::chrono::system_clock::from_time_t (static_cast<int64_t> (start));
        duration.graph_end = std::chrono::system_clock::from_time_t (static_cast<int64_t> (end));
        // Ensure there is no overflow in system clock representation
        // (should be handled by previous check).
        if (std::chrono::duration_cast<std::chrono::seconds> (
                duration.graph_start.time_since_epoch ())
                    .count ()
                < 0
            || std::chrono::duration_cast<std::chrono::seconds> (
                   duration.graph_end.time_since_epoch ())
                       .count ()
                   < 0) {
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
                          std::map<const distinct_range_t, std::shared_ptr<resobj_t>> &out)
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
            if (json_unpack (val,
                             "{s:s s:{s?:s s?:s}}",
                             "rank",
                             &rank,
                             "children",
                             "core",
                             &core,
                             "gpu",
                             &gpu)
                < 0)
                goto inval;
            // Split the rank idset in resobj, convert each entry into
            // distinct_range_t and use it as the key to std::map.
            // The value is the pointer to resobj_t.
            // The distinct_range_t class provides ordering logic such that
            // you will be able to iterate through ranges in strictly
            // ascending order.
            istr.str (rank);
            while (std::getline (istr, range, ',')) {
                uint64_t low, high;
                std::pair<std::map<const distinct_range_t, std::shared_ptr<resobj_t>>::iterator,
                          bool>
                    res;
                std::shared_ptr<resobj_t> robj = std::make_shared<resobj_t> ();
                if (distinct_range_t::get_low_high (range, low, high) < 0)
                    goto error;
                robj->exec_target_range = range;
                if (core && expand_ids (core, robj->core) < 0)
                    goto error;
                if (gpu && expand_ids (gpu, robj->gpu) < 0)
                    goto error;
                res = out.insert (
                    std::pair<const distinct_range_t,
                              std::shared_ptr<resobj_t>> (distinct_range_t{low, high}, robj));
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

static int remap_hwloc_namespace (std::shared_ptr<resource_ctx_t> &ctx, json_t *r_lite)
{
    std::map<const distinct_range_t, std::shared_ptr<resobj_t>> resobjs;
    if (unpack_resobj (r_lite, resobjs) < 0)
        return -1;
    for (auto &kv : resobjs) {
        /* hwloc reader only needs to remap gpu IDs */
        size_t logical;
        for (logical = 0; logical < kv.second->gpu.size (); logical++) {
            if (ctx->reader->namespace_remapper.add (kv.second->exec_target_range,
                                                     "gpu",
                                                     logical,
                                                     kv.second->gpu[logical])
                < 0)
                return -1;
        }
    }
    return 0;
}

static int remap_jgf_namespace (std::shared_ptr<resource_ctx_t> &ctx,
                                json_t *resobj,
                                json_t *p_resobj)
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
                                       kv.first.get_high () - kv.first.get_low () + cur_rank};
            std::shared_ptr<resobj_t> entry = nullptr;
            std::shared_ptr<resobj_t> p_entry = kv.second;
            if (robjs.find (new_range) == robjs.end ()) {
                // Can't find the rank range in the current namespace
                // corresponding to the (remapped) range of the parent namespace!
                errno = ENOENT;
                goto error;
            }
            entry = robjs[new_range];
            if (ctx->reader->namespace_remapper.add_exec_target_range (p_entry->exec_target_range,
                                                                       new_range)
                < 0)
                goto error;
            for (j = 0; j < p_entry->core.size (); j++) {
                if (ctx->reader->namespace_remapper.add (p_entry->exec_target_range,
                                                         "core",
                                                         p_entry->core[j],
                                                         entry->core[j])
                    < 0)
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
                                   struct idset *ids,
                                   json_t *resobj)
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
    if (db.metadata.roots.find (containment_sub) == db.metadata.roots.end ()) {
        if (rank != IDSET_INVALID_ID) {
            if (!(hwloc_xml = get_array_string (xml_array, rank)))
                goto done;
        } else
            hwloc_xml = NULL;
        // before hwloc reader is used, set remap
        if ((rc = remap_hwloc_namespace (ctx, resobj)) < 0)
            goto done;
        if ((rc = grow (ctx, v, rank, hwloc_xml)) < 0)
            goto done;
    }

    // If the above grow() does not grow resources in the "containment"
    // subsystem, this condition can still be false
    if (db.metadata.roots.find (containment_sub) == db.metadata.roots.end ()) {
        rc = -1;
        errno = EINVAL;
        flux_log (ctx->h, LOG_ERR, "%s: cluster vertex is unavailable", __FUNCTION__);
        goto done;
    }
    v = db.metadata.roots.at (containment_sub);

    rank = idset_next (ids, rank);
    while (rank != IDSET_INVALID_ID) {
        // For the rest of the ranks -- general case
        if (!(hwloc_xml = get_array_string (xml_array, rank)))
            goto done;
        if ((rc = grow (ctx, v, rank, hwloc_xml)) < 0)
            goto done;
        rank = idset_next (ids, rank);
    }

    flux_log (ctx->h, LOG_DEBUG, "resource graph datastore loaded with hwloc reader");

done:
    flux_future_destroy (f);
    return rc;
}

static int grow_resource_db_rv1exec (std::shared_ptr<resource_ctx_t> &ctx,
                                     struct idset *ids,
                                     json_t *resobj)
{
    int rc = -1;
    int saved_errno;
    resource_graph_db_t &db = *(ctx->db);
    char *rv1_str = nullptr;

    if (db.metadata.roots.find (containment_sub) == db.metadata.roots.end ()) {
        if ((rv1_str = json_dumps (resobj, JSON_INDENT (0))) == NULL) {
            errno = ENOMEM;
            goto done;
        }
        if ((rc = db.load (rv1_str, ctx->reader, -1)) < 0) {
            flux_log_error (ctx->h,
                            "%s: db.load: %s",
                            __FUNCTION__,
                            ctx->reader->err_message ().c_str ());
            goto done;
        }
        flux_log (ctx->h, LOG_DEBUG, "resource graph datastore loaded with rv1exec reader");
    }
done:
    saved_errno = errno;
    free (rv1_str);
    errno = saved_errno;
    return rc;
}

static int get_parent_job_resources (std::shared_ptr<resource_ctx_t> &ctx, json_t **resobj_p)
{
    int rc = -1;
    json_t *resobj;
    json_error_t json_err;
    flux_jobid_t id;
    flux_future_t *f = NULL;
    flux_t *parent_h = NULL;
    const char *uri, *jobid, *resobj_str;

    if (!(uri = flux_attr_get (ctx->h, "parent-uri")))
        return 0;
    if (!(jobid = flux_attr_get (ctx->h, "jobid")))
        return 0;
    if (flux_job_id_parse (jobid, &id) < 0) {
        flux_log_error (ctx->h, "%s: parsing jobid %s", __FUNCTION__, jobid);
        return -1;
    }
    if (!(parent_h = flux_open (uri, 0))) {
        flux_log_error (ctx->h, "%s: flux_open (%s)", __FUNCTION__, uri);
        goto done;
    }
    if (!(f = flux_rpc_pack (parent_h,
                             "job-info.lookup",
                             FLUX_NODEID_ANY,
                             0,
                             "{s:I s:[s] s:i}",
                             "id",
                             id,
                             "keys",
                             "R",
                             "flags",
                             0))) {
        flux_log_error (ctx->h, "%s: flux_rpc_pack (R)", __FUNCTION__);
        goto done;
    }
    if (flux_rpc_get_unpack (f, "{s:s}", "R", &resobj_str) < 0) {
        flux_log_error (ctx->h, "%s: flux_rpc_get_unpack (R)", __FUNCTION__);
        goto done;
    }
    if (!(resobj = json_loads (resobj_str, 0, &json_err))) {
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

static int unpack_parent_job_resources (std::shared_ptr<resource_ctx_t> &ctx, json_t **p_r_lite_p)
{
    int rc = 0;
    int saved_errno;
    // Unused: necessary to avoid function overload and duplicated code
    graph_duration_t duration;
    json_t *p_jgf = NULL;
    json_t *p_r_lite = NULL;
    json_t *p_resources = NULL;
    struct idset *p_grow_set = NULL;
    if ((rc = get_parent_job_resources (ctx, &p_resources)) < 0 || !p_resources)
        goto done;
    if ((rc = unpack_resources (p_resources, &p_grow_set, &p_r_lite, &p_jgf, duration)) < 0)
        goto done;
    if (!p_grow_set || !p_r_lite || !p_jgf) {
        errno = EINVAL;
        rc = -1;
        goto done;
    }
    if ((*p_r_lite_p = json_deep_copy (p_r_lite)) == NULL) {
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

static int grow_resource_db_jgf (std::shared_ptr<resource_ctx_t> &ctx, json_t *r_lite, json_t *jgf)
{
    int rc = -1;
    int saved_errno;
    json_t *p_r_lite = NULL;
    resource_graph_db_t &db = *(ctx->db);
    char *jgf_str = NULL;
    vtx_t v = boost::graph_traits<resource_graph_t>::null_vertex ();

    if ((rc = unpack_parent_job_resources (ctx, &p_r_lite)) < 0) {
        flux_log_error (ctx->h, "%s: unpack_parent_job_resources", __FUNCTION__);
        goto done;
    }
    if (db.metadata.roots.find (containment_sub) == db.metadata.roots.end ()) {
        if (p_r_lite && (rc = remap_jgf_namespace (ctx, r_lite, p_r_lite)) < 0) {
            flux_log_error (ctx->h, "%s: remap_jgf_namespace", __FUNCTION__);
            goto done;
        }
        if ((jgf_str = json_dumps (jgf, JSON_INDENT (0))) == NULL) {
            rc = -1;
            errno = ENOMEM;
            goto done;
        }
        if ((rc = db.load (jgf_str, ctx->reader, -1)) < 0) {
            flux_log_error (ctx->h,
                            "%s: db.load: %s",
                            __FUNCTION__,
                            ctx->reader->err_message ().c_str ());
            goto done;
        }
    }

    flux_log (ctx->h, LOG_DEBUG, "resource graph datastore loaded with JGF reader");

done:
    saved_errno = errno;
    json_decref (p_r_lite);
    free (jgf_str);
    errno = saved_errno;
    return rc;
}

static int grow_resource_db (std::shared_ptr<resource_ctx_t> &ctx, json_t *resources)
{
    int rc = 0;
    graph_duration_t duration;
    struct idset *grow_set = NULL;
    json_t *r_lite = NULL;
    json_t *jgf = NULL;
    auto guard = resource_type_t::storage_t::open_for_scope ();

    if ((rc = unpack_resources (resources, &grow_set, &r_lite, &jgf, duration)) < 0) {
        flux_log_error (ctx->h, "%s: unpack_resources", __FUNCTION__);
        goto done;
    }
    if (jgf) {
        if (ctx->reader == nullptr && (rc = create_reader (ctx, "jgf")) < 0) {
            flux_log (ctx->h, LOG_ERR, "%s: can't create jgf reader", __FUNCTION__);
            goto done;
        }
        rc = grow_resource_db_jgf (ctx, r_lite, jgf);
    } else {
        if (ctx->opts.get_opt ().get_load_format () == "hwloc") {
            if (!ctx->reader && (rc = create_reader (ctx, "hwloc")) < 0) {
                flux_log (ctx->h, LOG_ERR, "%s: can't create hwloc reader", __FUNCTION__);
                goto done;
            }
            rc = grow_resource_db_hwloc (ctx, grow_set, r_lite);
        } else if (ctx->opts.get_opt ().get_load_format () == "rv1exec") {
            if (!ctx->reader && (rc = create_reader (ctx, "rv1exec")) < 0) {
                flux_log (ctx->h, LOG_ERR, "%s: can't create rv1exec reader", __FUNCTION__);
                goto done;
            }
            rc = grow_resource_db_rv1exec (ctx, grow_set, resources);
        } else {
            errno = EINVAL;
            rc = -1;
        }
    }
    ctx->db->metadata.set_graph_duration (duration);
    ctx->m_resources_updated = true;

done:
    idset_destroy (grow_set);
    return rc;
}

static int decode_all (std::shared_ptr<resource_ctx_t> &ctx, std::set<int64_t> &ranks)
{
    ranks.clear ();
    for (auto const &kv : ctx->db->metadata.by_rank) {
        if (kv.first >= 0)
            ranks.insert (kv.first);
    }
    return 0;
}

static int decode_rankset (std::shared_ptr<resource_ctx_t> &ctx,
                           const char *ids,
                           std::set<int64_t> &ranks)
{
    int rc = -1;
    unsigned int rank;
    struct idset *idset = NULL;

    if (!ids) {
        errno = EINVAL;
        goto done;
    }
    if (std::string ("all") == ids) {
        if ((rc = decode_all (ctx, ranks)) < 0)
            goto done;
    } else {
        if (!(idset = idset_decode (ids)))
            goto done;
        for (rank = idset_first (idset); rank != IDSET_INVALID_ID;
             rank = idset_next (idset, rank)) {
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
                      const char *ids,
                      resource_pool_t::status_t status)
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
                     const char *ids,
                     resource_pool_t::status_t status)
{
    int rc = -1;
    std::set<int64_t> ranks;
    if (!ids) {
        errno = EINVAL;
        goto done;
    }
    if ((rc = decode_rankset (ctx, ids, ranks)) < 0)
        goto done;
    if ((rc = ctx->traverser->mark (ranks, status)) < 0) {
        flux_log_error (ctx->h,
                        "%s: traverser::mark: %s",
                        __FUNCTION__,
                        ctx->traverser->err_message ().c_str ());
        goto done;
    }
    flux_log (ctx->h,
              LOG_DEBUG,
              "resource status changed (rankset=[%s] status=%s)",
              ids,
              resource_pool_t::status_to_str (status).c_str ());

    // Updated the ranks
    ctx->m_resources_down_updated = true;

done:
    return rc;
}

int mark (std::shared_ptr<resource_ctx_t> &ctx, const char *ids, resource_pool_t::status_t status)
{
    return (ctx->traverser->is_initialized ()) ? mark_now (ctx, ids, status)
                                               : mark_lazy (ctx, ids, status);
}

int shrink_resources (std::shared_ptr<resource_ctx_t> &ctx, const char *ids)
{
    int rc = -1;
    std::set<int64_t> ranks;

    if (!ids) {
        errno = EINVAL;
        goto done;
    }
    if ((rc = decode_rankset (ctx, ids, ranks)) != 0) {
        flux_log (ctx->h, LOG_ERR, "decode_rankset (\"%s\") failed", ids);
        goto done;
    }
    if ((rc = ctx->traverser->remove (ranks)) != 0) {
        flux_log (ctx->h,
                  LOG_ERR,
                  "partial cancel by ranks (\"%s\") failed: %s",
                  ids,
                  ctx->traverser->err_message ().c_str ());
        goto done;
    }
    if ((rc = ctx->traverser->remove_subgraph (ranks)) != 0) {
        flux_log (ctx->h,
                  LOG_ERR,
                  "shrink %s failed: %s",
                  ids,
                  ctx->traverser->err_message ().c_str ());
        goto done;
    }
    // Update total counts:
    rc = ctx->traverser->initialize ();
    flux_log (ctx->h, LOG_DEBUG, "successfully removed ranks %s from resource set", ids);

done:
    return rc;
}

// Subtract the idset string in b from the idset string in a.
// Return result if non-empty in resultp (*resultp=NULL if no ids in result)
// Returns 0 on success, -1 on failure.
static int subtract_ids (const char *a, const char *b, char **resultp)
{
    int rc = -1;
    char *result = NULL;
    struct idset *idset = idset_decode (a);

    *resultp = NULL;
    if (!idset || idset_decode_subtract (idset, b, -1, NULL) < 0)
        goto out;

    // Success if idset is empty (result will be NULL) or non-empty
    // idset successfully encoded:
    if (idset_count (idset) == 0 || (*resultp = idset_encode (idset, IDSET_FLAG_RANGE)))
        rc = 0;
out:
    idset_destroy (idset);
    return rc;
}

int update_resource_db (std::shared_ptr<resource_ctx_t> &ctx,
                        json_t *resources,
                        const char *up,
                        const char *down,
                        const char *lost)
{
    int rc = 0;
    char *down_not_lost = NULL;

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

    // RFC 28 specifies that ranks in shrink (lost) will also appear
    // in down, and that lost takes precedence. So subtract lost from
    // down before marking resources.
    if (lost && down) {
        if (subtract_ids (down, lost, &down_not_lost) < 0) {
            flux_log_error (ctx->h, "%s: failed to subtract shrink ranks from down", __FUNCTION__);
            goto done;
        }
        down = down_not_lost;
    }
    if (down
        && (rc = mark (ctx, down_not_lost ? down_not_lost : down, resource_pool_t::status_t::DOWN))
               < 0) {
        flux_log_error (ctx->h, "%s: mark (down)", __FUNCTION__);
        goto done;
    }
    if (lost && ((rc = shrink_resources (ctx, lost)) < 0)) {
        flux_log_error (ctx->h, "%s: shrink (lost)", __FUNCTION__);
        goto done;
    }
done:
    free (down_not_lost);
    return rc;
}

int select_subsystems (std::shared_ptr<resource_ctx_t> &ctx)
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
            subsystem = subsystem_t{token};
            if (!ctx->db->known_subsystem (subsystem)) {
                rc = -1;
                errno = EINVAL;
                goto done;
            }
            ctx->matcher->add_subsystem (subsystem, "*");
        } else {
            subsystem = subsystem_t{token.substr (0, found)};
            if (!ctx->db->known_subsystem (subsystem)) {
                rc = -1;
                errno = EINVAL;
                goto done;
            }
            std::stringstream relations (token.substr (found + 1, std::string::npos));
            std::string relation;
            while (getline (relations, relation, ':'))
                ctx->matcher->add_subsystem (subsystem, relation);
        }
    }

done:
    return rc;
}

////////////////////////////////////////////////////////////////////////////////
// Request Handler Routines
////////////////////////////////////////////////////////////////////////////////

static void update_match_perf (double elapsed, int64_t jobid, bool match_success)
{
    if (match_success)
        perf.succeeded.update_stats (elapsed, jobid, perf.tmp_iter_count);
    else
        perf.failed.update_stats (elapsed, jobid, perf.tmp_iter_count);
}

static int track_schedule_info (std::shared_ptr<resource_ctx_t> &ctx,
                                int64_t id,
                                bool reserved,
                                bool allocated,
                                int64_t at,
                                const std::string &jspec,
                                const std::stringstream &R,
                                double elapse)
{
    if (id < 0 || at < 0) {
        errno = EINVAL;
        return -1;
    }
    try {
        job_lifecycle_t state;
        if (allocated)
            state = (!reserved) ? job_lifecycle_t::ALLOCATED : job_lifecycle_t::RESERVED;
        else
            state = job_lifecycle_t::MATCHED;
        ctx->jobs[id] = std::make_shared<job_info_t> (id, state, at, "", jspec, R.str (), elapse);
        if (allocated)
            if (reserved)
                ctx->reservations[id] = id;
            else
                ctx->allocations[id] = id;
    } catch (std::bad_alloc &e) {
        errno = ENOMEM;
        return -1;
    }
    return 0;
}

static int parse_R (std::shared_ptr<resource_ctx_t> &ctx,
                    const char *R,
                    std::string &R_graph_fmt,
                    int64_t &starttime,
                    uint64_t &duration,
                    std::string &format)
{
    int rc = 0;
    int version = 0;
    int saved_errno;
    double tstart = 0;
    double expiration = 0;
    double max = static_cast<double> (std::numeric_limits<int64_t>::max ());
    json_t *o = NULL;
    json_t *graph = NULL;
    json_error_t error;
    char *jgf_str = NULL;

    if ((o = json_loads (R, 0, &error)) == NULL) {
        rc = -1;
        flux_log (ctx->h, LOG_ERR, "%s: %s", __FUNCTION__, error.text);
        errno = EINVAL;
        goto out;
    }
    if ((rc = json_unpack_ex (o,
                              &error,
                              0,
                              "{s:i s:{s?F s?F} s?:o}",
                              "version",
                              &version,
                              "execution",
                              "starttime",
                              &tstart,
                              "expiration",
                              &expiration,
                              "scheduling",
                              &graph))
        < 0) {
        errno = EINVAL;
        flux_log (ctx->h, LOG_ERR, "%s: json_unpack: %s", __FUNCTION__, error.text);
        goto freemem_out;
    }
    // RFC 20 states: "An expiration of 0. SHALL be interpreted as "unset",
    // therefore if expiration is 0 set it to int64_t::max:
    if (expiration == 0.)
        expiration = max;
    if (version != 1 || tstart < 0 || expiration < tstart || expiration > max) {
        rc = -1;
        errno = EPROTO;
        flux_log (ctx->h,
                  LOG_ERR,
                  "%s: version=%d, starttime=%jd, expiration=%jd",
                  __FUNCTION__,
                  version,
                  static_cast<intmax_t> (tstart),
                  static_cast<intmax_t> (expiration));
        goto freemem_out;
    }
    if (graph != NULL) {
        if (!(jgf_str = json_dumps (graph, JSON_INDENT (0)))) {
            rc = -1;
            errno = ENOMEM;
            flux_log (ctx->h, LOG_ERR, "%s: json_dumps", __FUNCTION__);
            goto freemem_out;
        }
        R_graph_fmt = jgf_str;
        free (jgf_str);
        format = "jgf";
    } else {
        // Use the rv1exec reader
        R_graph_fmt = R;
        format = "rv1exec";
    }

    starttime = static_cast<int64_t> (tstart);
    duration = static_cast<uint64_t> (expiration - tstart);

freemem_out:
    saved_errno = errno;
    json_decref (o);
    errno = saved_errno;
out:
    return rc;
}

int Rlite_equal (const std::shared_ptr<resource_ctx_t> &ctx, const char *R1, const char *R2)
{
    int rc = -1;
    int saved_errno;
    json_t *o1 = NULL;
    json_t *o2 = NULL;
    json_t *rlite1 = NULL;
    json_t *rlite2 = NULL;
    json_error_t error1;
    json_error_t error2;

    if ((o1 = json_loads (R1, 0, &error1)) == NULL) {
        errno = EINVAL;
        flux_log (ctx->h, LOG_ERR, "%s: %s", __FUNCTION__, error1.text);
        goto out;
    }
    if ((rc = json_unpack (o1, "{s:{s:o}}", "execution", "R_lite", &rlite1)) < 0) {
        errno = EINVAL;
        goto out;
    }
    if ((o2 = json_loads (R2, 0, &error2)) == NULL) {
        errno = EINVAL;
        flux_log (ctx->h, LOG_ERR, "%s: %s", __FUNCTION__, error2.text);
        goto out;
    }
    if ((rc = json_unpack (o2, "{s:{s:o}}", "execution", "R_lite", &rlite2)) < 0) {
        errno = EINVAL;
        goto out;
    }
    rc = (json_equal (rlite1, rlite2) == 1) ? 0 : 1;

out:
    saved_errno = errno;
    json_decref (o1);
    json_decref (o2);
    errno = saved_errno;
    return rc;
}

static int run (std::shared_ptr<resource_ctx_t> &ctx,
                int64_t jobid,
                match_op_t op,
                const std::string &jstr,
                int64_t latest,
                int64_t *at,
                flux_error_t *errp)
{
    int rc = -1;
    try {
        Flux::Jobspec::Jobspec j{jstr};
        rc = ctx->traverser->run (j, ctx->writers, op, jobid, at, latest);
    } catch (const Flux::Jobspec::parse_error &e) {
        errno = EINVAL;
        if (errp && e.what ()) {
            int n = snprintf (errp->text, sizeof (errp->text), "%s", e.what ());
            if (n > (int)sizeof (errp->text))
                errp->text[sizeof (errp->text) - 2] = '+';
        }
    }

    return rc;
}

static int run (std::shared_ptr<resource_ctx_t> &ctx,
                int64_t jobid,
                const std::string &R,
                int64_t at,
                uint64_t duration,
                std::string &format)
{
    int rc = 0;
    dfu_traverser_t &tr = *(ctx->traverser);
    std::shared_ptr<resource_reader_base_t> rd;
    if (format == "jgf") {
        if ((rd = create_resource_reader ("jgf")) == nullptr) {
            rc = -1;
            flux_log (ctx->h,
                      LOG_ERR,
                      "%s: create JGF reader (id=%jd)",
                      __FUNCTION__,
                      static_cast<intmax_t> (jobid));
            goto out;
        }
    } else if (format == "rv1exec") {
        if ((rd = create_resource_reader ("rv1exec")) == nullptr) {
            rc = -1;
            flux_log (ctx->h,
                      LOG_ERR,
                      "%s: create rv1exec reader (id=%jd)",
                      __FUNCTION__,
                      static_cast<intmax_t> (jobid));
            goto out;
        }
    } else {
        rc = -1;
        flux_log (ctx->h,
                  LOG_ERR,
                  "%s: create rv1exec reader (id=%jd)",
                  __FUNCTION__,
                  static_cast<intmax_t> (jobid));
        goto out;
    }
    if ((rc = tr.run (R, ctx->writers, rd, jobid, at, duration)) < 0) {
        flux_log (ctx->h,
                  LOG_ERR,
                  "%s: dfu_traverser_t::run (id=%jd): %s",
                  __FUNCTION__,
                  static_cast<intmax_t> (jobid),
                  ctx->traverser->err_message ().c_str ());
        goto out;
    }

out:
    return rc;
}

int run_match (std::shared_ptr<resource_ctx_t> &ctx,
               int64_t jobid,
               const char *cmd,
               const std::string &jstr,
               int64_t within,
               int64_t *now,
               int64_t *at,
               double *overhead,
               std::stringstream &o,
               flux_error_t *errp)
{
    int rc = 0;
    int64_t latest;
    std::chrono::time_point<std::chrono::system_clock> start;
    std::chrono::duration<double> elapsed;
    std::chrono::duration<int64_t> epoch;
    bool rsv = false;

    start = std::chrono::system_clock::now ();
    match_op_t op = string_to_match_op (cmd);
    if (!match_op_valid (op)) {
        rc = -1;
        errno = EINVAL;
        flux_log (ctx->h, LOG_ERR, "%s: unknown cmd: %s", __FUNCTION__, cmd);
        goto done;
    }

    epoch = std::chrono::duration_cast<std::chrono::seconds> (start.time_since_epoch ());
    *at = *now = epoch.count ();

    if (within < 0 || within > std::numeric_limits<int64_t>::max () - *now)
        latest = std::numeric_limits<int64_t>::max ();
    else
        latest = *now + within;

    if ((rc = run (ctx, jobid, op, jstr, latest, at, errp)) < 0) {
        elapsed = std::chrono::system_clock::now () - start;
        *overhead = elapsed.count ();
        update_match_perf (*overhead, jobid, false);
        goto done;
    }
    if ((rc = ctx->writers->emit (o)) < 0) {
        flux_log_error (ctx->h, "%s: writer can't emit", __FUNCTION__);
        goto done;
    }

    rsv = (*now != *at) ? true : false;
    elapsed = std::chrono::system_clock::now () - start;
    *overhead = elapsed.count ();
    update_match_perf (*overhead, jobid, true);

    if (op != match_op_t::MATCH_SATISFIABILITY) {
        if ((rc = track_schedule_info (ctx,
                                       jobid,
                                       rsv,
                                       op != match_op_t::MATCH_WITHOUT_ALLOCATING,
                                       *at,
                                       jstr,
                                       o,
                                       *overhead))
            != 0) {
            flux_log_error (ctx->h,
                            "%s: can't add job info (id=%jd)",
                            __FUNCTION__,
                            (intmax_t)jobid);
            goto done;
        }
    }

done:
    return rc;
}

int run_update (std::shared_ptr<resource_ctx_t> &ctx,
                int64_t jobid,
                const char *R,
                int64_t &at,
                double &overhead,
                std::stringstream &o)
{
    int rc = 0;
    uint64_t duration = 0;
    std::chrono::time_point<std::chrono::system_clock> start;
    std::chrono::duration<double> elapsed;
    std::string R_graph_fmt;
    std::string format;

    start = std::chrono::system_clock::now ();
    if ((rc = parse_R (ctx, R, R_graph_fmt, at, duration, format)) < 0) {
        flux_log_error (ctx->h, "%s: parsing R", __FUNCTION__);
        goto done;
    }
    if ((rc = run (ctx, jobid, R_graph_fmt, at, duration, format)) < 0) {
        elapsed = std::chrono::system_clock::now () - start;
        overhead = elapsed.count ();
        update_match_perf (overhead, jobid, false);
        flux_log_error (ctx->h, "%s: run", __FUNCTION__);
        goto done;
    }
    if ((rc = ctx->writers->emit (o)) < 0) {
        flux_log_error (ctx->h, "%s: writers->emit", __FUNCTION__);
        goto done;
    }
    elapsed = std::chrono::system_clock::now () - start;
    overhead = elapsed.count ();
    update_match_perf (overhead, jobid, true);
    if ((rc = track_schedule_info (ctx, jobid, false, true, at, "", o, overhead)) != 0) {
        flux_log_error (ctx->h, "%s: can't add job info (id=%jd)", __FUNCTION__, (intmax_t)jobid);
        goto done;
    }

done:
    return rc;
}

int run_remove (std::shared_ptr<resource_ctx_t> &ctx,
                int64_t jobid,
                const char *R,
                bool part_cancel,
                bool &full_removal)
{
    int rc = -1;
    dfu_traverser_t &tr = *(ctx->traverser);

    if (part_cancel) {
        // RV1exec only reader supported in production currently
        std::shared_ptr<resource_reader_base_t> reader;
        if ((reader = create_resource_reader ("rv1exec")) == nullptr) {
            rc = -1;
            flux_log (ctx->h,
                      LOG_ERR,
                      "%s: creating rv1exec reader (id=%jd)",
                      __FUNCTION__,
                      static_cast<intmax_t> (jobid));
            goto out;
        }
        rc = tr.remove (R, reader, jobid, full_removal);
    } else {
        rc = tr.remove (jobid);
        full_removal = true;
    }
    if (rc != 0) {
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
        flux_log (ctx->h,
                  LOG_ERR,
                  "%s: dfu_traverser_t::remove (id=%jd): %s",
                  __FUNCTION__,
                  static_cast<intmax_t> (jobid),
                  ctx->traverser->err_message ().c_str ());
        goto out;
    }
    if (full_removal && is_existent_jobid (ctx, jobid))
        ctx->jobs.erase (jobid);

    rc = 0;
out:
    return rc;
}

int run_find (std::shared_ptr<resource_ctx_t> &ctx,
              const std::string &criteria,
              const std::string &format_str,
              json_t **R)
{
    int rc = -1;
    json_t *o = nullptr;
    std::shared_ptr<match_writers_t> w = nullptr;

    match_format_t format = match_writers_factory_t::get_writers_type (format_str);
    if (!(w = match_writers_factory_t::create (format)))
        goto error;
    if ((rc = ctx->traverser->find (w, criteria)) < 0) {
        if (ctx->traverser->err_message () != "") {
            flux_log_error (ctx->h,
                            "%s: %s",
                            __FUNCTION__,
                            ctx->traverser->err_message ().c_str ());
        }
        goto error;
    }
    if ((rc = w->emit_json (&o)) < 0) {
        flux_log_error (ctx->h, "%s: emit", __FUNCTION__);
        goto error;
    }
    if (o)
        *R = o;

error:
    return rc;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

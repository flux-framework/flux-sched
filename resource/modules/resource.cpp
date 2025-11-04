/*****************************************************************************\
 * Copyright 2025 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#include "resource_match.hpp"

MOD_NAME ("sched-fluxion-resource");

////////////////////////////////////////////////////////////////////////////////
// Request Handler Prototypes
////////////////////////////////////////////////////////////////////////////////

static void match_request_cb (flux_t *h, flux_msg_handler_t *w, const flux_msg_t *msg, void *arg);

static void match_multi_request_cb (flux_t *h,
                                    flux_msg_handler_t *w,
                                    const flux_msg_t *msg,
                                    void *arg);

static void update_request_cb (flux_t *h, flux_msg_handler_t *w, const flux_msg_t *msg, void *arg);

static void cancel_request_cb (flux_t *h, flux_msg_handler_t *w, const flux_msg_t *msg, void *arg);

static void partial_cancel_request_cb (flux_t *h,
                                       flux_msg_handler_t *w,
                                       const flux_msg_t *msg,
                                       void *arg);

static void info_request_cb (flux_t *h, flux_msg_handler_t *w, const flux_msg_t *msg, void *arg);

static void stat_request_cb (flux_t *h, flux_msg_handler_t *w, const flux_msg_t *msg, void *arg);

static void stat_clear_cb (flux_t *h, flux_msg_handler_t *w, const flux_msg_t *msg, void *arg);

static void next_jobid_request_cb (flux_t *h,
                                   flux_msg_handler_t *w,
                                   const flux_msg_t *msg,
                                   void *arg);

static void set_property_request_cb (flux_t *h,
                                     flux_msg_handler_t *w,
                                     const flux_msg_t *msg,
                                     void *arg);

static void remove_property_request_cb (flux_t *h,
                                        flux_msg_handler_t *w,
                                        const flux_msg_t *msg,
                                        void *arg);

static void get_property_request_cb (flux_t *h,
                                     flux_msg_handler_t *w,
                                     const flux_msg_t *msg,
                                     void *arg);

static void notify_request_cb (flux_t *h, flux_msg_handler_t *w, const flux_msg_t *msg, void *arg);

static void disconnect_request_cb (flux_t *h,
                                   flux_msg_handler_t *w,
                                   const flux_msg_t *msg,
                                   void *arg);

static void find_request_cb (flux_t *h, flux_msg_handler_t *w, const flux_msg_t *msg, void *arg);

static void status_request_cb (flux_t *h, flux_msg_handler_t *w, const flux_msg_t *msg, void *arg);

static void ns_info_request_cb (flux_t *h, flux_msg_handler_t *w, const flux_msg_t *msg, void *arg);

static void params_request_cb (flux_t *h, flux_msg_handler_t *w, const flux_msg_t *msg, void *arg);

static void set_status_request_cb (flux_t *h,
                                   flux_msg_handler_t *w,
                                   const flux_msg_t *msg,
                                   void *arg);

static const struct flux_msg_handler_spec htab[] =
    {{FLUX_MSGTYPE_REQUEST, "sched-fluxion-resource.match", match_request_cb, 0},
     {FLUX_MSGTYPE_REQUEST, "sched-fluxion-resource.match_multi", match_multi_request_cb, 0},
     {FLUX_MSGTYPE_REQUEST, "sched-fluxion-resource.update", update_request_cb, 0},
     {FLUX_MSGTYPE_REQUEST, "sched-fluxion-resource.cancel", cancel_request_cb, 0},
     {FLUX_MSGTYPE_REQUEST, "sched-fluxion-resource.partial-cancel", partial_cancel_request_cb, 0},
     {FLUX_MSGTYPE_REQUEST, "sched-fluxion-resource.info", info_request_cb, 0},
     {FLUX_MSGTYPE_REQUEST, "sched-fluxion-resource.stats-get", stat_request_cb, FLUX_ROLE_USER},
     {FLUX_MSGTYPE_REQUEST, "sched-fluxion-resource.stats-clear", stat_clear_cb, FLUX_ROLE_USER},
     {FLUX_MSGTYPE_REQUEST, "sched-fluxion-resource.next_jobid", next_jobid_request_cb, 0},
     {FLUX_MSGTYPE_REQUEST, "sched-fluxion-resource.set_property", set_property_request_cb, 0},
     {FLUX_MSGTYPE_REQUEST,
      "sched-fluxion-resource.remove_property",
      remove_property_request_cb,
      0},
     {FLUX_MSGTYPE_REQUEST, "sched-fluxion-resource.get_property", get_property_request_cb, 0},
     {FLUX_MSGTYPE_REQUEST, "sched-fluxion-resource.notify", notify_request_cb, 0},
     {FLUX_MSGTYPE_REQUEST, "sched-fluxion-resource.disconnect", disconnect_request_cb, 0},
     {FLUX_MSGTYPE_REQUEST, "sched-fluxion-resource.find", find_request_cb, 0},
     {FLUX_MSGTYPE_REQUEST, "sched-fluxion-resource.status", status_request_cb, 0},
     {FLUX_MSGTYPE_REQUEST, "sched-fluxion-resource.ns-info", ns_info_request_cb, 0},
     {FLUX_MSGTYPE_REQUEST, "sched-fluxion-resource.params", params_request_cb, 0},
     {FLUX_MSGTYPE_REQUEST, "sched-fluxion-resource.set_status", set_status_request_cb, 0},
     FLUX_MSGHANDLER_TABLE_END};

////////////////////////////////////////////////////////////////////////////////
// Module Initialization Routines
////////////////////////////////////////////////////////////////////////////////

static void set_default_args (std::shared_ptr<resource_ctx_t> &ctx)
{
    resource_opts_t ct_opts;
    std::string e = "";
    ct_opts.set_load_format ("rv1exec");
    ct_opts.set_match_subsystems ("containment");
    ct_opts.set_match_policy ("first", e);
    ct_opts.set_prune_filters ("ALL:core,ALL:node");
    ct_opts.set_match_format ("rv1_nosched");
    ct_opts.set_update_interval (0);
    ct_opts.set_traverser_policy ("simple");
    ctx->opts += ct_opts;
}

static std::shared_ptr<resource_ctx_t> getctx (flux_t *h)
{
    void *d = NULL;
    std::shared_ptr<resource_ctx_t> ctx = nullptr;
    if ((d = flux_aux_get (h, "sched-fluxion-resource")) != NULL)
        ctx = *(static_cast<std::shared_ptr<resource_ctx_t> *> (d));
    if (!ctx) {
        try {
            ctx = std::make_shared<resource_ctx_t> ();
            ctx->db = std::make_shared<resource_graph_db_t> ();
        } catch (std::bad_alloc &e) {
            errno = ENOMEM;
            goto done;
        }
        ctx->h = h;
        ctx->handlers = NULL;
        set_default_args (ctx);
        ctx->traverser = nullptr; /* Cannot be allocated at this point */
        ctx->matcher = nullptr;   /* Cannot be allocated at this point */
        ctx->writers = nullptr;   /* Cannot be allocated at this point */
        ctx->reader = nullptr;    /* Cannot be allocated at this point */
        ctx->m_resources_updated = true;
        ctx->m_resources_down_updated = true;
        ctx->m_resources_alloc_updated = std::chrono::system_clock::now ();
        ctx->m_acquire_resources_from_core = true;
    }

done:
    return ctx;
}

static int process_args (std::shared_ptr<resource_ctx_t> &ctx, int argc, char **argv)
{
    int rc = 0;
    optmgr_kv_t<resource_opts_t> opts_store;
    std::string info_str = "";

    for (int i = 0; i < argc; i++) {
        const std::string kv (argv[i]);
        if ((rc = opts_store.put (kv)) < 0) {
            flux_log_error (ctx->h, "%s: optmgr_kv_t::put (%s)", __FUNCTION__, argv[i]);
            return rc;
        }
    }
    if ((rc = opts_store.parse (info_str)) < 0) {
        flux_log_error (ctx->h, "%s: optmgr_kv_t::parse: %s", __FUNCTION__, info_str.c_str ());
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

    if ((rc = flux_conf_unpack (flux_get_conf (ctx->h),
                                nullptr,
                                "{ s?:o }",
                                "sched-fluxion-resource",
                                &conf))
        < 0) {
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
        if (!(tmp = json_dumps (v, JSON_ENCODE_ANY | JSON_COMPACT))) {
            errno = ENOMEM;
            return -1;
        }
        value = tmp;
        free (tmp);
        tmp = nullptr;
        if (json_typeof (v) == JSON_STRING)
            value = value.substr (1, value.length () - 2);
        if ((rc = opts_store.put (k, value)) < 0) {
            flux_log_error (ctx->h,
                            "%s: optmgr_kv_t::put (%s, %s)",
                            __FUNCTION__,
                            k,
                            value.c_str ());
            return rc;
        }
    }
    if ((rc = opts_store.parse (info_str)) < 0) {
        flux_log_error (ctx->h, "%s: optmgr_kv_t::parse: %s", __FUNCTION__, info_str.c_str ());
        return rc;
    }
    if (info_str != "") {
        flux_log (ctx->h, LOG_DEBUG, "%s: %s", __FUNCTION__, info_str.c_str ());
    }
    ctx->opts += opts_store.get_opt ();
    return rc;
}

static std::shared_ptr<resource_ctx_t> init_module (flux_t *h, int argc, char **argv)
{
    std::shared_ptr<resource_ctx_t> ctx = nullptr;
    uint32_t rank = 1;

    if (!(ctx = getctx (h))) {
        flux_log (h, LOG_ERR, "%s: can't allocate the context", __FUNCTION__);
        return nullptr;
    }
    if (flux_get_rank (h, &rank) < 0) {
        flux_log (h, LOG_ERR, "%s: can't determine rank", __FUNCTION__);
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

    if (rank) {
        flux_log (h, LOG_ERR, "%s: resource module must only run on rank 0", __FUNCTION__);
        goto error;
    }
    if (flux_msg_handler_addvec (h, htab, (void *)h, &ctx->handlers) < 0) {
        flux_log_error (h, "%s: error registering resource event handler", __FUNCTION__);
        goto error;
    }
    return ctx;

error:
    return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// Request Handler Routines
////////////////////////////////////////////////////////////////////////////////

static void update_request_cb (flux_t *h, flux_msg_handler_t *w, const flux_msg_t *msg, void *arg)
{
    char *R = NULL;
    int64_t at = 0;
    double overhead = 0.0f;
    int64_t jobid = 0;
    uint64_t duration = 0;
    std::string status = "";
    std::stringstream o;
    std::chrono::time_point<std::chrono::system_clock> start;
    std::chrono::duration<double> elapsed;

    std::shared_ptr<resource_ctx_t> ctx = getctx ((flux_t *)arg);
    if (flux_request_unpack (msg, NULL, "{s:I s:s}", "jobid", &jobid, "R", &R) < 0) {
        flux_log_error (ctx->h, "%s: flux_request_unpack", __FUNCTION__);
        goto error;
    }
    if (is_existent_jobid (ctx, jobid)) {
        int rc = 0;
        start = std::chrono::system_clock::now ();
        if ((rc = Rlite_equal (ctx, R, ctx->jobs[jobid]->R.c_str ())) < 0) {
            flux_log_error (ctx->h, "%s: Rlite_equal", __FUNCTION__);
            goto error;
        } else if (rc == 1) {
            errno = EINVAL;
            flux_log (ctx->h,
                      LOG_ERR,
                      "%s: jobid (%jd) with different R exists!",
                      __FUNCTION__,
                      static_cast<intmax_t> (jobid));
            goto error;
        }
        elapsed = std::chrono::system_clock::now () - start;
        // If a jobid with matching R exists, no need to update
        overhead = elapsed.count ();
        get_jobstate_str (ctx->jobs[jobid]->state, status);
        o << ctx->jobs[jobid]->R;
        at = ctx->jobs[jobid]->scheduled_at;
        flux_log (ctx->h,
                  LOG_DEBUG,
                  "%s: jobid (%jd) with matching R exists",
                  __FUNCTION__,
                  static_cast<intmax_t> (jobid));
    } else if (run_update (ctx, jobid, R, at, overhead, o) < 0) {
        flux_log_error (ctx->h,
                        "%s: update failed (id=%jd)",
                        __FUNCTION__,
                        static_cast<intmax_t> (jobid));
        goto error;
    }

    if (status == "")
        status = get_status_string (at, at);

    if (flux_respond_pack (h,
                           msg,
                           "{s:I s:s s:f s:s s:I}",
                           "jobid",
                           jobid,
                           "status",
                           status.c_str (),
                           "overhead",
                           overhead,
                           "R",
                           o.str ().c_str (),
                           "at",
                           at)
        < 0)
        flux_log_error (h, "%s", __FUNCTION__);

    return;

error:
    if (flux_respond_error (h, msg, errno, NULL) < 0)
        flux_log_error (h, "%s: flux_respond_error", __FUNCTION__);
}

static void match_request_cb (flux_t *h, flux_msg_handler_t *w, const flux_msg_t *msg, void *arg)
{
    int64_t at = 0;
    int64_t now = 0;
    int64_t jobid = -1;
    double overhead = 0.0f;
    std::string status = "";
    const char *cmd = NULL;
    const char *js_str = NULL;
    std::stringstream R;

    std::shared_ptr<resource_ctx_t> ctx = getctx ((flux_t *)arg);
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
        flux_log_error (h, "%s: existent job (%jd).", __FUNCTION__, (intmax_t)jobid);
        goto error;
    }
    if (run_match (ctx, jobid, cmd, js_str, &now, &at, &overhead, R, NULL) < 0) {
        if (errno != EBUSY && errno != ENODEV)
            flux_log_error (ctx->h,
                            "%s: match failed due to match error (id=%jd)",
                            __FUNCTION__,
                            (intmax_t)jobid);
        // The resources couldn't be allocated *or reserved*
        // Kicking back to qmanager, remove from tracking
        if (errno == EBUSY) {
            ctx->jobs.erase (jobid);
        }
        goto error;
    }

    if (std::string ("without_allocating") == cmd) {
        status = "MATCHED";
    } else {
        // "ALLOCATED" or "RESERVED"
        status = get_status_string (now, at);
    }
    if (flux_respond_pack (h,
                           msg,
                           "{s:I s:s s:f s:s s:I}",
                           "jobid",
                           jobid,
                           "status",
                           status.c_str (),
                           "overhead",
                           overhead,
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

static void match_multi_request_cb (flux_t *h,
                                    flux_msg_handler_t *w,
                                    const flux_msg_t *msg,
                                    void *arg)
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
    if (flux_request_unpack (msg, NULL, "{s:s s:s}", "cmd", &cmd, "jobs", &jobs_str) < 0)
        goto error;
    if (!(jobs = json_loads (jobs_str, 0, &err))) {
        errno = ENOMEM;
        goto error;
    }

    json_array_foreach (jobs, index, value) {
        const char *js_str;
        int64_t at = 0;
        int64_t now = 0;
        double overhead = 0.0f;
        std::string status = "";
        std::stringstream R;

        if (json_unpack (value, "{s:I s:s}", "jobid", &jobid, "jobspec", &js_str) < 0)
            goto error;
        if (is_existent_jobid (ctx, jobid)) {
            errno = EINVAL;
            flux_log_error (h,
                            "%s: existent job (%jd).",
                            __FUNCTION__,
                            static_cast<intmax_t> (jobid));
            goto error;
        }
        if (run_match (ctx, jobid, cmd, js_str, &now, &at, &overhead, R, NULL) < 0) {
            if (errno != EBUSY && errno != ENODEV)
                flux_log_error (ctx->h,
                                "%s: match failed due to match error (id=%jd)",
                                __FUNCTION__,
                                static_cast<intmax_t> (jobid));
            // The resources couldn't be allocated *or reserved*
            // Kicking back to qmanager, remove from tracking
            if (errno == EBUSY) {
                ctx->jobs.erase (jobid);
            }
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
                               overhead,
                               "R",
                               R.str ().c_str (),
                               "at",
                               at)
            < 0) {
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
    if (flux_respond_error (h, msg, errno, !errmsg.empty () ? errmsg.c_str () : nullptr) < 0)
        flux_log_error (h, "%s: flux_respond_error", __FUNCTION__);
}

static void cancel_request_cb (flux_t *h, flux_msg_handler_t *w, const flux_msg_t *msg, void *arg)
{
    std::shared_ptr<resource_ctx_t> ctx = getctx ((flux_t *)arg);
    int64_t jobid = -1;
    char *R = NULL;
    bool full_removal = true;

    if (flux_request_unpack (msg, NULL, "{s:I}", "jobid", &jobid) < 0)
        goto error;
    if (ctx->allocations.find (jobid) != ctx->allocations.end ())
        ctx->allocations.erase (jobid);
    else if (ctx->reservations.find (jobid) != ctx->reservations.end ())
        ctx->reservations.erase (jobid);
    else {
        errno = ENOENT;
        flux_log (h, LOG_DEBUG, "%s: nonexistent job (id=%jd)", __FUNCTION__, (intmax_t)jobid);
        goto error;
    }

    if (run_remove (ctx, jobid, R, false, full_removal) < 0) {
        flux_log_error (h,
                        "%s: remove fails due to match error (id=%jd)",
                        __FUNCTION__,
                        (intmax_t)jobid);
        goto error;
    }
    if (flux_respond_pack (h, msg, "{}") < 0)
        flux_log_error (h, "%s", __FUNCTION__);

    return;

error:
    if (flux_respond_error (h, msg, errno, NULL) < 0)
        flux_log_error (h, "%s: flux_respond_error", __FUNCTION__);
}

static void partial_cancel_request_cb (flux_t *h,
                                       flux_msg_handler_t *w,
                                       const flux_msg_t *msg,
                                       void *arg)
{
    std::shared_ptr<resource_ctx_t> ctx = getctx ((flux_t *)arg);
    int64_t jobid = -1;
    char *R = NULL;
    decltype (ctx->allocations)::iterator jobid_it;
    bool full_removal = false;
    int int_full_removal = 0;

    if (flux_request_unpack (msg, NULL, "{s:I s:s}", "jobid", &jobid, "R", &R) < 0)
        goto error;

    jobid_it = ctx->allocations.find (jobid);
    if (jobid_it == ctx->allocations.end ()) {
        errno = ENOENT;
        flux_log (h,
                  LOG_DEBUG,
                  "%s: job (id=%jd) not found in allocations",
                  __FUNCTION__,
                  (intmax_t)jobid);
        goto error;
    }

    if (run_remove (ctx, jobid, R, true, full_removal) < 0) {
        flux_log_error (h,
                        "%s: remove fails due to match error (id=%jd)",
                        __FUNCTION__,
                        (intmax_t)jobid);
        goto error;
    }
    int_full_removal = full_removal;
    if (flux_respond_pack (h, msg, "{s:i}", "full-removal", int_full_removal) < 0)
        flux_log_error (h, "%s", __FUNCTION__);

    if (full_removal)
        ctx->allocations.erase (jobid_it);

    return;

error:
    if (flux_respond_error (h, msg, errno, NULL) < 0)
        flux_log_error (h, "%s: flux_respond_error", __FUNCTION__);
}

static void info_request_cb (flux_t *h, flux_msg_handler_t *w, const flux_msg_t *msg, void *arg)
{
    std::shared_ptr<resource_ctx_t> ctx = getctx ((flux_t *)arg);
    int64_t jobid = -1;
    std::shared_ptr<job_info_t> info = NULL;
    std::string status = "";

    if (flux_request_unpack (msg, NULL, "{s:I}", "jobid", &jobid) < 0)
        goto error;
    if (!is_existent_jobid (ctx, jobid)) {
        errno = ENOENT;
        flux_log (h, LOG_DEBUG, "%s: nonexistent job (id=%jd)", __FUNCTION__, (intmax_t)jobid);
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

static int get_stat_by_rank (std::shared_ptr<resource_ctx_t> &ctx, json_t *o)
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
            if (!(ids = idset_create (0, IDSET_FLAG_AUTOGROW)))
                goto done;
            s2r[kv.second.size ()] = ids;
        }
        if ((rc = idset_set (s2r[kv.second.size ()], static_cast<unsigned int> (kv.first))) < 0)
            goto done;
    }

    for (auto &kv : s2r) {
        if (!(str = idset_encode (kv.second, IDSET_FLAG_BRACKETS | IDSET_FLAG_RANGE))) {
            rc = -1;
            goto done;
        }
        if ((rc = json_object_set_new (o, str, json_integer (static_cast<json_int_t> (kv.first))))
            < 0) {
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

static void stat_request_cb (flux_t *h, flux_msg_handler_t *w, const flux_msg_t *msg, void *arg)
{
    std::shared_ptr<resource_ctx_t> ctx = getctx ((flux_t *)arg);
    int saved_errno;
    json::value o;
    json_t *match_succeeded = nullptr;
    json_t *match_failed = nullptr;
    double avg = 0.0f;
    double min = 0.0f;
    double variance = 0.0f;
    // Failed match stats
    double avg_failed = 0.0f;
    double min_failed = 0.0f;
    double variance_failed = 0.0f;
    int64_t graph_uptime_s = 0;
    int64_t time_since_reset_s = 0;
    std::chrono::time_point<std::chrono::system_clock> now;

    if (perf.succeeded.njobs_reset > 1) {
        avg = perf.succeeded.avg;
        min = perf.succeeded.min;
        // Welford's online algorithm
        variance = perf.succeeded.M2 / (double)perf.succeeded.njobs_reset;
    }
    if (perf.failed.njobs_reset > 1) {
        avg_failed = perf.failed.avg;
        min_failed = perf.failed.min;
        // Welford's online algorithm
        variance_failed = perf.failed.M2 / (double)perf.failed.njobs_reset;
    }
    if (!(o = json::value::take (json_object ()))) {
        errno = ENOMEM;
        goto error;
    }
    if (get_stat_by_rank (ctx, o.get ()) < 0) {
        flux_log_error (h, "%s: get_stat_by_rank", __FUNCTION__);
        goto error;
    }

    if (!(match_succeeded = json_pack ("{s:I s:I s:I s:I s:{s:f s:f s:f s:f}}",
                                       "njobs",
                                       perf.succeeded.njobs,
                                       "njobs-reset",
                                       perf.succeeded.njobs_reset,
                                       "max-match-jobid",
                                       perf.succeeded.max_match_jobid,
                                       "max-match-iters",
                                       perf.succeeded.match_iter_count,
                                       "stats",
                                       "min",
                                       min,
                                       "max",
                                       perf.succeeded.max,
                                       "avg",
                                       avg,
                                       "variance",
                                       variance))) {
        errno = ENOMEM;
        goto error;
    }
    if (!(match_failed = json_pack ("{s:I s:I s:I s:I s:{s:f s:f s:f s:f}}",
                                    "njobs",
                                    perf.failed.njobs,
                                    "njobs-reset",
                                    perf.failed.njobs_reset,
                                    "max-match-jobid",
                                    perf.failed.max_match_jobid,
                                    "max-match-iters",
                                    perf.failed.match_iter_count,
                                    "stats",
                                    "min",
                                    min_failed,
                                    "max",
                                    perf.failed.max,
                                    "avg",
                                    avg_failed,
                                    "variance",
                                    variance_failed))) {
        errno = ENOMEM;
        goto error;
    }
    now = std::chrono::system_clock::now ();
    graph_uptime_s =
        std::chrono::duration_cast<std::chrono::seconds> (now - perf.graph_uptime).count ();
    time_since_reset_s =
        std::chrono::duration_cast<std::chrono::seconds> (now - perf.time_of_last_reset).count ();

    if (flux_respond_pack (h,
                           msg,
                           "{s:I s:I s:O s:f s:I s:I s:{s:O s:O}}",
                           "V",
                           num_vertices (ctx->db->resource_graph),
                           "E",
                           num_edges (ctx->db->resource_graph),
                           "by_rank",
                           o.get (),
                           "load-time",
                           perf.load,
                           "graph-uptime",
                           graph_uptime_s,
                           "time-since-reset",
                           time_since_reset_s,
                           "match",
                           "succeeded",
                           match_succeeded,
                           "failed",
                           match_failed)
        < 0) {
        flux_log_error (h, "%s: flux_respond_pack", __FUNCTION__);
    }
    json_decref (match_succeeded);
    json_decref (match_failed);

    return;

error:
    saved_errno = errno;
    json_decref (match_succeeded);
    json_decref (match_failed);
    errno = saved_errno;
    if (flux_respond_error (h, msg, errno, NULL) < 0)
        flux_log_error (h, "%s: flux_respond_error", __FUNCTION__);
}

static void stat_clear_cb (flux_t *h, flux_msg_handler_t *w, const flux_msg_t *msg, void *arg)
{
    std::shared_ptr<resource_ctx_t> ctx = getctx ((flux_t *)arg);

    perf.time_of_last_reset = std::chrono::system_clock::now ();
    // Clear the jobs-related stats and reset time
    perf.succeeded.njobs_reset = 0;
    perf.succeeded.max_match_jobid = -1;
    perf.succeeded.match_iter_count = -1;
    perf.succeeded.min = std::numeric_limits<double>::max ();
    perf.succeeded.max = 0.0;
    perf.succeeded.avg = 0.0;
    perf.succeeded.M2 = 0.0;
    // Failed match stats
    perf.failed.njobs_reset = 0;
    perf.failed.max_match_jobid = -1;
    perf.failed.match_iter_count = -1;
    perf.failed.min = std::numeric_limits<double>::max ();
    perf.failed.max = 0.0;
    perf.failed.avg = 0.0;
    perf.failed.M2 = 0.0;

    if (flux_respond (h, msg, NULL) < 0)
        flux_log_error (h, "%s: flux_respond", __FUNCTION__);
}

static inline int64_t next_jobid (const std::map<uint64_t, std::shared_ptr<job_info_t>> &m)
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

static void set_property_request_cb (flux_t *h,
                                     flux_msg_handler_t *w,
                                     const flux_msg_t *msg,
                                     void *arg)
{
    const char *rp = NULL, *kv = NULL;
    std::string resource_path = "", keyval = "", errmsg = "";
    std::string property_key = "", property_value = "";
    size_t pos;
    std::shared_ptr<resource_ctx_t> ctx = getctx ((flux_t *)arg);
    std::map<std::string, std::vector<vtx_t>>::const_iterator it;
    std::pair<std::map<std::string, std::string>::iterator, bool> ret;
    vtx_t v;

    if (flux_request_unpack (msg, NULL, "{s:s s:s}", "sp_resource_path", &rp, "sp_keyval", &kv)
        < 0) {
        errmsg = "could not unpack payload";
        goto error;
    }

    resource_path = rp;
    keyval = kv;

    pos = keyval.find ('=');

    if (pos == 0 || (pos == keyval.size () - 1) || pos == std::string::npos) {
        errno = EINVAL;
        errmsg = "Incorrect format, use set-property <resource> PROPERTY=VALUE";
        goto error;
    }

    property_key = keyval.substr (0, pos);
    property_value = keyval.substr (pos + 1);

    it = ctx->db->metadata.by_path.find (resource_path);

    if (it == ctx->db->metadata.by_path.end ()) {
        errno = ENOENT;
        errmsg = "Couldn't find '" + resource_path + "' in resource graph";
        goto error;
    }

    for (auto &v : it->second) {
        ret = ctx->db->resource_graph[v].properties.insert (
            std::pair<std::string, std::string> (property_key, property_value));

        if (ret.second == false) {
            ctx->db->resource_graph[v].properties.erase (property_key);
            ctx->db->resource_graph[v].properties.insert (
                std::pair<std::string, std::string> (property_key, property_value));
        }
    }

    if (flux_respond_pack (h, msg, "{}") < 0)
        flux_log_error (h, "%s", __FUNCTION__);

    return;

error:
    if (flux_respond_error (h, msg, errno, errmsg.c_str ()) < 0)
        flux_log_error (h, "%s: flux_respond_error", __FUNCTION__);
}

static void remove_property_request_cb (flux_t *h,
                                        flux_msg_handler_t *w,
                                        const flux_msg_t *msg,
                                        void *arg)
{
    const char *rp = NULL, *kv = NULL;
    std::string resource_path = "", property_key = "", errmsg = "";
    std::shared_ptr<resource_ctx_t> ctx = getctx ((flux_t *)arg);
    std::map<std::string, std::vector<vtx_t>>::const_iterator it;
    std::pair<std::map<std::string, std::string>::iterator, bool> ret;
    vtx_t v;

    if (flux_request_unpack (msg, NULL, "{s:s s:s}", "resource_path", &rp, "key", &kv) < 0) {
        errmsg = "could not unpack payload";
        goto error;
    }

    resource_path = rp;
    property_key = kv;

    it = ctx->db->metadata.by_path.find (resource_path);

    if (it == ctx->db->metadata.by_path.end ()) {
        errno = ENOENT;
        errmsg = "Couldn't find '" + resource_path + "' in resource graph";
        goto error;
    }

    for (auto &v : it->second) {
        ctx->db->resource_graph[v].properties.erase (property_key);
    }

    if (flux_respond_pack (h, msg, "{}") < 0)
        flux_log_error (h, "%s", __FUNCTION__);

    return;

error:
    if (flux_respond_error (h, msg, errno, errmsg.c_str ()) < 0)
        flux_log_error (h, "%s: flux_respond_error", __FUNCTION__);
}

static void get_property_request_cb (flux_t *h,
                                     flux_msg_handler_t *w,
                                     const flux_msg_t *msg,
                                     void *arg)
{
    const char *rp = NULL, *gp_key = NULL;
    std::string resource_path = "", property_key = "", errmsg = "";
    std::shared_ptr<resource_ctx_t> ctx = getctx ((flux_t *)arg);
    std::map<std::string, std::vector<vtx_t>>::const_iterator it;
    std::map<std::string, std::string>::const_iterator p_it;
    vtx_t v;
    std::vector<std::string> resp_values;
    json_t *resp_array = nullptr;

    if (flux_request_unpack (msg, NULL, "{s:s s:s}", "gp_resource_path", &rp, "gp_key", &gp_key)
        < 0) {
        errmsg = "could not unpack payload";
        goto error;
    }

    resource_path = rp;
    property_key = gp_key;

    it = ctx->db->metadata.by_path.find (resource_path);

    if (it == ctx->db->metadata.by_path.end ()) {
        errno = ENOENT;
        errmsg = "Couldn't find '" + resource_path + "' in resource graph";
        goto error;
    }

    for (auto &v : it->second) {
        for (p_it = ctx->db->resource_graph[v].properties.begin ();
             p_it != ctx->db->resource_graph[v].properties.end ();
             p_it++) {
            if (property_key.compare (p_it->first) == 0)
                resp_values.push_back (p_it->second);
        }
    }
    if (resp_values.empty ()) {
        errno = ENOENT;
        errmsg = "Property '" + property_key + "' was not found for resource " + resource_path;
        goto error;
    }

    if (!(resp_array = json_array ())) {
        errno = ENOMEM;
        goto error;
    }
    for (auto &resp_value : resp_values) {
        json_t *value = nullptr;
        if (!(value = json_string (resp_value.c_str ()))) {
            errno = EINVAL;
            errmsg = "internal error";
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
    if (flux_respond_error (h, msg, errno, errmsg.c_str ()) < 0)
        flux_log_error (h, "%s: flux_respond_error", __FUNCTION__);
}

static void disconnect_request_cb (flux_t *h,
                                   flux_msg_handler_t *w,
                                   const flux_msg_t *msg,
                                   void *arg)
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

static void notify_request_cb (flux_t *h, flux_msg_handler_t *w, const flux_msg_t *msg, void *arg)
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
        if (ctx->opts.get_opt ().is_load_file_set ()) {
            errno = ENODATA;
            // Since m_acquired_resources is null,
            flux_log_error (ctx->h, "%s: cannot notify when load-file set", __FUNCTION__);
            goto error;
        }

        // Traverse all nodes to calculate the # of UP/DOWN
        struct idset *up = idset_create (0, IDSET_FLAG_AUTOGROW);
        struct idset *down = idset_create (0, IDSET_FLAG_AUTOGROW);

        resource_graph_t rg = ctx->db->resource_graph;

        for (vtx_t vtx : ctx->db->metadata.by_type[node_rt]) {
            struct idset *vtx_status_idset = nullptr;
            switch (rg[vtx].status) {
                case resource_pool_t::status_t::UP:
                    vtx_status_idset = up;
                    break;
                case resource_pool_t::status_t::DOWN:
                    vtx_status_idset = down;
                    break;
                default:
                    flux_log_error (h, "%s: invalid vertex status", __FUNCTION__);
                    goto error;
            }
            struct idset *rank = idset_decode (std::to_string (rg[vtx].rank).c_str ());
            idset_add (vtx_status_idset, rank);
            idset_destroy (rank);
        }

        char *up_str = idset_encode (up, IDSET_FLAG_RANGE);
        char *down_str = idset_encode (down, IDSET_FLAG_RANGE);
        char *lost_str = idset_encode (ctx->m_notify_lost, IDSET_FLAG_RANGE);

        if (strcmp (up_str, "") == 0) {
            free (up_str);
            up_str = NULL;
        }
        if (strcmp (down_str, "") == 0) {
            free (down_str);
            down_str = NULL;
        }
        if (strcmp (lost_str, "") == 0) {
            free (lost_str);
            lost_str = NULL;
        }

        // Respond only after sched-fluxion-resource gets
        //  resources from its resource.acquire RPC.
        // This is guaranteed by the order of mod_main:
        //  init_resource_graph runs before flux_reactor_run.
        // Only send resources at first so that the
        //  module can initialize its graph.
        if (flux_respond_pack (ctx->h, msg, "{s:O*}", "resources", ctx->m_notify_resources.get ())
            < 0) {
            flux_log_error (ctx->h, "%s: flux_respond_pack", __FUNCTION__);
            goto error;
        }

        // Once the module's graph is initialized, send the node statuses.
        if (flux_respond_pack (ctx->h,
                               msg,
                               "{s:s* s:s* s:s* s:f}",
                               "up",
                               up_str,
                               "down",
                               down_str,
                               "shrink",
                               lost_str,
                               "expiration",
                               ctx->m_notify_expiration)
            < 0) {
            flux_log_error (ctx->h, "%s: flux_respond_pack", __FUNCTION__);
            goto error;
        }

        free (up_str);
        free (down_str);
        free (lost_str);
        idset_destroy (up);
        idset_destroy (down);

        // Add msg as a subscriber to resource UP/DOWN updates
        m->set_msg (msg);
        auto ret = ctx->notify_msgs.insert (
            std::pair<std::string, std::shared_ptr<msg_wrap_t>> (route, m));
        if (!ret.second) {
            errno = EEXIST;
            flux_log_error (h, "%s: insert", __FUNCTION__);
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

static void find_request_cb (flux_t *h, flux_msg_handler_t *w, const flux_msg_t *msg, void *arg)
{
    json_t *R = nullptr;
    int saved_errno;
    const char *criteria = nullptr;
    const char *format_str = "rv1_nosched";
    std::shared_ptr<resource_ctx_t> ctx = getctx ((flux_t *)arg);

    if (flux_request_unpack (msg,
                             nullptr,
                             "{s:s, s?:s}",
                             "criteria",
                             &criteria,
                             "format",
                             &format_str)
        < 0)
        goto error;

    if (run_find (ctx, criteria, format_str, &R) < 0)
        goto error;
    if (flux_respond_pack (h, msg, "{s:o?}", "R", R) < 0) {
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

static void status_request_cb (flux_t *h, flux_msg_handler_t *w, const flux_msg_t *msg, void *arg)
{
    int saved_errno;
    json_t *R_all = nullptr;
    json_t *R_down = nullptr;
    json_t *R_alloc = nullptr;
    std::chrono::time_point<std::chrono::system_clock> now;
    std::chrono::duration<double> elapsed;
    std::shared_ptr<resource_ctx_t> ctx = getctx ((flux_t *)arg);

    now = std::chrono::system_clock::now ();
    elapsed = now - ctx->m_resources_alloc_updated;
    // Get R alloc whenever m_resources_alloc_updated or
    // the elapsed time is greater than configured limit
    if ((elapsed.count () > static_cast<double> (ctx->opts.get_opt ().get_update_interval ()))
        || ctx->m_resources_updated) {
        if (run_find (ctx, "sched-now=allocated", "rv1_nosched", &R_alloc) < 0)
            goto error;
        ctx->m_r_alloc = json::value::take (json_deep_copy (R_alloc));
        ctx->m_resources_alloc_updated = std::chrono::system_clock::now ();
    } else
        R_alloc = json_deep_copy (ctx->m_r_alloc.get ());

    if (ctx->m_resources_updated) {
        if (run_find (ctx, "status=up or status=down", "rv1_nosched", &R_all) < 0)
            goto error;
        ctx->m_r_all = json::value::take (json_deep_copy (R_all));
        ctx->m_resources_updated = false;
    } else
        R_all = json_deep_copy (ctx->m_r_all.get ());

    if (ctx->m_resources_down_updated) {
        if (run_find (ctx, "status=down", "rv1_nosched", &R_down) < 0)
            goto error;
        ctx->m_r_down = json::value::take (json_deep_copy (R_down));
        ctx->m_resources_down_updated = false;
    } else
        R_down = json_deep_copy (ctx->m_r_down.get ());

    if (flux_respond_pack (h,
                           msg,
                           "{s:o? s:o? s:o?}",
                           "all",
                           R_all,
                           "down",
                           R_down,
                           "allocated",
                           R_alloc)
        < 0) {
        flux_log_error (h, "%s: flux_respond_pack", __FUNCTION__);
        goto error;
    }
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

static void ns_info_request_cb (flux_t *h, flux_msg_handler_t *w, const flux_msg_t *msg, void *arg)
{
    uint64_t rank, id, remapped_id;
    const char *type_name;
    std::shared_ptr<resource_ctx_t> ctx = getctx ((flux_t *)arg);

    if (flux_request_unpack (msg,
                             nullptr,
                             "{s:I s:s s:I}",
                             "rank",
                             &rank,
                             "type-name",
                             &type_name,
                             "id",
                             &id)
        < 0) {
        flux_log_error (h, "%s: flux_respond_unpack", __FUNCTION__);
        goto error;
    }
    if (ctx->reader->namespace_remapper.query (rank, type_name, id, remapped_id) < 0) {
        flux_log_error (h, "%s: namespace_remapper.query", __FUNCTION__);
        goto error;
    }
    if (remapped_id > static_cast<uint64_t> (std::numeric_limits<int64_t>::max ())) {
        errno = EOVERFLOW;
        flux_log_error (h, "%s: remapped id too large", __FUNCTION__);
        goto error;
    }
    if (flux_respond_pack (h, msg, "{s:I}", "id", static_cast<int64_t> (remapped_id)) < 0) {
        flux_log_error (h, "%s: flux_respond_pack", __FUNCTION__);
        goto error;
    }
    return;

error:
    if (flux_respond_error (h, msg, errno, nullptr) < 0)
        flux_log_error (h, "%s: flux_respond_error", __FUNCTION__);
}

static void params_request_cb (flux_t *h, flux_msg_handler_t *w, const flux_msg_t *msg, void *arg)
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

/*
 * Send a NULL resource.notify message to get qmanager to reconsider jobs
 */
static int reconsider_blocked_jobs (
    flux_t *h,
    const std::map<std::string, std::shared_ptr<msg_wrap_t>> &notify_msgs)
{
    int rc = 0;
    for (auto &kv : notify_msgs) {
        if (flux_respond (h, kv.second->get_msg (), NULL) < 0) {
            rc = -1;
            flux_log_error (h, "%s: flux_respond", __FUNCTION__);
        }
    }
    return rc;
}

/*
 * Mark a vertex as up or down
 */
static void set_status_request_cb (flux_t *h,
                                   flux_msg_handler_t *w,
                                   const flux_msg_t *msg,
                                   void *arg)
{
    const char *rp = NULL, *st = NULL;
    std::string resource_path = "", status = "", errmsg = "";
    std::shared_ptr<resource_ctx_t> ctx = getctx ((flux_t *)arg);
    resource_pool_t::string_to_status sts = resource_pool_t::str_to_status;
    std::map<std::string, std::vector<vtx_t>>::const_iterator it{};
    resource_pool_t::string_to_status::iterator status_it{};

    if (flux_request_unpack (msg, NULL, "{s:s, s:s}", "resource_path", &rp, "status", &st) < 0) {
        errmsg = "malformed RPC";
        goto error;
    }
    resource_path = rp;
    status = st;
    // check that the path/vertex exists
    it = ctx->db->metadata.by_path.find (resource_path);
    if (it == ctx->db->metadata.by_path.end ()) {
        errmsg = "could not find path '" + resource_path + "' in resource graph";
        goto error;
    }
    // check that the status given is valid ('up' or 'down')
    status_it = sts.find (status);
    if (status_it == sts.end ()) {
        errmsg = "unrecognized status '" + status + "'";
        goto error;
    }
    // mark the vertex
    if (ctx->traverser->mark (resource_path, status_it->second) < 0) {
        flux_log_error (h,
                        "%s: traverser::mark: %s",
                        __FUNCTION__,
                        ctx->traverser->err_message ().c_str ());
        errmsg = "Failed to set status of resource vertex";
        goto error;
    }
    ctx->m_resources_down_updated = true;
    if (flux_respond (h, msg, NULL) < 0) {
        flux_log_error (h, "%s: flux_respond", __FUNCTION__);
    }
    // if status was UP, need to reconsider blocked jobs
    if (status_it->second == resource_pool_t::status_t::UP) {
        if (reconsider_blocked_jobs (h, ctx->notify_msgs) < 0) {
            flux_log_error (h, "%s: reconsider_blocked_jobs", __FUNCTION__);
        }
    }
    return;

error:
    if (flux_respond_error (h, msg, EINVAL, errmsg.c_str ()) < 0)
        flux_log_error (h, "%s: flux_respond_error", __FUNCTION__);
    return;
}

////////////////////////////////////////////////////////////////////////////////
// Resource Graph and Traverser Initialization
////////////////////////////////////////////////////////////////////////////////

static int init_resource_graph (std::shared_ptr<resource_ctx_t> &ctx)
{
    int rc = 0;

    // Select the appropriate traverser based on CLI policy.
    ctx->traverser =
        std::make_shared<dfu_traverser_t> (ctx->opts.get_opt ().get_traverser_policy ());

    // Select the appropriate matcher based on CLI policy.
    if (!(ctx->matcher = create_match_cb (ctx->opts.get_opt ().get_match_policy ()))) {
        flux_log (ctx->h, LOG_ERR, "%s: can't create match callback", __FUNCTION__);
        return -1;
    }
    if ((rc = populate_resource_db (ctx)) != 0) {
        flux_log (ctx->h, LOG_ERR, "%s: can't populate graph resource database", __FUNCTION__);
        return rc;
    }
    if ((rc = select_subsystems (ctx)) != 0) {
        flux_log (ctx->h,
                  LOG_ERR,
                  "%s: error processing subsystems %s",
                  __FUNCTION__,
                  ctx->opts.get_opt ().get_match_subsystems ().c_str ());
        return rc;
    }

    // Create a writers object for matched vertices and edges
    match_format_t format =
        match_writers_factory_t::get_writers_type (ctx->opts.get_opt ().get_match_format ());
    if (!(ctx->writers = match_writers_factory_t::create (format)))
        return -1;

    if (ctx->opts.get_opt ().is_prune_filters_set ()
        && ctx->matcher->set_pruning_types_w_spec (ctx->matcher->dom_subsystem (),
                                                   ctx->opts.get_opt ().get_prune_filters ())
               < 0) {
        flux_log (ctx->h,
                  LOG_ERR,
                  "%s: error setting pruning types with: %s",
                  __FUNCTION__,
                  ctx->opts.get_opt ().get_prune_filters ().c_str ());
        return -1;
    }

    // Initialize the DFU traverser
    if (ctx->traverser->initialize (ctx->db, ctx->matcher) < 0) {
        flux_log (ctx->h, LOG_ERR, "%s: traverser initialization", __FUNCTION__);
        return -1;
    }

    // Perform the initial status marking only when "up" rankset is available
    // Rankless reader cases (required for testing e.g., GRUG) must not
    // execute the following branch.
    // Use ctx->update_f != nullptr to differentiate
    if (ctx->update_f) {
        if (mark (ctx, "all", resource_pool_t::status_t::DOWN) < 0) {
            flux_log (ctx->h, LOG_ERR, "%s: mark (down)", __FUNCTION__);
            return -1;
        }
        if (ctx->is_ups_set ()) {
            if (mark (ctx, ctx->get_ups ().c_str (), resource_pool_t::status_t::UP) < 0) {
                flux_log (ctx->h, LOG_ERR, "%s: mark (up)", __FUNCTION__);
                return -1;
            }
        }
    }

    // prevent users from consuming unbounded memory with arbitrary resource types
    subsystem_t::storage_t::finalize ();
    resource_type_t::storage_t::finalize ();
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Module Main
////////////////////////////////////////////////////////////////////////////////

extern "C" int mod_main (flux_t *h, int argc, char **argv)
{
    int rc = -1;

    flux_log (h, LOG_INFO, "version %s", PACKAGE_VERSION);

    try {
        std::shared_ptr<resource_ctx_t> ctx = nullptr;
        uint32_t rank = 1;

        if (!(ctx = init_module (h, argc, argv))) {
            flux_log (h, LOG_ERR, "%s: can't initialize resource module", __FUNCTION__);
            goto done;
        }
        // Because mod_main is always active, the following is safe.
        flux_aux_set (h, "sched-fluxion-resource", &ctx, NULL);
        flux_log (h, LOG_DEBUG, "%s: resource module starting", __FUNCTION__);

        /* Before beginning synchronous resource.acquire RPC, set module status
         * to 'running' to let flux module load return success.
         */
        if ((rc = flux_module_set_running (ctx->h)) < 0) {
            flux_log_error (ctx->h, "%s: flux_module_set_running", __FUNCTION__);
            goto done;
        }
        if ((rc = init_resource_graph (ctx)) != 0) {
            flux_log (h, LOG_ERR, "%s: can't initialize resource graph database", __FUNCTION__);
            goto done;
        }
        flux_log (h, LOG_DEBUG, "%s: resource graph database loaded", __FUNCTION__);

        if ((rc = flux_reactor_run (flux_get_reactor (h), 0)) < 0) {
            flux_log (h, LOG_ERR, "%s: flux_reactor_run: %s", __FUNCTION__, strerror (errno));
            goto done;
        }
    } catch (std::exception &e) {
        errno = ENOSYS;
        flux_log (h, LOG_ERR, "%s: %s", __FUNCTION__, e.what ());
        return -1;
    } catch (...) {
        errno = ENOSYS;
        flux_log (h, LOG_ERR, "%s: caught unknown exception", __FUNCTION__);
        return -1;
    }

done:
    return rc;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

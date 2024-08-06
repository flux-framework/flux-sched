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

MOD_NAME ("sched-fluxion-feasibility");

////////////////////////////////////////////////////////////////////////////////
// Request Handler Prototypes
////////////////////////////////////////////////////////////////////////////////

static void feasibility_request_cb (flux_t *h,
                                    flux_msg_handler_t *w,
                                    const flux_msg_t *msg,
                                    void *arg);

static const struct flux_msg_handler_spec htab[] =
    {{FLUX_MSGTYPE_REQUEST, "feasibility.check", feasibility_request_cb, 0},
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
    ct_opts.set_prune_filters ("ALL:core,ALL:node,ALL:gpu,ALL:ssd");
    ct_opts.set_match_format ("rv1_nosched");
    ct_opts.set_update_interval (0);
    ctx->opts += ct_opts;
}

static std::shared_ptr<resource_ctx_t> getctx (flux_t *h)
{
    void *d = NULL;
    std::shared_ptr<resource_ctx_t> ctx = nullptr;
    if ((d = flux_aux_get (h, "sched-fluxion-feasibility")) != NULL)
        ctx = *(static_cast<std::shared_ptr<resource_ctx_t> *> (d));
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
        ctx->matcher = nullptr; /* Cannot be allocated at this point */
        ctx->writers = nullptr; /* Cannot be allocated at this point */
        ctx->reader = nullptr;  /* Cannot be allocated at this point */
        ctx->m_resources_updated = false;
        ctx->m_resources_down_updated = true;
        ctx->m_resources_alloc_updated = std::chrono::system_clock::now ();
        ctx->m_acquire_resources_from_core = false;
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
                                "sched-fluxion-feasibility",
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
    flux_future_t *f = nullptr;
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

    // Register feasibility service
    f = flux_service_register (h, "feasibility");
    if (flux_future_get (f, NULL) < 0) {
        flux_log_error (h, "%s: error registering feasibility service", __FUNCTION__);
        flux_future_destroy (f);
        goto error;
    } else {
        flux_log (h, LOG_DEBUG, "service registered");
        flux_future_destroy (f);
    }
    // Register feasibility handlers
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

static void feasibility_request_cb (flux_t *h,
                                    flux_msg_handler_t *w,
                                    const flux_msg_t *msg,
                                    void *arg)
{
    int64_t at = 0;
    int64_t now = 0;
    double overhead = 0.0f;
    int saved_errno = 0;
    std::stringstream R;
    json_t *jobspec = nullptr;
    const char *js_str = nullptr;
    std::string errmsg;
    flux_error_t error;
    std::shared_ptr<resource_ctx_t> ctx = getctx ((flux_t *)arg);

    if (flux_request_unpack (msg, NULL, "{s:o}", "jobspec", &jobspec) < 0)
        goto error;
    if (!(js_str = json_dumps (jobspec, JSON_INDENT (0)))) {
        errno = ENOMEM;
        goto error;
    }
    error.text[0] = '\0';
    if (run_match (ctx, -1, "satisfiability", js_str, &now, &at, &overhead, R, &error) < 0) {
        if (errno == ENODEV)
            errmsg = "Unsatisfiable request";
        else {
            errmsg = "Internal match error: ";
            errmsg += error.text;
        }
        goto error_memfree;
    }
    free ((void *)js_str);
    if (flux_respond (h, msg, NULL) < 0)
        flux_log_error (h, "%s: flux_respond", __FUNCTION__);
    return;

error_memfree:
    saved_errno = errno;
    free ((void *)js_str);
    errno = saved_errno;
error:
    if (flux_respond_error (h, msg, errno, errmsg.c_str ()) < 0)
        flux_log_error (h, "%s: flux_respond_error", __FUNCTION__);
}

////////////////////////////////////////////////////////////////////////////////
// Resource Graph and Traverser Initialization
////////////////////////////////////////////////////////////////////////////////

static int init_resource_graph (std::shared_ptr<resource_ctx_t> &ctx)
{
    int rc = 0;

    // The feasibility module should only use "first". Exclusivity has no effect.
    if (!(ctx->matcher = create_match_cb ("first"))) {
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
            flux_log (h, LOG_ERR, "%s: can't initialize feasibility module", __FUNCTION__);
            goto done;
        }

        // Because mod_main is always active, the following is safe.
        flux_aux_set (h, "sched-fluxion-feasibility", &ctx, NULL);
        flux_log (h, LOG_DEBUG, "%s: feasibility module starting", __FUNCTION__);

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

        // Unregister feasibility service (registered in init_module)
        flux_future_t *f = nullptr;
        f = flux_service_unregister (h, "feasibility");
        if (flux_future_get (f, NULL) < 0)
            flux_log_error (h, "Failed to unregister feasibility service");
        flux_future_destroy (f);

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

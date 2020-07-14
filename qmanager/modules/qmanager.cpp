/*****************************************************************************\
 *  Copyright (c) 2019 Lawrence Livermore National Security, LLC.  Produced at
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

extern "C" {
#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <flux/core.h>
#include <flux/schedutil.h>
#include <jansson.h>
}

#include <sstream>

#include "qmanager/policies/base/queue_policy_base.hpp"
#include "qmanager/policies/base/queue_policy_base_impl.hpp"
#include "qmanager/policies/queue_policy_factory_impl.hpp"
#include "qmanager/modules/qmanager_opts.hpp"
#include "src/common/c++wrappers/eh_wrapper.hpp"
#include "qmanager/modules/qmanager_callbacks.hpp"

using namespace Flux;
using namespace Flux::queue_manager;
using namespace Flux::queue_manager::detail;
using namespace Flux::opts_manager;
using namespace Flux::cplusplus_wrappers;


/******************************************************************************
 *                                                                            *
 *                 Queue Manager Service Module Context                       *
 *                                                                            *
 ******************************************************************************/

class fluxion_resource_interface_t {
public:
    ~fluxion_resource_interface_t ();
    int fetch_and_reset_notify_rc ();
    int get_notify_rc () const;
    void set_notify_rc (int rc);
    flux_future_t *notify_f{nullptr};
private:
    int m_notify_rc = 0;
};

struct qmanager_ctx_t : public qmanager_cb_ctx_t,
                        public fluxion_resource_interface_t {
    flux_t *h;
};

fluxion_resource_interface_t::~fluxion_resource_interface_t ()
{
    if (notify_f)
        flux_future_destroy (notify_f);
}

int fluxion_resource_interface_t::fetch_and_reset_notify_rc ()
{
    int rc = m_notify_rc;
    m_notify_rc = 0;
    return rc;
}

int fluxion_resource_interface_t::get_notify_rc () const
{
    return m_notify_rc;
}

void fluxion_resource_interface_t::set_notify_rc (int rc)
{
    m_notify_rc = rc;
}

static int process_args (std::shared_ptr<qmanager_ctx_t> &ctx,
                         int argc, char **argv)
{
    int rc = 0;
    optmgr_kv_t<qmanager_opts_t> opts_store;
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

static int process_config_file (std::shared_ptr<qmanager_ctx_t> &ctx)
{
    int rc = 0;
    json_t *conf = NULL;

    if ( (rc = flux_conf_unpack (flux_get_conf (ctx->h), NULL,
                                     "{ s?:o }",
                                         "sched-fluxion-qmanager",
                                         &conf)) < 0) {
        flux_log_error (ctx->h, "%s: flux_conf_unpack", __FUNCTION__);
        return rc;
    }

    const char *k = NULL;
    json_t *v = NULL;
    optmgr_kv_t<qmanager_opts_t> opts_store;
    std::string info_str = "";
    json_object_foreach (conf, k, v) {
        std::string value = json_dumps (v, JSON_ENCODE_ANY|JSON_COMPACT);
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

static void set_default (std::shared_ptr<qmanager_ctx_t> &ctx)
{
    qmanager_opts_t ct_opts;
    ct_opts.set_queue_policy ("fcfs");
    ct_opts.set_queue_params ("");
    ct_opts.set_policy_params ("");
    ctx->opts += ct_opts;
}

static void update_on_resource_response (flux_future_t *f, void *arg)
{
    int rc = -1;
    qmanager_ctx_t *ctx = static_cast<qmanager_ctx_t *> (arg);

    if ( (rc = flux_rpc_get (f, NULL)) < 0) {
        flux_log_error (ctx->h,
            "%s: exiting due to sched-fluxion-resource.notify failure",
            __FUNCTION__);
        flux_reactor_stop (flux_get_reactor (ctx->h));
        goto out;
    }

    for (auto &kv : ctx->queues) {
        if ( (rc = kv.second->run_sched_loop (static_cast<void *> (ctx->h),
                                              true)) < 0
            || (rc = qmanager_safe_cb_t::post_sched_loop (ctx->h,
                                                          ctx->schedutil,
                                                          ctx->queues)) < 0) {
            flux_log_error (ctx->h, "%s: schedule loop", __FUNCTION__);
            goto out;
        }
    }
out:
    flux_future_reset (f);
    ctx->set_notify_rc (rc);
    return;
}

static int handshake_resource (std::shared_ptr<qmanager_ctx_t> &ctx)
{
    int rc = -1;

    if ( !(ctx->notify_f = flux_rpc (ctx->h, "sched-fluxion-resource.notify",
                                     NULL,
                                     FLUX_NODEID_ANY,
                                     FLUX_RPC_STREAMING))) {
        flux_log_error (ctx->h, "%s: flux_rpc (notify)", __FUNCTION__);
        goto out;
    }

    update_on_resource_response (ctx->notify_f, ctx.get ());
    if ( (rc = ctx->fetch_and_reset_notify_rc ()) < 0) {
        flux_log_error (ctx->h, "%s: update_on_resource_response",
                        __FUNCTION__);
        goto out;
    }
    if ( (rc = flux_future_then (ctx->notify_f,
                                 -1.0,
                                 update_on_resource_response,
                                 ctx.get ())) < 0) {
        flux_log_error (ctx->h, "%s: flux_future_then", __FUNCTION__);
        goto out;
    }
out:
    return rc;
}

static int handshake_jobmanager (std::shared_ptr<qmanager_ctx_t> &ctx)
{
    int rc = -1;
    int queue_depth = 0;  /* Not implemented in job-manager */

    if (!(ctx->schedutil = schedutil_create (
                               ctx->h,
                               &qmanager_safe_cb_t::jobmanager_alloc_cb,
                               &qmanager_safe_cb_t::jobmanager_free_cb,
                               &qmanager_safe_cb_t::jobmanager_cancel_cb,
                               std::static_pointer_cast<
                                   qmanager_cb_ctx_t> (ctx).get ()))) {
        flux_log_error (ctx->h, "%s: schedutil_create", __FUNCTION__);
        goto out;
    }
    if (schedutil_hello (ctx->schedutil,
                         &qmanager_safe_cb_t::jobmanager_hello_cb,
                         std::static_pointer_cast<
			     qmanager_cb_ctx_t> (ctx).get ()) < 0) {
        flux_log_error (ctx->h, "%s: schedutil_hello", __FUNCTION__);
        goto out;
    }
    if (schedutil_ready (ctx->schedutil, "unlimited", &queue_depth)) {
        flux_log_error (ctx->h, "%s: schedutil_ready", __FUNCTION__);
        goto out;
    }
    rc = 0;
out:
    return rc;
}

static int enforce_queue_policy (std::shared_ptr<qmanager_ctx_t> &ctx,
                                 const std::string &queue_name,
                                 const queue_prop_t &p)
{
    int rc = -1;
    std::shared_ptr<queue_policy_base_t> queue;
    std::pair<std::map<std::string,
                       std::shared_ptr<queue_policy_base_t>>::iterator,
                                       bool> ret;
    if ( !(queue = create_queue_policy (p.get_queue_policy (), "module"))) {
        errno = EINVAL;
        flux_log_error (ctx->h, "%s: create_queue_policy (%s)",
                        __FUNCTION__,
                        p.get_queue_policy ().c_str ());
        goto out;
    }
    ret = ctx->queues.insert (
              std::pair<std::string, std::shared_ptr<queue_policy_base_t>> (
                  queue_name, queue));
    if (!ret.second) {
        errno = EEXIST;
        goto out;
    }
    rc = 0;
out:
    return rc;
}

static int enforce_params (std::shared_ptr<qmanager_ctx_t> &ctx,
                           const std::string queue_name,
                           const queue_prop_t &prop)
{
    if (!prop.is_queue_params_set () || !prop.is_policy_params_set ()) {
        errno = EINVAL;
        return -1;
    }
    const std::string &queue_params = prop.get_queue_params ();
    const std::string &policy_params = prop.get_policy_params ();
    if (prop.is_queue_params_set () && queue_params != ""
        && ctx->queues.at (queue_name)->set_queue_params (queue_params) < 0) {
        flux_log_error (ctx->h, "%s: queues[%s]->set_queue_params (%s)",
                        __FUNCTION__, queue_name.c_str (), queue_params.c_str ());
        return -1;
    }
    if (prop.is_policy_params_set () && policy_params != ""
        && ctx->queues.at (queue_name)->set_policy_params (policy_params) < 0) {
        flux_log_error (ctx->h, "%s: queues[%s]->set_policy_params (%s)",
                        __FUNCTION__, queue_name.c_str (), policy_params.c_str ());
        return -1;
    }
    if (ctx->queues.at (queue_name)->apply_params () < 0) {
        flux_log_error (ctx->h, "%s: queue[%s]->apply_params",
                        __FUNCTION__, queue_name.c_str ());
        return -1;
    }
    return 0;
}

static int enforce_queues (std::shared_ptr<qmanager_ctx_t> &ctx)
{
    int rc = 0;
    ctx->opts.canonicalize ();
    const std::map<std::string, queue_prop_t> &per_queue_prop
        = ctx->opts.get_opt ().get_per_queue_prop ();

    for (const auto &kv : per_queue_prop) {
        std::string res_qp = "";
        std::string res_pp = "";
        const std::string &queue_name = kv.first;
        const queue_prop_t &queue_prop = kv.second;

        if ( (rc = enforce_queue_policy (ctx, queue_name, queue_prop)) < 0)
            goto out;

        flux_log (ctx->h, LOG_DEBUG,
                  "enforced policy (queue=%s): %s", queue_name.c_str (),
                  queue_prop.get_queue_policy ().c_str ());

        if ( (rc = enforce_params (ctx, queue_name, queue_prop)) < 0)
            goto out;

        ctx->queues.at (queue_name)->get_params (res_qp, res_pp);
        if (res_qp.empty ())
            res_qp = std::string ("default");
        if (res_pp.empty ())
            res_pp = std::string ("default");
        flux_log (ctx->h, LOG_DEBUG,
                  "effective queue params (queue=%s): %s",
                  queue_name.c_str (), res_qp.c_str ());
        flux_log (ctx->h, LOG_DEBUG,
                  "effective policy params (queue=%s): %s",
                  queue_name.c_str (), res_pp.c_str ());
    }

out:
    return rc;
}

static int enforce_options (std::shared_ptr<qmanager_ctx_t> &ctx)
{
    int rc = 0;

    if ( (rc = enforce_queues (ctx)) < 0) {
        flux_log_error (ctx->h, "%s: enforce_queues", __FUNCTION__);
        return rc;
    }
    if ( (rc = handshake_resource (ctx)) < 0) {
        flux_log_error (ctx->h, "%s: handshake_resource", __FUNCTION__);
        return rc;
    }
    flux_log (ctx->h, LOG_DEBUG,
              "handshaking with sched-fluxion-resource completed");

    if ( (rc = handshake_jobmanager (ctx)) < 0) {
        flux_log_error (ctx->h, "%s: handshake_jobmanager", __FUNCTION__);
        return rc;
    }
    flux_log (ctx->h, LOG_DEBUG,
              "handshaking with job-manager completed");

    return rc;
}

static std::shared_ptr<qmanager_ctx_t> qmanager_new (flux_t *h)
{
    std::shared_ptr<qmanager_ctx_t> ctx = nullptr;
    try {
        ctx = std::make_shared<qmanager_ctx_t> ();
        ctx->h = h;
        set_default (ctx);
    } catch (std::bad_alloc &e) {
        errno = ENOMEM;
    }

    return ctx;
}

static void qmanager_destroy (std::shared_ptr<qmanager_ctx_t> &ctx)
{
    if (ctx != nullptr) {
        int saved_errno = errno;
        std::shared_ptr<job_t> job;
        for (auto kv : ctx->queues) {
            while ( (job = ctx->queues.at (kv.first)->pending_pop ())
                     != nullptr)
                flux_respond_error (ctx->h, job->msg, ENOSYS, "unloading");
            while ( (job = ctx->queues.at (kv.first)->complete_pop ())
                     != nullptr)
                flux_respond_error (ctx->h, job->msg, ENOSYS, "unloading");
        }
        schedutil_destroy (ctx->schedutil);
        errno = saved_errno;
    }
}


/******************************************************************************
 *                                                                            *
 *                               Module Main                                  *
 *                                                                            *
 ******************************************************************************/

int mod_start (flux_t *h, int argc, char **argv)
{
    int rc = -1;
    std::shared_ptr<qmanager_ctx_t> ctx = nullptr;
    if ( !(ctx = qmanager_new (h))) {
        flux_log_error (h, "%s: qmanager_new", __FUNCTION__);
        return rc;
    }
    if ( (rc = process_config_file (ctx)) < 0) {
        flux_log_error (h, "%s: config file parsing", __FUNCTION__);
        qmanager_destroy (ctx);
        return rc;
    }
    if ( (rc = process_args (ctx, argc, argv)) < 0) {
        flux_log_error (h, "%s: load line argument parsing", __FUNCTION__);
        qmanager_destroy (ctx);
        return rc;
    }
    if ( (rc = enforce_options (ctx)) < 0) {
        flux_log_error (h, "%s: enforce_queue_policy", __FUNCTION__);
        qmanager_destroy (ctx);
        return rc;
    }
    if ( (rc = flux_reactor_run (flux_get_reactor (h), 0)) < 0)
        flux_log_error (h, "%s: flux_reactor_run", __FUNCTION__);
    qmanager_destroy (ctx);
    return rc;
}

extern "C" int mod_main (flux_t *h, int argc, char **argv)
{
    eh_wrapper_t exception_safe_main;
    int rc = exception_safe_main (mod_start, h, argc, argv);
    if (exception_safe_main.bad ())
        flux_log_error (h, "%s: %s", __FUNCTION__,
                            exception_safe_main.get_err_message ());
    return rc;
}

MOD_NAME ("sched-fluxion-qmanager");

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

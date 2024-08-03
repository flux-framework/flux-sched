/*****************************************************************************\
 * Copyright 2020 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef QMANAGER_CALLBACKS_HPP
#define QMANAGER_CALLBACKS_HPP

extern "C" {
#include <flux/core.h>
#include <flux/schedutil.h>
}

#include "qmanager/modules/qmanager_opts.hpp"
#include "qmanager/policies/base/queue_policy_base.hpp"

struct qmanager_cb_ctx_t {
    flux_t *h{nullptr};

    flux_watcher_t *prep{nullptr};
    flux_watcher_t *check{nullptr};
    flux_watcher_t *idle{nullptr};
    bool pls_sched_loop{false};
    bool pls_post_loop{false};

    schedutil_t *schedutil{nullptr};
    Flux::opts_manager::optmgr_composer_t<Flux::opts_manager::qmanager_opts_t> opts;
    std::map<std::string, std::shared_ptr<Flux::queue_manager::queue_policy_base_t>> queues;

    int find_queue (flux_jobid_t id,
                    std::string &queue_name,
                    std::shared_ptr<Flux::queue_manager::queue_policy_base_t> &queue);
};

class qmanager_cb_t {
   protected:
    static int jobmanager_hello_cb (flux_t *h, const flux_msg_t *msg, const char *R, void *arg);
    static void jobmanager_stats_get_cb (flux_t *h,
                                         flux_msg_handler_t *w,
                                         const flux_msg_t *msg,
                                         void *arg);
    static void jobmanager_stats_clear_cb (flux_t *h,
                                           flux_msg_handler_t *w,
                                           const flux_msg_t *msg,
                                           void *arg);
    static void jobmanager_alloc_cb (flux_t *h, const flux_msg_t *msg, void *arg);
    static void jobmanager_free_cb (flux_t *h, const flux_msg_t *msg, const char *R, void *arg);
    static void jobmanager_cancel_cb (flux_t *h, const flux_msg_t *msg, void *arg);
    static void jobmanager_prioritize_cb (flux_t *h, const flux_msg_t *msg, void *arg);
    static void prep_watcher_cb (flux_reactor_t *r, flux_watcher_t *w, int revents, void *arg);
    static void check_watcher_cb (flux_reactor_t *r, flux_watcher_t *w, int revents, void *arg);
    static int post_sched_loop (
        flux_t *h,
        schedutil_t *schedutil,
        std::map<std::string, std::shared_ptr<Flux::queue_manager::queue_policy_base_t>> &queues);
};

struct qmanager_safe_cb_t : public qmanager_cb_t {
    static int jobmanager_hello_cb (flux_t *h, const flux_msg_t *msg, const char *R, void *arg);
    static void jobmanager_stats_get_cb (flux_t *h,
                                         flux_msg_handler_t *w,
                                         const flux_msg_t *msg,
                                         void *arg);
    static void jobmanager_stats_clear_cb (flux_t *h,
                                           flux_msg_handler_t *w,
                                           const flux_msg_t *msg,
                                           void *arg);
    static void jobmanager_alloc_cb (flux_t *h, const flux_msg_t *msg, void *arg);
    static void jobmanager_free_cb (flux_t *h, const flux_msg_t *msg, const char *R, void *arg);
    static void jobmanager_cancel_cb (flux_t *h, const flux_msg_t *msg, void *arg);
    static void jobmanager_prioritize_cb (flux_t *h, const flux_msg_t *msg, void *arg);
    static void prep_watcher_cb (flux_reactor_t *r, flux_watcher_t *w, int revents, void *arg);
    static void check_watcher_cb (flux_reactor_t *r, flux_watcher_t *w, int revents, void *arg);
    static int post_sched_loop (
        flux_t *h,
        schedutil_t *schedutil,
        std::map<std::string, std::shared_ptr<Flux::queue_manager::queue_policy_base_t>> &queues);
};

#endif  // #define QMANAGER_CALLBACKS_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

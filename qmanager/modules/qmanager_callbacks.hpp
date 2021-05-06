/*****************************************************************************\
 *  Copyright (c) 2020 Lawrence Livermore National Security, LLC.  Produced at
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
    bool pls_sched_loop {false};
    bool pls_post_loop {false};

    schedutil_t *schedutil{nullptr};
    Flux::opts_manager::optmgr_composer_t<
        Flux::opts_manager::qmanager_opts_t> opts;
    std::map<std::string, std::shared_ptr<
        Flux::queue_manager::queue_policy_base_t>> queues;

    int find_queue (
            flux_jobid_t id, std::string &queue_name,
            std::shared_ptr<Flux::queue_manager::queue_policy_base_t> &queue);
};

class qmanager_cb_t {
protected:
    static int jobmanager_hello_cb (flux_t *h, const flux_msg_t *msg,
                                    const char *R, void *arg);
    static void jobmanager_alloc_cb (flux_t *h, const flux_msg_t *msg,
                                     void *arg);
    static void jobmanager_free_cb (flux_t *h, const flux_msg_t *msg,
                                    const char *R, void *arg);
    static void jobmanager_cancel_cb (flux_t *h, const flux_msg_t *msg,
                                      void *arg);
    static void jobmanager_prioritize_cb (flux_t *h, const flux_msg_t *msg,
                                          void *arg);
    static void prep_watcher_cb (flux_reactor_t *r, flux_watcher_t *w,
                                 int revents, void *arg);
    static void check_watcher_cb (flux_reactor_t *r, flux_watcher_t *w,
                                  int revents, void *arg);
    static int post_sched_loop (flux_t *h,
        schedutil_t *schedutil,
        std::map<std::string, std::shared_ptr<
            Flux::queue_manager::queue_policy_base_t>> &queues);
};

struct qmanager_safe_cb_t : public qmanager_cb_t {
    static int jobmanager_hello_cb (flux_t *h, const flux_msg_t *msg,
                                    const char *R, void *arg);
    static void jobmanager_alloc_cb (flux_t *h, const flux_msg_t *msg,
                                     void *arg);
    static void jobmanager_free_cb (flux_t *h, const flux_msg_t *msg,
                                    const char *R, void *arg);
    static void jobmanager_cancel_cb (flux_t *h, const flux_msg_t *msg,
                                      void *arg);
    static void jobmanager_prioritize_cb (flux_t *h, const flux_msg_t *msg,
                                          void *arg);
    static void prep_watcher_cb (flux_reactor_t *r, flux_watcher_t *w,
                                 int revents, void *arg);
    static void check_watcher_cb (flux_reactor_t *r, flux_watcher_t *w,
                                  int revents, void *arg);
    static int post_sched_loop (flux_t *h,
        schedutil_t *schedutil,
        std::map<std::string, std::shared_ptr<
            Flux::queue_manager::queue_policy_base_t>> &queues);
};

#endif // #define QMANAGER_CALLBACKS_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

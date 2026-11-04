/*****************************************************************************\
 * Copyright 2026 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

extern "C" {
#if HAVE_CONFIG_H
#include <config.h>
#endif
}

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>
#include <catch2/catch_test_macros.hpp>

#include "qmanager/modules/qmanager_callbacks.hpp"
#include "qmanager/policies/queue_policy_easy.hpp"
#include "qmanager/policies/queue_policy_easy_impl.hpp"

using namespace Flux::queue_manager;
using namespace Flux::queue_manager::detail;

// Resources released in one queue can let a blocked job of any other
// queue run. The free callback records the release and the prepare
// callback reconsiders the blocked jobs of every queue, one time per
// reactor turn. These tests drive both callbacks directly.

// Stand-in for the resource API. The backfill policy template calls
// these static methods only, so no base class is needed.
struct fake_reapi_t {
    // Return without a response, like the streaming match RPC of the
    // resource module. The scheduling loop therefore stays active until
    // the test reports a match success or failure.
    static int match_allocate_multi (void *,
                                     bool,
                                     json_t *,
                                     Flux::resource_model::queue_adapter_base_t *)
    {
        return 0;
    }
    static int cancel (void *, flux_jobid_t, bool)
    {
        return 0;
    }
    static int cancel (void *, flux_jobid_t, const char *, bool, bool &full_removal)
    {
        // A partial release gives back part of the allocation only, so
        // the job keeps the rest and stays in the running queue.
        full_removal = false;
        return 0;
    }
    static int update_allocate (void *,
                                uint64_t,
                                const std::string &,
                                int64_t &,
                                double &,
                                std::string &)
    {
        return 0;
    }
};

using easy_queue_t = queue_policy_easy_t<fake_reapi_t>;
using queue_ptr_t = std::shared_ptr<queue_policy_base_t>;

struct msg_deleter_t {
    void operator() (flux_msg_t *msg) const
    {
        flux_msg_destroy (msg);
    }
};
using msg_ptr_t = std::unique_ptr<flux_msg_t, msg_deleter_t>;

static void submit (queue_ptr_t &queue, flux_jobid_t id)
{
    auto job = std::make_shared<job_t> (job_state_kind_t::INIT, id, 0, 0, 0.0, "");
    // An empty object is enough: the fake resource API never reads the
    // jobspec, but the policy still parses it as JSON.
    job->jobspec = "{}";
    REQUIRE (queue->insert (job) == 0);
}

static void start_loop (queue_ptr_t &queue)
{
    // Moves the newly inserted jobs into the pending queue and sends the
    // first match request.
    REQUIRE (queue->run_sched_loop (nullptr, true) == 0);
}

static void allocate_job (queue_ptr_t &queue, flux_jobid_t id)
{
    REQUIRE (queue->handle_match_success (id, "ALLOCATED", "{}", 0, 0.0) == 0);
}

static void block_job (queue_ptr_t &queue)
{
    // EBUSY while the policy tries to reserve moves the job in progress
    // to the blocked queue. The failure carries no jobid; the queue
    // identifies the job by its own iterator.
    REQUIRE (queue->handle_match_failure (static_cast<flux_jobid_t> (-1), EBUSY) == 0);
}

// Count the jobs of one pending queue in the JSON that the stats-get RPC
// reports, so the test and the operator read the same oracle.
static size_t queue_size (queue_ptr_t &queue, const char *name)
{
    json::value jv;
    queue->to_json_value (jv);
    json_t *arr = json_object_get (json_object_get (jv.get (), "pending_queues"), name);
    REQUIRE (json_is_array (arr));
    return json_array_size (arr);
}

static size_t blocked (queue_ptr_t &queue)
{
    return queue_size (queue, "blocked");
}

static size_t pending (queue_ptr_t &queue)
{
    return queue_size (queue, "pending");
}

// Callback context with the reactor that owns the idle watcher. The
// prepare callback starts that watcher, so it must exist. The flux
// handle stays unset: it is used on error paths only, where flux_log
// falls back to stderr.
struct cb_fixture_t {
    qmanager_cb_ctx_t ctx;
    flux_reactor_t *r{nullptr};

    cb_fixture_t ()
    {
        r = flux_reactor_create (0);
        REQUIRE (r != nullptr);
        ctx.idle = flux_idle_watcher_create (r, nullptr, nullptr);
        REQUIRE (ctx.idle != nullptr);
    }

    ~cb_fixture_t ()
    {
        flux_watcher_destroy (ctx.idle);
        flux_reactor_destroy (r);
    }

    cb_fixture_t (const cb_fixture_t &) = delete;
    cb_fixture_t &operator= (const cb_fixture_t &) = delete;

    queue_ptr_t queue (const std::string &name)
    {
        return ctx.queues.at (name);
    }

    queue_ptr_t add_queue (const std::string &name)
    {
        queue_ptr_t queue = std::make_shared<easy_queue_t> ();
        ctx.queues[name] = queue;
        return queue;
    }

    // Add a queue that holds the given jobs in the running state.
    queue_ptr_t add_running_queue (const std::string &name, const std::vector<flux_jobid_t> &ids)
    {
        queue_ptr_t queue = add_queue (name);
        for (auto id : ids)
            submit (queue, id);
        start_loop (queue);
        for (auto id : ids)
            allocate_job (queue, id);
        // The pending queue is empty, so the loop has ended.
        REQUIRE (!queue->is_sched_loop_active ());
        // Collect the allocated jobs and clear the per-loop state, as the
        // module does after every loop.
        while (queue->alloced_pop () != nullptr) {
        }
        queue->reset_scheduled ();
        queue->set_schedulability (false);
        return queue;
    }

    // Add a queue that holds one job in the blocked state.
    queue_ptr_t add_blocked_queue (const std::string &name, flux_jobid_t id)
    {
        queue_ptr_t queue = add_queue (name);
        submit (queue, id);
        start_loop (queue);
        block_job (queue);
        REQUIRE (blocked (queue) == 1);
        REQUIRE (pending (queue) == 0);
        REQUIRE (!queue->is_sched_loop_active ());
        queue->set_schedulability (false);
        return queue;
    }

    void prep ()
    {
        qmanager_safe_cb_t::prep_watcher_cb (r, nullptr, 0, &ctx);
    }

    // Run one prepare callback so that every test starts with the flags
    // down and with nothing left to schedule.
    void settle ()
    {
        prep ();
        REQUIRE (ctx.pls_sched_loop == false);
        REQUIRE (ctx.resources_freed == false);
    }

    // One job runs in "batch" and one job waits in "debug" for the
    // resources that the running job holds.
    void hold_and_block ()
    {
        add_running_queue ("batch", {1});
        add_blocked_queue ("debug", 2);
        settle ();
    }

    // Deliver a .free RPC. The R argument of the callback is unused on
    // this path: the callback reads R from the request payload.
    void free_resources (flux_jobid_t id, bool final)
    {
        char payload[128];
        snprintf (payload,
                  sizeof (payload),
                  "{\"id\":%ju,\"R\":{},\"final\":%s}",
                  static_cast<uintmax_t> (id),
                  final ? "true" : "false");
        msg_ptr_t msg (flux_request_encode ("sched.free", payload));
        REQUIRE (msg != nullptr);
        qmanager_safe_cb_t::jobmanager_free_cb (nullptr, msg.get (), nullptr, &ctx);
    }
};

TEST_CASE ("a final free in one queue unblocks the other queue at the next prepare callback",
           "[qmanager_callbacks]")
{
    cb_fixture_t f;
    f.hold_and_block ();
    queue_ptr_t batch = f.queue ("batch");
    queue_ptr_t debug = f.queue ("debug");

    f.free_resources (1, true);
    // The free callback records the release only. It reconsiders the
    // blocked jobs of the queue that held the resources, not of the
    // other queues.
    CHECK (f.ctx.resources_freed == true);
    CHECK (blocked (debug) == 1);

    f.prep ();
    CHECK (f.ctx.resources_freed == false);
    CHECK (blocked (debug) == 0);
    CHECK (pending (debug) == 1);
    CHECK (debug->is_schedulable ());
    CHECK (f.ctx.pls_sched_loop == true);
    // The queue that released the resources has no blocked job of its own.
    CHECK (blocked (batch) == 0);
}

TEST_CASE ("a prepare callback without a free does not reconsider blocked jobs",
           "[qmanager_callbacks]")
{
    cb_fixture_t f;
    f.hold_and_block ();
    queue_ptr_t debug = f.queue ("debug");

    // A blocked job must wait for an event. A move back to the pending
    // queue on every reactor turn would retry it on every scheduling
    // loop, which is what the blocked queue exists to prevent.
    for (int i = 0; i < 3; i++)
        f.prep ();

    CHECK (blocked (debug) == 1);
    CHECK (!debug->is_schedulable ());
    CHECK (f.ctx.pls_sched_loop == false);
}

TEST_CASE ("a free during an active scheduling loop is applied when the loop ends",
           "[qmanager_callbacks]")
{
    cb_fixture_t f;
    f.hold_and_block ();
    queue_ptr_t debug = f.queue ("debug");

    // A third job starts a new loop in "debug" and waits for a match
    // response that never comes.
    submit (debug, 3);
    start_loop (debug);
    REQUIRE (debug->is_sched_loop_active ());

    f.free_resources (1, true);
    f.prep ();
    CHECK (f.ctx.resources_freed == false);
    // The move out of the blocked queue waits for the end of the loop,
    // because the loop iterates over the pending queue.
    CHECK (blocked (debug) == 1);
    CHECK (pending (debug) == 1);

    block_job (debug);
    REQUIRE (!debug->is_sched_loop_active ());
    CHECK (blocked (debug) == 0);
    CHECK (pending (debug) == 2);
    CHECK (debug->is_schedulable ());
}

TEST_CASE ("a partial free unblocks the other queue", "[qmanager_callbacks]")
{
    cb_fixture_t f;
    f.hold_and_block ();
    queue_ptr_t batch = f.queue ("batch");
    queue_ptr_t debug = f.queue ("debug");

    f.free_resources (1, false);
    CHECK (f.ctx.resources_freed == true);
    // The job gave back part of its allocation and keeps running.
    CHECK (batch->lookup (1) != nullptr);

    f.prep ();
    CHECK (f.ctx.resources_freed == false);
    CHECK (blocked (debug) == 0);
    CHECK (pending (debug) == 1);
}

TEST_CASE ("two frees in one reactor turn cause one reconsideration", "[qmanager_callbacks]")
{
    cb_fixture_t f;
    f.add_running_queue ("batch", {1, 4});
    f.add_blocked_queue ("debug", 2);
    f.settle ();
    queue_ptr_t debug = f.queue ("debug");

    f.free_resources (1, true);
    f.free_resources (4, true);
    CHECK (f.ctx.resources_freed == true);

    f.prep ();
    CHECK (blocked (debug) == 0);
    CHECK (f.ctx.resources_freed == false);

    // Nothing was released since the last prepare callback.
    f.prep ();
    CHECK (f.ctx.resources_freed == false);
}

TEST_CASE ("a free wakes blocked jobs of every other queue", "[qmanager_callbacks]")
{
    cb_fixture_t f;
    f.hold_and_block ();
    queue_ptr_t extra = f.add_blocked_queue ("extra", 5);
    queue_ptr_t debug = f.queue ("debug");

    f.free_resources (1, true);
    f.prep ();
    CHECK (blocked (debug) == 0);
    CHECK (blocked (extra) == 0);
    CHECK (pending (debug) == 1);
    CHECK (pending (extra) == 1);
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

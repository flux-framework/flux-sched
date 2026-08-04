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

#include <string>
#include <catch2/catch_test_macros.hpp>
#include "qmanager/modules/qmanager_opts.hpp"

using namespace Flux::opts_manager;

// Test the resolution of an RFC 33 virtual queue name to its parent
// queue's internal queue name -- this is the helper that qmanager uses
// to decide which internal queue a job should be scheduled in.

TEST_CASE ("resolve_queue_name is a no-op with no virtual queues", "[qmanager_opts]")
{
    qmanager_opts_t opts;

    // With no "vqueue-parents" ever parsed, every name resolves to
    // itself. This models current (pre-RFC33-vqueue) flux-core
    // behavior, where this feature must be a strict no-op.
    CHECK (opts.resolve_queue_name ("batch") == "batch");
    CHECK (opts.resolve_queue_name ("default") == "default");
}

TEST_CASE ("resolve_queue_name maps a virtual queue to its parent", "[qmanager_opts]")
{
    qmanager_opts_t opts;
    std::string info;

    CHECK (opts.parse ("vqueue-parents", "expedite:batch", info) == 0);
    CHECK (opts.resolve_queue_name ("expedite") == "batch");
    CHECK (opts.resolve_queue_name ("batch") == "batch");
    CHECK (opts.resolve_queue_name ("debug") == "debug");
}

TEST_CASE ("resolve_queue_name maps multiple virtual queues", "[qmanager_opts]")
{
    qmanager_opts_t opts;
    std::string info;

    CHECK (opts.parse ("vqueue-parents", "expedite:batch urgent:batch", info) == 0);
    CHECK (opts.resolve_queue_name ("expedite") == "batch");
    CHECK (opts.resolve_queue_name ("urgent") == "batch");
}

TEST_CASE ("queue-policy-per-queue rejects a virtual queue name", "[qmanager_opts]")
{
    qmanager_opts_t opts;
    std::string info;

    // Only "batch" is a real (internal) queue; "expedite" is an RFC 33
    // virtual queue parented under "batch" and therefore has no internal
    // queue of its own.
    CHECK (opts.parse ("queues", "batch", info) == 0);
    CHECK (opts.parse ("vqueue-parents", "expedite:batch", info) == 0);

    // A per-queue policy naming the real queue succeeds.
    info.clear ();
    CHECK (opts.parse ("queue-policy-per-queue", "batch:fcfs", info) == 0);

    // But naming the virtual queue is rejected: since a virtual queue is
    // never inserted into m_per_queue_prop, it is reported as an unknown
    // queue (a virtual queue cannot have its own scheduler policy).
    info.clear ();
    errno = 0;
    CHECK (opts.parse ("queue-policy-per-queue", "expedite:fcfs", info) == -1);
    CHECK (errno == ENOENT);
    CHECK (info.find ("Unknown queue (expedite)") != std::string::npos);
}

TEST_CASE ("operator+= merges virtual queue parents", "[qmanager_opts]")
{
    qmanager_opts_t dst;
    qmanager_opts_t src;
    std::string info;

    // This mirrors how qmanager applies parsed options: a freshly parsed
    // opts object is merged into the live config via operator+=. Without
    // that merge carrying m_vqueue_parents, resolution on the destination
    // would silently fall back to a no-op.
    CHECK (src.parse ("vqueue-parents", "expedite:batch", info) == 0);
    CHECK (dst.resolve_queue_name ("expedite") == "expedite");

    dst += src;
    CHECK (dst.resolve_queue_name ("expedite") == "batch");
}

TEST_CASE ("operator+= leaves virtual queue parents intact when src has none", "[qmanager_opts]")
{
    qmanager_opts_t dst;
    qmanager_opts_t src;
    std::string info;

    // A subsequent merge that does not itself set vqueue-parents must not
    // clobber an already-resolved mapping (the merge only overwrites when
    // the source is non-empty).
    CHECK (dst.parse ("vqueue-parents", "expedite:batch", info) == 0);
    CHECK (src.parse ("queue-policy", "fcfs", info) == 0);

    dst += src;
    CHECK (dst.resolve_queue_name ("expedite") == "batch");
}

// Classification of an RFC 33 queues config table into real queues and
// virtual queues, including rejection of malformed configs. This is the
// logic qmanager runs on (re)load before committing a new config.

TEST_CASE ("classify_queues splits real and virtual queues", "[qmanager_opts]")
{
    // { "batch": { "requires": [...] }, "expedite": { "parent": "batch" } }
    json_t *conf = json_pack ("{s:{s:[s]}, s:{s:s}}",
                              "batch",
                              "requires",
                              "batch",
                              "expedite",
                              "parent",
                              "batch");
    REQUIRE (conf != nullptr);

    std::string queues, vqueue_parents, err;
    CHECK (classify_queues (conf, queues, vqueue_parents, err) == 0);
    // Only the real queue appears in "queues"; the virtual queue maps to
    // its parent in "vqueue-parents".
    CHECK (queues == "batch ");
    CHECK (vqueue_parents == "expedite:batch ");
    json_decref (conf);
}

TEST_CASE ("classify_queues handles a config with no virtual queues", "[qmanager_opts]")
{
    json_t *conf = json_pack ("{s:{s:[s]}}", "batch", "requires", "batch");
    REQUIRE (conf != nullptr);

    std::string queues, vqueue_parents, err;
    CHECK (classify_queues (conf, queues, vqueue_parents, err) == 0);
    CHECK (queues == "batch ");
    CHECK (vqueue_parents.empty ());
    json_decref (conf);
}

TEST_CASE ("classify_queues rejects a parent that is not a queue", "[qmanager_opts]")
{
    // "expedite" names a parent "nope" that is not a configured queue.
    json_t *conf = json_pack ("{s:{s:s}}", "expedite", "parent", "nope");
    REQUIRE (conf != nullptr);

    std::string queues, vqueue_parents, err;
    errno = 0;
    CHECK (classify_queues (conf, queues, vqueue_parents, err) == -1);
    CHECK (errno == EINVAL);
    CHECK (err.find ("unknown parent queue") != std::string::npos);
    json_decref (conf);
}

TEST_CASE ("classify_queues rejects a parent that is itself virtual", "[qmanager_opts]")
{
    // "default" is a real queue, "batch" is a valid virtual queue parented
    // under it, and "expedite" tries to parent under "batch" -- but a
    // virtual queue may not be a parent, so this is rejected.
    json_t *conf = json_pack ("{s:{s:[s]}, s:{s:s}, s:{s:s}}",
                              "default",
                              "requires",
                              "default",
                              "batch",
                              "parent",
                              "default",
                              "expedite",
                              "parent",
                              "batch");
    REQUIRE (conf != nullptr);

    std::string queues, vqueue_parents, err;
    errno = 0;
    CHECK (classify_queues (conf, queues, vqueue_parents, err) == -1);
    CHECK (errno == EINVAL);
    CHECK (err.find ("itself a virtual queue") != std::string::npos);
    json_decref (conf);
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

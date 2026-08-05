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
#include <chrono>
#include <cstdint>
#include <limits>

#include <catch2/catch_test_macros.hpp>

#include "resource/libjobspec/jobspec.hpp"
#include "resource/store/resource_graph_store.hpp"
#include "resource/traversers/dfu_impl.hpp"

using Flux::resource_model::graph_duration_t;
using Flux::resource_model::detail::jobmeta_t;
using alloc_type_t = jobmeta_t::alloc_type_t;

namespace {

// A jobspec with the given duration and nothing else set.
Flux::Jobspec::Jobspec make_jobspec (double duration)
{
    Flux::Jobspec::Jobspec js;
    js.attributes.system.duration = duration;
    return js;
}

// Default graph window is [0, SYSTEM_MAX_DURATION), i.e. ~100 years, so
// g_duration is large enough not to interfere with the duration tests below.
graph_duration_t default_graph_duration ()
{
    return graph_duration_t{};
}

// A graph window of exactly `seconds` seconds.
graph_duration_t graph_duration_of (int64_t seconds)
{
    graph_duration_t gd;
    gd.graph_start = std::chrono::system_clock::from_time_t (0);
    gd.graph_end = std::chrono::system_clock::from_time_t (seconds);
    return gd;
}

}  // namespace

TEST_CASE ("integer duration is preserved", "[jobmeta]")
{
    jobmeta_t m;
    auto js = make_jobspec (3600.0);
    auto gd = default_graph_duration ();
    REQUIRE (m.build (js, alloc_type_t::AT_ALLOC, 1, 0, gd) == 0);
    REQUIRE (m.duration == 3600);
}

TEST_CASE ("fractional duration rounds up", "[jobmeta]")
{
    jobmeta_t m;
    auto js = make_jobspec (1.2);
    auto gd = default_graph_duration ();
    REQUIRE (m.build (js, alloc_type_t::AT_ALLOC, 1, 0, gd) == 0);
    REQUIRE (m.duration == 2);  // truncation would have given 1
}

TEST_CASE ("sub-second duration maps to 1, not 0", "[jobmeta]")
{
    // The planner rejects duration < 1 with EINVAL; before b11ff4b, 0.1
    // truncated to 0 and the match failed.
    jobmeta_t m;
    auto js = make_jobspec (0.1);
    auto gd = default_graph_duration ();
    REQUIRE (m.build (js, alloc_type_t::AT_ALLOC, 1, 0, gd) == 0);
    REQUIRE (m.duration == 1);
}

TEST_CASE ("zero duration falls back to the graph duration", "[jobmeta]")
{
    jobmeta_t m;
    auto js = make_jobspec (0.0);
    auto gd = graph_duration_of (1234);
    REQUIRE (m.build (js, alloc_type_t::AT_ALLOC, 1, 0, gd) == 0);
    REQUIRE (m.duration == 1234);
}

TEST_CASE ("NaN duration is rejected with EINVAL", "[jobmeta]")
{
    jobmeta_t m;
    auto js = make_jobspec (std::numeric_limits<double>::quiet_NaN ());
    auto gd = default_graph_duration ();
    errno = 0;
    REQUIRE (m.build (js, alloc_type_t::AT_ALLOC, 1, 0, gd) == -1);
    REQUIRE (errno == EINVAL);
}

TEST_CASE ("inf duration is rejected with EINVAL", "[jobmeta]")
{
    jobmeta_t m;
    auto js = make_jobspec (std::numeric_limits<double>::infinity ());
    auto gd = default_graph_duration ();
    errno = 0;
    REQUIRE (m.build (js, alloc_type_t::AT_ALLOC, 1, 0, gd) == -1);
    REQUIRE (errno == EINVAL);
}

TEST_CASE ("negative duration is rejected with EINVAL", "[jobmeta]")
{
    jobmeta_t m;
    auto js = make_jobspec (-1.0);
    auto gd = default_graph_duration ();
    errno = 0;
    REQUIRE (m.build (js, alloc_type_t::AT_ALLOC, 1, 0, gd) == -1);
    REQUIRE (errno == EINVAL);
}

TEST_CASE ("duration longer than the graph window is rejected", "[jobmeta]")
{
    jobmeta_t m;
    auto js = make_jobspec (20.0);
    auto gd = graph_duration_of (10);
    errno = 0;
    REQUIRE (m.build (js, alloc_type_t::AT_ALLOC, 1, 0, gd) == -1);
    REQUIRE (errno == EINVAL);
}

TEST_CASE ("duration too large is rejected with EINVAL", "[jobmeta]")
{
    jobmeta_t m;
    auto js = make_jobspec (std::numeric_limits<int64_t>::max ());
    auto gd = graph_duration_of (std::numeric_limits<int64_t>::max ());
    errno = 0;
    REQUIRE (m.build (js, alloc_type_t::AT_ALLOC, 1, 0, gd) == -1);
    REQUIRE (errno == EINVAL);
}

TEST_CASE ("empty graph window is rejected with EINVAL", "[jobmeta]")
{
    jobmeta_t m;
    auto js = make_jobspec (5.0);
    auto gd = graph_duration_of (0);  // graph_start == graph_end
    errno = 0;
    REQUIRE (m.build (js, alloc_type_t::AT_ALLOC, 1, 0, gd) == -1);
    REQUIRE (errno == EINVAL);
}

TEST_CASE ("build populates the non-duration fields", "[jobmeta]")
{
    jobmeta_t m;
    auto js = make_jobspec (100.0);
    js.attributes.system.queue = "batch";
    auto gd = default_graph_duration ();
    REQUIRE (m.build (js, alloc_type_t::AT_ALLOC_ORELSE_RESERVE, 42, 7, gd) == 0);
    REQUIRE (m.jobid == 42);
    REQUIRE (m.at == 7);
    REQUIRE (m.now == 7);
    REQUIRE (m.alloc_type == alloc_type_t::AT_ALLOC_ORELSE_RESERVE);
    REQUIRE (m.is_queue_set ());
    REQUIRE (m.get_queue () == "batch");
}

TEST_CASE ("queue is unset when jobspec has no queue", "[jobmeta]")
{
    jobmeta_t m;
    auto js = make_jobspec (100.0);
    auto gd = default_graph_duration ();
    REQUIRE (m.build (js, alloc_type_t::AT_ALLOC, 1, 0, gd) == 0);
    REQUIRE_FALSE (m.is_queue_set ());
}

/*****************************************************************************\
 * Copyright 2024 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef PERF_DATA_HPP
#define PERF_DATA_HPP

#include <chrono>

namespace Flux {
namespace resource_model {

struct perf_stats {
    void update_stats (double elapsed, int64_t jobid, int64_t match_iter_ct)
    {
        /* Update using Welford's algorithm */
        double delta = 0.0;
        double delta2 = 0.0;
        njobs++;
        njobs_reset++;
        min = (min > elapsed) ? elapsed : min;
        if (max < elapsed) {
            max = elapsed;
            max_match_jobid = jobid;
            match_iter_count = match_iter_ct;
        }
        /* Welford's online algorithm for variance */
        delta = elapsed - avg;
        avg += delta / (double)njobs_reset;
        delta2 = elapsed - avg;
        M2 += delta * delta2;
    }

    /* Performance data */
    uint64_t njobs = 0;          /* Total match count */
    uint64_t njobs_reset = 0;    /* Jobs since match count reset */
    int64_t max_match_jobid = 0; /* jobid corresponding to max match time */
    int64_t match_iter_count = 0;
    double min = std::numeric_limits<double>::max (); /* Min match time */
    double max = 0.0;                                 /* Max match time */
    double accum = 0.0; /* Accumulated match time for resource query */
    double avg = 0.0;   /* Average match time */
    double M2 = 0.0;    /* Welford's algorithm */
};

struct match_perf_t {
    double load = 0.0; /* Graph load time */
    /* Graph uptime in seconds */
    std::chrono::time_point<std::chrono::system_clock> graph_uptime =
        std::chrono::system_clock::now ();
    /* Time since stats were last cleared */
    std::chrono::time_point<std::chrono::system_clock> time_of_last_reset =
        std::chrono::system_clock::now ();
    perf_stats succeeded;
    perf_stats failed;
    int64_t tmp_iter_count = -1;
};

extern struct match_perf_t perf;

}  // namespace resource_model
}  // namespace Flux

#endif  // PERF_DATA_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

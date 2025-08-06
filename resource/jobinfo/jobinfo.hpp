/*****************************************************************************\
 * Copyright 2014 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef JOBINFO_HPP
#define JOBINFO_HPP

#include <string>
#include <stdint.h>

namespace Flux {
namespace resource_model {

enum class job_lifecycle_t { INIT, ALLOCATED, RESERVED, MATCHED, CANCELED, ERROR };

struct job_info_t {
    job_info_t (uint64_t j,
                job_lifecycle_t s,
                int64_t at,
                const std::string &j_fn,
                const std::string &jstr,
                const std::string &R_str,
                double o);

    job_info_t (uint64_t j,
                job_lifecycle_t s,
                int64_t at,
                const std::string &j_fn,
                const std::string &jstr,
                double o);

    uint64_t jobid = UINT64_MAX;
    job_lifecycle_t state = job_lifecycle_t::INIT;
    int64_t scheduled_at = -1;
    std::string jobspec_fn = "";
    std::string jobspec_str = "";
    std::string R = "";
    double overhead = 0.0f;
};

void get_jobstate_str (job_lifecycle_t state, std::string &status);

}  // namespace resource_model
}  // namespace Flux

#endif  // JOBINFO_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

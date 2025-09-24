/*****************************************************************************\
 * Copyright 2014 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

extern "C" {
#if HAVE_CONFIG_H
#include "config.h"
#endif
}

#include <string>
#include "resource/jobinfo/jobinfo.hpp"

namespace Flux {
namespace resource_model {

job_info_t::job_info_t (uint64_t j,
                        job_lifecycle_t s,
                        int64_t at,
                        const std::string &fn,
                        const std::string &jstr,
                        const std::string &R_str,
                        double o)
    : jobid (j),
      state (s),
      scheduled_at (at),
      jobspec_fn (fn),
      jobspec_str (jstr),
      R (R_str),
      overhead (o)
{
}

job_info_t::job_info_t (uint64_t j,
                        job_lifecycle_t s,
                        int64_t at,
                        const std::string &fn,
                        const std::string &jstr,
                        double o)
    : jobid (j), state (s), scheduled_at (at), jobspec_fn (fn), jobspec_str (jstr), overhead (o)
{
}

void get_jobstate_str (job_lifecycle_t state, std::string &status)
{
    switch (state) {
        case job_lifecycle_t::ALLOCATED:
            status = "ALLOCATED";
            break;
        case job_lifecycle_t::RESERVED:
            status = "RESERVED";
            break;
        case job_lifecycle_t::MATCHED:
            status = "MATCHED";
            break;
        case job_lifecycle_t::CANCELED:
            status = "CANCELED";
            break;
        case job_lifecycle_t::ERROR:
            status = "ERROR";
            break;
        case job_lifecycle_t::INIT:
        default:
            status = "INIT";
            break;
    }
    return;
}

}  // namespace resource_model
}  // namespace Flux

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

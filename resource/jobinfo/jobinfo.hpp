/*****************************************************************************\
 *  Copyright (c) 2014 Lawrence Livermore National Security, LLC.  Produced at
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

#ifndef JOBINFO_HPP
#define JOBINFO_HPP

#include <string>

namespace Flux {
namespace resource_model {

enum class job_lifecycle_t { INIT, ALLOCATED, RESERVED, CANCELED, ERROR };

struct job_info_t {
    job_info_t (uint64_t j, job_lifecycle_t s, int64_t at,
                const std::string &j_fn, const std::string &jstr,
                const std::string &R_str,  double o);

    job_info_t (uint64_t j, job_lifecycle_t s, int64_t at,
                const std::string &j_fn, const std::string &jstr, double o);

    uint64_t jobid = UINT64_MAX;
    job_lifecycle_t state = job_lifecycle_t::INIT;
    int64_t scheduled_at = -1;
    std::string jobspec_fn = "";
    std::string jobspec_str = "";
    std::string R = "";
    double overhead = 0.0f;
};

void get_jobstate_str (job_lifecycle_t state, std::string &status);

} // namespace resource_model
} // namespace Flux

#endif // JOBINFO_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

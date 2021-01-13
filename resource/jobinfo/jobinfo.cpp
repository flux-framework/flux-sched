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

#include <string>
#include "resource/jobinfo/jobinfo.hpp"

namespace Flux {
namespace resource_model {

job_info_t::job_info_t (uint64_t j, job_lifecycle_t s, int64_t at, const std::string &fn,
                        const std::string &jstr, const std::string &R_str,  double o)
    : jobid (j), state (s), scheduled_at (at), jobspec_fn (fn), jobspec_str (jstr),
      R (R_str), overhead (o)
{

}

job_info_t::job_info_t (uint64_t j, job_lifecycle_t s, int64_t at, const std::string &fn,
                        const std::string &jstr, double o)
    : jobid (j), state (s), scheduled_at (at), jobspec_fn (fn), jobspec_str (jstr),
      overhead (o)
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

}
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

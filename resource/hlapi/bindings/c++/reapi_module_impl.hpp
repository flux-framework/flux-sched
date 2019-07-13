/*****************************************************************************\
 *  Copyright (c) 2019 Lawrence Livermore National Security, LLC.  Produced at
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

#ifndef REAPI_MODULE_IMPL_HPP
#define REAPI_MODULE_IMPL_HPP

extern "C" {
#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <flux/core.h>
}

#include <cerrno>
#include "resource/hlapi/bindings/c++/reapi_module.hpp"

namespace Flux {
namespace resource_model {
namespace detail {

int reapi_module_t::match_allocate (void *h, bool orelse_reserve,
                                    const std::string &jobspec,
                                    const uint64_t jobid, bool &reserved,
                                    std::string &R, int64_t &at, double &ov)
{
    int rc = -1;
    int64_t rj = -1;
    flux_t *fh = (flux_t *)h;
    flux_future_t *f = NULL;
    const char *rset = NULL;
    const char *status = NULL;
    const char *cmd = (orelse_reserve)? "allocate_orelse_reserve"
                                      : "allocate_with_satisfiability";

    if (!fh || jobspec == "" || jobid > INT64_MAX) {
        errno = EINVAL;
        goto out;
    }

    if (!(f = flux_rpc_pack (fh, "resource.match", FLUX_NODEID_ANY, 0,
                             "{s:s s:I s:s}",
                             "cmd", cmd, "jobid", (const int64_t)jobid,
                             "jobspec", jobspec.c_str ()))) {
        goto out;
    }

    if (flux_rpc_get_unpack (f, "{s:I s:s s:f s:s s:I}",
                             "jobid", &rj, "status", &status,
                             "overhead", &ov, "R", &rset, "at", &at) < 0) {
        goto out;
    }
    reserved = (std::string ("RESERVED") == status)? true : false;
    R = rset;
    if (rj != (int64_t)jobid) {
        errno = EINVAL;
        goto out;
    }
    rc = 0;

out:
    flux_future_destroy (f);
    return rc;
}

int reapi_module_t::cancel (void *h, const uint64_t jobid)
{
    int rc = -1;
    flux_t *fh = (flux_t *)h;
    flux_future_t *f = NULL;

    if (!fh || jobid > INT64_MAX) {
        errno = EINVAL;
        goto out;
    }
    if (!(f = flux_rpc_pack (fh, "resource.cancel", FLUX_NODEID_ANY, 0,
                             "{s:I}", "jobid", (const int64_t)jobid))) {
        goto out;
    }
    if ((rc = flux_rpc_get (f, NULL)) < 0) {
        goto out;
    }
    rc = 0;

out:
    flux_future_destroy (f);
    return rc;
}

int reapi_module_t::info (void *h, const uint64_t jobid,
                          bool &reserved, int64_t &at, double &ov)
{
    int rc = -1;
    int64_t rj = -1;
    flux_t *fh = (flux_t *)h;
    flux_future_t *f = NULL;
    const char *status = NULL;

    if (!fh || jobid > INT64_MAX) {
        errno = EINVAL;
        goto out;
    }
    if (!(f = flux_rpc_pack (fh, "resource.info", FLUX_NODEID_ANY, 0,
                             "{s:I}", "jobid", (const int64_t)jobid))) {
        goto out;
    }
    if (flux_rpc_get_unpack (f, "{s:I s:s s:I s:f}",
                             "jobid", &rj, "status", &status,
                             "at", &at, "overhead", &ov) < 0) {
        goto out;
    }
    reserved = (std::string ("RESERVED") == status)? true : false;
    if (rj != (int64_t)jobid) {
        errno = EINVAL;
        goto out;
    }
    rc = 0;

out:
    flux_future_destroy (f);
    return rc;
}

int reapi_module_t::stat (void *h, int64_t &V, int64_t &E,int64_t &J,
                          double &load, double &min, double &max, double &avg)
{
    int rc = -1;
    flux_t *fh = (flux_t *)h;
    flux_future_t *f = NULL;

    if (!fh) {
        errno = EINVAL;
        goto out;
    }

    if (!(f = flux_rpc (fh, "resource.stat", NULL, FLUX_NODEID_ANY, 0))) {
        goto out;
    }
    if ((rc = flux_rpc_get_unpack (f, "{s:I s:I s:f s:I s:f s:f s:f}",
                                   "V", &V, "E", &E, "load-time", &load,
                                   "njobs", &J, "min-match", &min,
                                   "max-match", &max, "avg-match", &avg)) < 0) {
        goto out;
    }

out:
    flux_future_destroy (f);
    return rc;
}

} // namespace Flux::resource_model::detail
} // namespace Flux::resource_model
} // namespace Flux

#endif // REAPI_MODULE_IMPL_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

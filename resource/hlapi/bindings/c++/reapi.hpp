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

#ifndef REAPI_HPP
#define REAPI_HPP

extern "C" {
#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <flux/core.h>
}

#include <cstdint>
#include <string>

namespace Flux {
namespace resource_model {


/*! High-level resource API base class. Derived classes must implement
 *  the methods.
 */
class reapi_t {
public:
    /*! Match a jobspec to the "best" resources and either allocate
     *  orelse reserve them. The best resources are determined by
     *  the selected match policy.
     *
     *  \param h         Opaque handle. How it is used is an implementation
     *                   detail. However, when it is used within a Flux's
     *                   service module, it is expected to be a pointer
     *                   to a flux_t object.
     *  \param orelse_reserve
     *                   Boolean: if false, only allocate; otherwise, first try
     *                   to allocate and if that fails, reserve.
     *  \param jobspec   jobspec string.
     *  \param jobid     jobid of the uint64_t type.
     *  \param reserved  Boolean into which to return true if this job has been
     *                   reserved instead of allocated.
     *  \param R         String into which to return the resource set either
     *                   allocated or reserved.
     *  \param at        If allocated, 0 is returned; if reserved, actual time
     *                   at which the job is reserved.
     *  \param ov        Double into which to return performance overhead
     *                   in terms of elapse time needed to complete
     *                   the match operation.
     *  \return          0 on success; -1 on error.
     */
    static int match_allocate (void *h, bool orelse_reserve,
                               const std::string &jobspec, const uint64_t jobid,
                               bool &reserved,
                               std::string &R, int64_t &at, double &ov)
    {
        return -1;
    }

    /*! Update the resource state with R.
     *
     *  \param h         Opaque handle. How it is used is an implementation
     *                   detail. However, when it is used within a Flux's
     *                   service module, it is expected to be a pointer
     *                   to a flux_t object.
     *  \param jobid     jobid of the uint64_t type.
     *  \param R         R String of std::string.
     *  \param at        return the scheduled time.
     *  \param ov        return the performance overhead
     *                   in terms of elapse time needed to complete
     *                   the update operation.
     *  \param R_out     return the updated R string.
     *  \return          0 on success; -1 on error.
     */
    static int update_allocate (void *h, const uint64_t jobid,
                                const std::string &R, int64_t &at, double &ov,
                                std::string &R_out)
    {
        return -1;
    }

    /*! Cancel the allocation or reservation corresponding to jobid.
     *
     *  \param h         Opaque handle. How it is used is an implementation
     *                   detail. However, when it is used within a Flux's
     *                   service module, it is expected to be a pointer
     *                   to a flux_t object.
     *  \param jobid     jobid of the uint64_t type.
     *  \return          0 on success; -1 on error.
     */
    static int cancel (void *h, const uint64_t jobid)
    {
        return -1;
    }


    /*! Get the information on the allocation or reservation corresponding
     *  to jobid.
     *
     *  \param h         Opaque handle. How it is used is an implementation
     *                   detail. However, when it is used within a Flux's
     *                   service module, it is expected to be a pointer
     *                   to a flux_t object.
     *  \param jobid     const jobid of the uint64_t type.
     *  \param reserved  Boolean into which to return true if this job has been
     *                   reserved instead of allocated.
     *  \param at        If allocated, 0 is returned; if reserved, actual time
     *                   at which the job is reserved.
     *  \param ov        Double into which to return performance overhead
     *                   in terms of elapse time needed to complete
     *                   the match operation.
     *  \return          0 on success; -1 on error.
     */
    static int info (void *h, const uint64_t jobid,
                     bool &reserved, int64_t &at, double &ov)
    {
        return -1;
    }

    /*! Get the performance information about the resource infrastructure.
     *
     *  \param h         Opaque handle. How it is used is an implementation
     *                   detail. However, when it is used within a Flux's
     *                   service module, it is expected to be a pointer
     *                   to a flux_t object.
     *  \param V         Number of resource vertices
     *  \param E         Number of edges
     *  \param J         Number of jobs
     *  \param load      Graph load time
     *  \param min       Min match time
     *  \param max       Max match time
     *  \param avg       Avg match time
     *  \return          0 on success; -1 on error.
     */
    static int stat (void *h, int64_t &V, int64_t &E,int64_t &J,
                     double &load, double &min, double &max, double &avg)
    {
        return -1;
    }
};

} // namespace Flux::resource_model
} // namespace Flux

#endif // REAPI_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

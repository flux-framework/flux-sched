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

class reapi_t {
public:
    static int match_allocate (void *h, bool orelse_reserve,
                               const std::string &jobspec, const uint64_t jobid,
                               bool &reserved,
                               std::string &R, int64_t &at, double &ov)
    {
        return -1;
    }
    static int cancel (void *h, const uint64_t jobid)
    {
        return -1;
    }
    static int info (void *h, const uint64_t jobid,
                     bool &reserved, int64_t &at, double &ov)
    {
        return -1;
    }
    static int stat (void *h, int64_t &V, int64_t &E,int64_t &J,
                     double &load, double &min, double &max, double &avg)
    {
        return -1;
    }
};

} // namespace resource_model
} // namespace Flux

#endif // REAPI_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

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

#ifndef REAPI_CLI_HPP
#define REAPI_CLI_HPP

extern "C" {
#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <flux/core.h>
#include <cstdint>
#include <cerrno>
}

#include "resource/hlapi/bindings/c++/reapi.hpp"

namespace Flux {
namespace resource_model {
namespace detail {

class reapi_cli_t : public reapi_t {
public:
    static int match_allocate (void *h, bool orelse_reserve,
                               const std::string &jobspec,
                               const uint64_t jobid, bool &reserved,
                               std::string &R, int64_t &at, double &ov);
    static int match_allocate_multi (void *h, bool orelse_reserve,
                                     const char *jobs,
                                     queue_adapter_base_t *adapter);
    static int update_allocate (void *h, const uint64_t jobid,
                                const std::string &R, int64_t &at, double &ov,
                                std::string &R_out);
    static int cancel (void *h, const int64_t jobid, bool noent_ok);
    static int info (void *h, const int64_t jobid,
                     bool &reserved, int64_t &at, double &ov);
    static int stat (void *h, int64_t &V, int64_t &E,int64_t &J,
                     double &load, double &min, double &max, double &avg);
};


} // namespace Flux::resource_model::detail
} // namespace Flux::resource_model
} // namespace Flux

#endif // REAPI_MODULE_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

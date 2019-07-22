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

#ifndef QUEUE_POLICY_FACTORY_IMPL_HPP
#define QUEUE_POLICY_FACTORY_IMPL_HPP

#include "resource/hlapi/bindings/c++/reapi.hpp"
#include "resource/hlapi/bindings/c++/reapi_module.hpp"
#include "resource/hlapi/bindings/c++/reapi_module_impl.hpp"
#include "resource/hlapi/bindings/c++/reapi_cli.hpp"
#include "resource/hlapi/bindings/c++/reapi_cli_impl.hpp"
#include "qmanager/policies/base/queue_policy_base.hpp"
#include "qmanager/policies/base/queue_policy_base_impl.hpp"
#include "qmanager/policies/queue_policy_fcfs.hpp"
#include "qmanager/policies/queue_policy_fcfs_impl.hpp"
#include "qmanager/policies/queue_policy_easy.hpp"
#include "qmanager/policies/queue_policy_easy_impl.hpp"
#include <string>

namespace Flux {
namespace queue_manager {
namespace detail {

using namespace resource_model;
using namespace resource_model::detail;

bool known_queue_policy (const std::string &policy)
{
    bool rc = false;
    if (policy == "fcfs" || policy == "easy")
        rc = true;
    return rc;
}

queue_policy_base_t *create_queue_policy (const std::string &policy,
                                          const std::string &reapi)
{
    queue_policy_base_t *p = NULL;
    if (policy == "fcfs") {
        if (reapi == "module") {
            p = (queue_policy_base_t *)
                    new (std::nothrow)queue_policy_fcfs_t<reapi_module_t> ();
        }
        else if (reapi == "cli") {
            p = (queue_policy_base_t *)
                    new (std::nothrow)queue_policy_fcfs_t<reapi_cli_t> ();
        }
    }
    else if (policy == "easy") {
        if (reapi == "module") {
            p = (queue_policy_base_t *)
                    new (std::nothrow)queue_policy_easy_t<reapi_module_t> ();
        }
        else if (reapi == "cli") {
            p = (queue_policy_base_t *)
                    new (std::nothrow)queue_policy_easy_t<reapi_cli_t> ();
        }
    }
    return p;
}

} // namespace Flux::queue_manager::detail
} // namespace Flux::queue_manager
} // namespace Flux

#endif // QUEUE_POLICY_FACTORY_IMPL_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

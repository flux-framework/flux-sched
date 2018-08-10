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
#include "resource/policies/dfu_match_policy_factory.hpp"

namespace Flux {
namespace resource_model {

bool known_match_policy (const std::string &policy)
{
    bool rc = true;
    if (policy != HIGH_ID_FIRST && policy != LOW_ID_FIRST
        && policy != LOCALITY_AWARE)
        rc = false;

    return rc;
}

dfu_match_cb_t *create_match_cb (const std::string &policy)
{
    dfu_match_cb_t *matcher = NULL;
    if (policy == HIGH_ID_FIRST)
        matcher = (dfu_match_cb_t *)new high_first_t ();
    else if (policy == LOW_ID_FIRST)
        matcher = (dfu_match_cb_t *)new low_first_t ();
    else if (policy == LOCALITY_AWARE)
        matcher = (dfu_match_cb_t *)new greater_interval_first_t ();
    return matcher;
}

} // resource_model
} // Flux

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

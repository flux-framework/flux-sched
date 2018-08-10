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

#ifndef DFU_MATCH_POLICY_FACTORY_HPP
#define DFU_MATCH_POLICY_FACTORY_HPP

#include "resource/policies/base/dfu_match_cb.hpp"
#include "resource/policies/dfu_match_high_id_first.hpp"
#include "resource/policies/dfu_match_low_id_first.hpp"
#include "resource/policies/dfu_match_locality.hpp"

namespace Flux {
namespace resource_model {

static const string HIGH_ID_FIRST = "high";
static const string LOW_ID_FIRST = "low";
static const string LOCALITY_AWARE = "locality";

bool known_match_policy (const string &policy);

/*! Factory method for creating a matching callback
 *  object, representing a matching policy.
 */
dfu_match_cb_t *create_match_cb (const string &policy);

} // resource_model
} // Flux

#endif // DFU_MATCH_POLICY_FACTORY_HPP

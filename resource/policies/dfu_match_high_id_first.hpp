/*****************************************************************************\
 * Copyright 2014 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef DFU_MATCH_HIGH_ID_FIRST_HPP
#define DFU_MATCH_HIGH_ID_FIRST_HPP

#include "resource/policies/base/dfu_match_cb.hpp"
#include "resource/policies/dfu_match_multilevel_id.hpp"

namespace Flux {
namespace resource_model {

/* High ID first policy: select resources of each type
 * with higher numeric IDs.
 */
using high_first_t = multilevel_id_t<fold::greater>;

}  // namespace resource_model
}  // namespace Flux

#endif  // DFU_MATCH_HIGH_ID_FIRST_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

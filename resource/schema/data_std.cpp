/*****************************************************************************\
 * Copyright 2024 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#include "data_std.hpp"

namespace Flux {
namespace resource_model {

subsystem_t containment_sub{"containment"};

resource_type_t cluster_rt{"cluster"};
resource_type_t core_rt{"core"};
resource_type_t socket_rt{"socket"};
resource_type_t gpu_rt{"gpu"};
resource_type_t node_rt{"node"};
resource_type_t rack_rt{"rack"};
resource_type_t slot_rt{"slot"};

}  // namespace resource_model
}  // namespace Flux

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

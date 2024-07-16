/*****************************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef DATA_STD_H
#define DATA_STD_H

#include <map>
#include <set>
#include <string>
#include <src/common/libintern/interner.hpp>

namespace Flux {
namespace resource_model {

// We create an x_checker planner for each resource vertex for quick exclusivity
// checking. We update x_checker for all of the vertices involved in each
// job allocation/reservation -- subtract 1 from x_checker planner for the
// scheduled span. Any vertex with less than X_CHECKER_NJOBS available in its
// x_checker cannot be exclusively allocated or reserved.
const char *const X_CHECKER_JOBS_STR = "jobs";
const int64_t X_CHECKER_NJOBS = 0x40000000;

struct subsystem_tag {};
using subsystem_t = intern::interned_string<subsystem_tag, uint8_t>;
extern subsystem_t containment_sub;

using multi_subsystems_t = std::map<subsystem_t, std::string>;
using multi_subsystemsS = std::map<subsystem_t, std::set<std::string>>;

}  // namespace resource_model
}  // namespace Flux

#endif  // DATA_STD_H

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

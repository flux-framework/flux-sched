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

#include "data_std.hpp"

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

constexpr uint64_t subsystem_id{0};
struct subsystem_tag {};
using subsystem_t = intern::interned_string<intern::dense_storage<subsystem_tag, uint8_t>>;
extern subsystem_t containment_sub;

constexpr uint64_t resource_type_id{1};
struct resource_type_tag {};
using resource_type_t = intern::interned_string<intern::dense_storage<resource_type_tag, uint16_t>>;
extern resource_type_t slot_rt;
extern resource_type_t cluster_rt;
extern resource_type_t rack_rt;
extern resource_type_t node_rt;
extern resource_type_t socket_rt;
extern resource_type_t gpu_rt;
extern resource_type_t core_rt;
extern resource_type_t gpu_mps_rt;

template<class T, int likely_count = 2>
using subsystem_key_vec = intern::interned_key_vec<subsystem_t, T, likely_count>;
using multi_subsystems_t = intern::interned_key_vec<subsystem_t, std::string>;
using multi_subsystemsS = intern::interned_key_vec<subsystem_t, std::set<std::string>>;

}  // namespace resource_model
}  // namespace Flux

#endif  // DATA_STD_H

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

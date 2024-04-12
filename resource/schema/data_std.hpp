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

#include <set>
#include <string>
#include <boost/flyweight.hpp>

#include "resource/schema/data_std.hpp"

namespace Flux {
namespace resource_model {

// We create an x_checker planner for each resource vertex for quick exclusivity
// checking. We update x_checker for all of the vertices involved in each
// job allocation/reservation -- subtract 1 from x_checker planner for the
// scheduled span. Any vertex with less than X_CHECKER_NJOBS available in its
// x_checker cannot be exclusively allocated or reserved.
const char * const X_CHECKER_JOBS_STR = "jobs";
const int64_t X_CHECKER_NJOBS = 0x40000000;

extern boost::flyweight<std::string> flux_error;

// init_flyweight sets the string values for each flyweight defined below
// if you don't call this in the resource mod_main, you will get segfaults
// because it tries to atomically increment an atomic var that hasn't been
// allocated yet.
extern void init_flyweight();

// Resource types (node, slot, etc)
extern boost::flyweight<std::string> flux_node;
extern boost::flyweight<std::string> flux_slot;

// Subsystems
extern boost::flyweight<std::string> flux_match_any;
extern boost::flyweight<std::string> flux_subsystem_containment;
extern boost::flyweight<std::string> flux_subsystem_power;
extern boost::flyweight<std::string> flux_subsystem_infiniband_network;
extern boost::flyweight<std::string> flux_subsystem_infiniband_bandwidth;
extern boost::flyweight<std::string> flux_subsystem_parallel_filesystem_bandwidth;
extern boost::flyweight<std::string> flux_subsystem_virtual;
extern boost::flyweight<std::string> flux_subsystem_network;
extern boost::flyweight<std::string> flux_subsystem_storage;


// Relations
extern boost::flyweight<std::string> flux_relation_contains;
extern boost::flyweight<std::string> flux_relation_in;

using subsystem_t = boost::flyweight<std::string>;
using multi_subsystems_t = std::map<subsystem_t, boost::flyweight<std::string>>;
using multi_subsystemsS = std::map<subsystem_t, std::set<boost::flyweight<std::string>>>;

} // Flux
} // Flux::resource_model

#endif // DATA_STD_H

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

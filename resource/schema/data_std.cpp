/*****************************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#include <set>
#include <string>
#include <boost/flyweight.hpp>

namespace Flux {
namespace resource_model {


boost::flyweight<std::string> flux_error;
boost::flyweight<std::string> flux_match_any;
boost::flyweight<std::string> flux_node;
boost::flyweight<std::string> flux_slot;
boost::flyweight<std::string> flux_core;
boost::flyweight<std::string> flux_subsystem_containment;
boost::flyweight<std::string> flux_subsystem_power;
boost::flyweight<std::string> flux_subsystem_infiniband_network;
boost::flyweight<std::string> flux_subsystem_infiniband_bandwidth;
boost::flyweight<std::string> flux_subsystem_parallel_filesystem_bandwidth;
boost::flyweight<std::string> flux_subsystem_virtual;
boost::flyweight<std::string> flux_subsystem_network;
boost::flyweight<std::string> flux_subsystem_storage;
boost::flyweight<std::string> flux_relation_contains;
boost::flyweight<std::string> flux_relation_in;;


void init_flyweight ()
{    
    flux_error = "error";
    flux_match_any = "*";
    flux_node = "node";
    flux_slot = "slot";
    flux_core = "core";
    flux_subsystem_containment = "containment";
    flux_subsystem_power = "power";
    flux_subsystem_infiniband_network = "ibnet";
    flux_subsystem_infiniband_bandwidth ="ibnetbw";
    flux_subsystem_parallel_filesystem_bandwidth = "pfs1bw";
    flux_subsystem_virtual = "virtual1";
    flux_subsystem_network = "network";
    flux_subsystem_storage = "storage";
    flux_relation_contains = "contains";
    flux_relation_in = "in";
}


} // Flux
} // Flux::resource_model

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

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

#ifndef COMMAND_HPP
#define COMMAND_HPP

#include "resource/schema/resource_graph.hpp"
#include "resource/generators/gen.hpp"
#include "resource/traversers/dfu.hpp"
#include "resource/jobinfo/jobinfo.hpp"
#include <cerrno>
#include <vector>
#include <map>

namespace Flux {
namespace resource_model {

struct test_params_t {
    std::string grug;           /* GRUG file name */
    std::string hwloc_xml;      /* hwloc XML file name */
    std::string matcher_name;   /* Matcher name */
    std::string matcher_policy; /* Matcher policy name */
    std::string o_fname;        /* Output file to dump the filtered graph */
    std::ofstream r_out;        /* Output file stream for emitted R */
    std::string r_fname;        /* Output file to dump the emitted R */
    std::string o_fext;         /* File extension */
    std::string prune_filters;   /* Raw prune-filter specification */
    emit_format_t o_format;
    bool elapse_time;           /* Print elapse time */
    bool flux_hwloc;            /* get hwloc info from flux instance */
};

struct resource_context_t {
    test_params_t params;        /* Parameters for resource-query */
    uint64_t jobid_counter;      /* Hold the current jobid value */
    resource_graph_db_t db;      /* Resource graph data store */
    dfu_match_cb_t *matcher;     /* Match callback object */
    dfu_traverser_t traverser;   /* Graph traverser object */
    std::map<uint64_t, job_info_t *> jobs;     /* Jobs table */
    std::map<uint64_t, uint64_t> allocations;  /* Allocation table */
    std::map<uint64_t, uint64_t> reservations; /* Reservation table */
    std::map<std::string, f_resource_graph_t *> resource_graph_views;
};

typedef int cmd_func_f (resource_context_t *, std::vector<std::string> &);

cmd_func_f *find_cmd (const std::string &cmd_str);
int cmd_match (resource_context_t *ctx, std::vector<std::string> &args);
int cmd_cancel (resource_context_t *ctx, std::vector<std::string> &args);
int cmd_list (resource_context_t *ctx, std::vector<std::string> &args);
int cmd_info (resource_context_t *ctx, std::vector<std::string> &args);
int cmd_cat (resource_context_t *ctx, std::vector<std::string> &args);
int cmd_quit (resource_context_t *ctx, std::vector<std::string> &args);
int cmd_help (resource_context_t *ctx, std::vector<std::string> &args);

} // namespace resource_model
} // namespace Flux

#endif // COMMAND_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

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
#include "resource/store/resource_graph_store.hpp"
#include "resource/readers/resource_reader_factory.hpp"
#include "resource/traversers/dfu.hpp"
#include "resource/jobinfo/jobinfo.hpp"
#include <memory>
#include <cerrno>
#include <vector>
#include <map>

namespace Flux {
namespace resource_model {

struct test_params_t {
    std::string load_file;      /* load file name */
    std::string load_format;    /* load reader format */
    std::string load_allowlist; /* load resource allowlist */
    std::string matcher_name;   /* Matcher name */
    std::string matcher_policy; /* Matcher policy name */
    std::string o_fname;        /* Output file to dump the filtered graph */
    std::ofstream r_out;        /* Output file stream for emitted R */
    std::string r_fname;        /* Output file to dump the emitted R */
    std::string o_fext;         /* File extension */
    std::string prune_filters;  /* Raw prune-filter specification */
    std::string match_format;   /* Format to emit a matched resources */
    emit_format_t o_format;
    bool elapse_time;           /* Print elapse time */
    bool disable_prompt;        /* Disable resource-query> prompt */
    bool flux_hwloc;            /* get hwloc info from flux instance */
    size_t reserve_vtx_vec;     /* Allow for reserving vertex vector size */
};

struct match_perf_t {
    double min;                 /* Min match time */
    double max;                 /* Max match time */
    double accum;               /* Total match time accumulated */
};

struct resource_context_t {
    test_params_t params;        /* Parameters for resource-query */
    uint64_t jobid_counter;      /* Hold the current jobid value */
    std::shared_ptr<dfu_match_cb_t> matcher; /* Match callback object */
    std::shared_ptr<dfu_traverser_t> traverser; /* Graph traverser object */
    std::shared_ptr<resource_graph_db_t> db;    /* Resource graph data store */
    std::shared_ptr<f_resource_graph_t> fgraph; /* Filtered graph */
    std::shared_ptr<match_writers_t> writers;  /* Vertex/Edge writers */
    match_perf_t perf;           /* Match performance stats */
    std::map<uint64_t, std::shared_ptr<job_info_t>> jobs; /* Jobs table */
    std::map<uint64_t, uint64_t> allocations;  /* Allocation table */
    std::map<uint64_t, uint64_t> reservations; /* Reservation table */
};

typedef int cmd_func_f (std::shared_ptr<resource_context_t> &,
                        std::vector<std::string> &);

cmd_func_f *find_cmd (const std::string &cmd_str);
int cmd_match (std::shared_ptr<resource_context_t> &ctx,
               std::vector<std::string> &args);
int cmd_match_multi (std::shared_ptr<resource_context_t> &ctx,
                     std::vector<std::string> &args);
int cmd_update (std::shared_ptr<resource_context_t> &ctx,
                std::vector<std::string> &args);
int cmd_attach (std::shared_ptr<resource_context_t> &ctx,
                std::vector<std::string> &args);
int cmd_find (std::shared_ptr<resource_context_t> &ctx,
                std::vector<std::string> &args);
int cmd_cancel (std::shared_ptr<resource_context_t> &ctx,
                std::vector<std::string> &args);
int cmd_set_property (std::shared_ptr<resource_context_t> &ctx,
                      std::vector<std::string> &args);
int cmd_get_property (std::shared_ptr<resource_context_t> &ctx,
                      std::vector<std::string> &args);
int cmd_set_status (std::shared_ptr<resource_context_t> &ctx,
                      std::vector<std::string> &args);
int cmd_get_status (std::shared_ptr<resource_context_t> &ctx,
                      std::vector<std::string> &args);
int cmd_list (std::shared_ptr<resource_context_t> &ctx,
              std::vector<std::string> &args);
int cmd_info (std::shared_ptr<resource_context_t> &ctx,
              std::vector<std::string> &args);
int cmd_stat (std::shared_ptr<resource_context_t> &ctx,
              std::vector<std::string> &args);
int cmd_cat (std::shared_ptr<resource_context_t> &ctx,
             std::vector<std::string> &args);
int cmd_quit (std::shared_ptr<resource_context_t> &ctx,
              std::vector<std::string> &args);
int cmd_help (std::shared_ptr<resource_context_t> &ctx,
              std::vector<std::string> &args);
double get_elapse_time (timeval &st, timeval &et);

} // namespace resource_model
} // namespace Flux

#endif // COMMAND_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

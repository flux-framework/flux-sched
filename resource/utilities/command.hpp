/*****************************************************************************\
 * Copyright 2014 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef COMMAND_HPP
#define COMMAND_HPP

#include "resource/schema/resource_graph.hpp"
#include "resource/store/resource_graph_store.hpp"
#include "resource/traversers/dfu.hpp"
#include "resource/jobinfo/jobinfo.hpp"
#include "resource/reapi/bindings/c++/reapi_cli.hpp"
#include <memory>
#include <cerrno>
#include <fstream>
#include <vector>
#include <map>

namespace Flux {
namespace resource_model {

struct test_params_t {
    std::string load_file;        /* load file name */
    std::string load_format;      /* load reader format */
    std::string load_allowlist;   /* load resource allowlist */
    std::string matcher_name;     /* Matcher name */
    std::string matcher_policy;   /* Matcher policy name */
    std::string traverser_policy; /* Traverser policy name */
    std::string o_fname;          /* Output file to dump the filtered graph */
    std::ofstream r_out;          /* Output file stream for emitted R */
    std::string r_fname;          /* Output file to dump the emitted R */
    std::string o_fext;           /* File extension */
    std::string prune_filters;    /* Raw prune-filter specification */
    std::string match_format;     /* Format to emit a matched resources */
    emit_format_t o_format;
    bool elapse_time;       /* Print elapse time */
    bool disable_prompt;    /* Disable resource-query> prompt */
    bool flux_hwloc;        /* get hwloc info from flux instance */
    size_t reserve_vtx_vec; /* Allow for reserving vertex vector size */
};

struct match_perf_t {
    double min;   /* Min match time */
    double max;   /* Max match time */
    double accum; /* Total match time accumulated */
};

typedef int cmd_func_f (std::shared_ptr<detail::resource_query_t> &,
                        std::vector<std::string> &,
                        std::ostream &);

cmd_func_f *find_cmd (const std::string &cmd_str);
int cmd_match (std::shared_ptr<detail::resource_query_t> &ctx,
               std::vector<std::string> &args,
               std::ostream &out);
int cmd_match_multi (std::shared_ptr<detail::resource_query_t> &ctx,
                     std::vector<std::string> &args,
                     std::ostream &out);
int cmd_update (std::shared_ptr<detail::resource_query_t> &ctx,
                std::vector<std::string> &args,
                std::ostream &out);
int cmd_attach (std::shared_ptr<detail::resource_query_t> &ctx,
                std::vector<std::string> &args,
                std::ostream &out);
int cmd_remove (std::shared_ptr<detail::resource_query_t> &ctx,
                std::vector<std::string> &args,
                std::ostream &out);
int cmd_find (std::shared_ptr<detail::resource_query_t> &ctx,
              std::vector<std::string> &args,
              std::ostream &out);
int cmd_cancel (std::shared_ptr<detail::resource_query_t> &ctx,
                std::vector<std::string> &args,
                std::ostream &out);
int cmd_partial_cancel (std::shared_ptr<detail::resource_query_t> &ctx,
                        std::vector<std::string> &args,
                        std::ostream &out);
int cmd_set_property (std::shared_ptr<detail::resource_query_t> &ctx,
                      std::vector<std::string> &args,
                      std::ostream &out);
int cmd_get_property (std::shared_ptr<detail::resource_query_t> &ctx,
                      std::vector<std::string> &args,
                      std::ostream &out);
int cmd_set_status (std::shared_ptr<detail::resource_query_t> &ctx,
                    std::vector<std::string> &args,
                    std::ostream &out);
int cmd_get_status (std::shared_ptr<detail::resource_query_t> &ctx,
                    std::vector<std::string> &args,
                    std::ostream &out);
int cmd_list (std::shared_ptr<detail::resource_query_t> &ctx,
              std::vector<std::string> &args,
              std::ostream &out);
int cmd_info (std::shared_ptr<detail::resource_query_t> &ctx,
              std::vector<std::string> &args,
              std::ostream &out);
int cmd_stat (std::shared_ptr<detail::resource_query_t> &ctx,
              std::vector<std::string> &args,
              std::ostream &out);
int cmd_cat (std::shared_ptr<detail::resource_query_t> &ctx,
             std::vector<std::string> &args,
             std::ostream &out);
int cmd_quit (std::shared_ptr<detail::resource_query_t> &ctx,
              std::vector<std::string> &args,
              std::ostream &out);
int cmd_help (std::shared_ptr<detail::resource_query_t> &ctx,
              std::vector<std::string> &args,
              std::ostream &out);
double get_elapse_time (timeval &st, timeval &et);

}  // namespace resource_model
}  // namespace Flux

#endif  // COMMAND_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

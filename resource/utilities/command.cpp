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

#include <sys/time.h>
#include "command.hpp"

extern "C" {
#if HAVE_CONFIG_H
#include "config.h"
#endif
}

namespace Flux {
namespace resource_model {

using namespace Flux::Jobspec;

struct command_t {
    std::string name;
    std::string abbr;
    cmd_func_f *cmd;
    std::string note;
};

command_t commands[] = {
    { "match",  "m", cmd_match, "Allocate or reserve matching resources (subcmd:"
"allocate | allocate_with_satisfiability | allocate_orelse_reserve): "
"resource-query> match allocate jobspec"},
    { "cancel", "c", cmd_cancel, "Cancel an allocation or reservation: "
"resource-query> cancel jobid" },
    { "set-property", "p", cmd_set_property, "Add a property to a resource: "
"resource-query> set-property resource PROPERTY=VALUE" },
{ "get-property", "g", cmd_get_property, "Get all properties of a resource: "
"resource-query> get-property resource" },
    { "list", "l", cmd_list, "List all jobs: resource-query> list" },
    { "info", "i", cmd_info,
"Print info on a jobid: resource-query> info jobid" },
    { "stat", "s", cmd_stat,
 "Print overall stats: resource-query> stat jobid" },
    { "cat", "a", cmd_cat, "Print jobspec file: resource-query> cat jobspec" },
    { "help", "h", cmd_help, "Print help message: resource-query> help" },
    { "quit", "q", cmd_quit, "Quit the session: resource-query> quit" },
    { "NA", "NA", (cmd_func_f *)NULL, "NA" }
};

static int do_remove (std::shared_ptr<resource_context_t> &ctx, int64_t jobid)
{
    int rc = -1;
    if ((rc = ctx->traverser->remove ((int64_t)jobid)) == 0) {
        if (ctx->jobs.find (jobid) != ctx->jobs.end ()) {
           std::shared_ptr<job_info_t> info = ctx->jobs[jobid];
           info->state = job_lifecycle_t::CANCELLED;
        }
    } else {
        std::cout << ctx->traverser->err_message ();
        ctx->traverser->clear_err_message ();
    }
    return rc;
}

static void print_schedule_info (std::shared_ptr<resource_context_t> &ctx,
                                 std::ostream &out, uint64_t jobid,
                                 std::string &jobspec_fn, bool matched,
                                 int64_t at, double elapse, bool sat)
{
    if (matched) {
        job_lifecycle_t st;
        std::string mode = (at == 0)? "ALLOCATED" : "RESERVED";
        std::string scheduled_at = (at == 0)? "Now" : std::to_string (at);
        out << "INFO:" << " =============================" << std::endl;
        out << "INFO:" << " JOBID=" << jobid << std::endl;
        out << "INFO:" << " RESOURCES=" << mode << std::endl;
        out << "INFO:" << " SCHEDULED AT=" << scheduled_at << std::endl;
        if (ctx->params.elapse_time)
            std::cout << "INFO:" << " ELAPSE=" << std::to_string (elapse)
                      << std::endl;

        out << "INFO:" << " =============================" << std::endl;
        st = (at == 0)? job_lifecycle_t::ALLOCATED : job_lifecycle_t::RESERVED;
        ctx->jobs[jobid] = std::make_shared<job_info_t> (jobid, st, at,
                                                         jobspec_fn,
                                                         "", elapse);
        if (at == 0)
            ctx->allocations[jobid] = jobid;
        else
            ctx->reservations[jobid] = jobid;
    } else {
        out << "INFO:" << " =============================" << std::endl;
        out << "INFO: " << "No matching resources found" << std::endl;
        if (!sat)
            out << "INFO: " << "Unsatisfiable request" << std::endl;
        out << "INFO:" << " JOBID=" << jobid << std::endl;
        if (ctx->params.elapse_time)
            out << "INFO:" << " ELAPSE=" << std::to_string (elapse)
                << std::endl;
        out << "INFO:" << " =============================" << std::endl;
    }
    ctx->jobid_counter++;
}

static void update_match_perf (std::shared_ptr<resource_context_t> &ctx,
                               double elapse)
{
    ctx->perf.min = (ctx->perf.min > elapse)? elapse : ctx->perf.min;
    ctx->perf.max = (ctx->perf.max < elapse)? elapse : ctx->perf.max;
    ctx->perf.accum += elapse;
}

double get_elapse_time (timeval &st, timeval &et)
{
    double ts1 = (double)st.tv_sec + (double)st.tv_usec/1000000.0f;
    double ts2 = (double)et.tv_sec + (double)et.tv_usec/1000000.0f;
    return ts2 - ts1;
}

int cmd_match (std::shared_ptr<resource_context_t> &ctx,
               std::vector<std::string> &args)
{
    if (args.size () != 3) {
        std::cerr << "ERROR: malformed command" << std::endl;
        return 0;
    }
    std::string subcmd = args[1];
    if (!(subcmd == "allocate" || subcmd == "allocate_orelse_reserve"
          || subcmd == "allocate_with_satisfiability")) {
        std::cerr << "ERROR: unknown subcmd " << args[1] << std::endl;
        return 0;
    }

    try {
        int rc = 0;
        bool sat = true;
        int64_t at = 0;
        int64_t jobid = ctx->jobid_counter;
        std::string &jobspec_fn = args[2];
        std::ifstream jobspec_in (jobspec_fn);
        if (!jobspec_in) {
            std::cerr << "ERROR: can't open " << jobspec_fn << std::endl;
            return 0;
        }
        Flux::Jobspec::Jobspec job {jobspec_in};
        std::stringstream o;
        double elapse = 0.0f;
        struct timeval st, et;

        gettimeofday (&st, NULL);
        ctx->writers->reset ();

        if (args[1] == "allocate")
            rc = ctx->traverser->run (job, ctx->writers, match_op_t::
                                      MATCH_ALLOCATE, (int64_t)jobid, &at);
        else if (args[1] == "allocate_with_satisfiability")
            rc = ctx->traverser->run (job, ctx->writers, match_op_t::
                                      MATCH_ALLOCATE_W_SATISFIABILITY,
                                      (int64_t)jobid, &at);
        else if (args[1] == "allocate_orelse_reserve")
            rc = ctx->traverser->run (job, ctx->writers, match_op_t::
                                      MATCH_ALLOCATE_ORELSE_RESERVE,
                                      (int64_t)jobid, &at);

        if ((rc != 0) && (errno == ENODEV))
            sat = false;

        ctx->writers->emit (o);
        std::ostream &out = (ctx->params.r_fname != "")? ctx->params.r_out
                                                       : std::cout;
        out << o.str ();

        gettimeofday (&et, NULL);
        elapse = get_elapse_time (st, et);
        update_match_perf (ctx, elapse);

        print_schedule_info (ctx, out, jobid,
                             jobspec_fn, rc == 0, at, elapse, sat);
        jobspec_in.close ();

    } catch (parse_error &e) {
        std::cerr << "ERROR: Jobspec error for " << ctx->jobid_counter <<": "
                  << e.what () << std::endl;
    }
    return 0;
}

int cmd_cancel (std::shared_ptr<resource_context_t> &ctx,
                std::vector<std::string> &args)
{
    if (args.size () != 2) {
        std::cerr << "ERROR: malformed command" << std::endl;
        return 0;
    }

    int rc = -1;
    std::string jobid_str = args[1];
    uint64_t jobid = (uint64_t)std::strtoll (jobid_str.c_str (), NULL, 10);

    if (ctx->allocations.find (jobid) != ctx->allocations.end ()) {
        if ( (rc = do_remove (ctx, jobid)) == 0)
            ctx->allocations.erase (jobid);
    } else if (ctx->reservations.find (jobid) != ctx->reservations.end ()) {
        if ( (rc = do_remove (ctx, jobid)) == 0)
            ctx->reservations.erase (jobid);
    } else {
        std::cerr << "ERROR: nonexistent job " << jobid << std::endl;
        goto done;
    }

    if (rc != 0) {
        std::cerr << "ERROR: error encountered while removing job "
                  << jobid << std::endl;
    }

done:
    return 0;
}

int cmd_set_property (std::shared_ptr<resource_context_t> &ctx,
                      std::vector<std::string> &args)
{
    if (args.size () != 3) {
        std::cerr << "ERROR: malformed command" << std::endl;
        return 0;
    }

    std::string resource_path = args[1];
    std::string property_key, property_value;
    std::ostream &out = (ctx->params.r_fname != "") ? ctx->params.r_out
                                                    : std::cout;
    size_t pos = args[2].find ('=');

    if (pos == 0 || (pos == args[2].size () - 1) || pos == std::string::npos) {
        out << "Incorrect input format. " << std::endl
            << "Please use `set-property <resource> PROPERTY=VALUE`."
            << std::endl;
        return 0;
    }
    else {
        property_key = args[2].substr (0, pos);
        property_value = args[2].substr (pos + 1);
    }

    std::map<std::string, vtx_t>::const_iterator it =
        ctx->db->metadata.by_path.find (resource_path);

    if (it == ctx->db->metadata.by_path.end ()) {
        out << "Couldn't find path " << resource_path
            << " in resource graph." << std::endl;
    }
    else {
        vtx_t v = it->second;

        /* Note that map.insert () does not insert if the key exists.
         * Assuming we want to update the value though, we do an erase
         * before we insert. */

        if (ctx->db->resource_graph[v].properties.find (property_key)
             != ctx->db->resource_graph[v].properties.end ()) {
            ctx->db->resource_graph[v].properties.erase (property_key);
        }
        ctx->db->resource_graph[v].properties.insert (
            std::pair<std::string, std::string> (property_key,
                                                 property_value));
    }
    return 0;
}

int cmd_get_property (std::shared_ptr<resource_context_t> &ctx,
                      std::vector<std::string> &args)
{
    if (args.size () != 2) {
        std::cerr << "ERROR: malformed command" << std::endl;
        return 0;
    }

    std::string resource_path = args[1];
    std::ostream &out = (ctx->params.r_fname != "") ? ctx->params.r_out
                                                    : std::cout;

    std::map<std::string, vtx_t>::const_iterator it =
        ctx->db->metadata.by_path.find (resource_path);

    if (it == ctx->db->metadata.by_path.end ()) {
        out << "Could not find path " << resource_path
            << " in resource graph." << std::endl;
    }
    else {
        vtx_t v = it->second;
        if (ctx->db->resource_graph[v].properties.size () == 0) {
            out << "No properties were found for " << resource_path
                << ". " << std::endl;
        }
        else {
            std::map<std::string, std::string>::const_iterator p_it;
            for (p_it = ctx->db->resource_graph[v].properties.begin ();
                p_it != ctx->db->resource_graph[v].properties.end (); p_it++)
                    out << p_it->first << "=" << p_it->second << std::endl;
        }
    }
    return 0;
}

int cmd_list (std::shared_ptr<resource_context_t> &ctx,
              std::vector<std::string> &args)
{
    for (auto &kv: ctx->jobs) {
        std::shared_ptr<job_info_t> info = kv.second;
        std::string mode;
        get_jobstate_str (info->state, mode);
        std::cout << "INFO: " << info->jobid << ", " << mode << ", "
                  << info->scheduled_at << ", " << info->jobspec_fn << ", "
                  << info->overhead << std::endl;
    }
    return 0;
}

int cmd_info (std::shared_ptr<resource_context_t> &ctx,
              std::vector<std::string> &args)
{
    if (args.size () != 2) {
        std::cerr << "ERROR: malformed command" << std::endl;
        return 0;
    }
    uint64_t jobid = (uint64_t)std::atoll(args[1].c_str ());
    if (ctx->jobs.find (jobid) == ctx->jobs.end ()) {
       std::cout << "ERROR: jobid doesn't exist: " << args[1] << std::endl;
       return 0;
    }
    std::string mode;
    std::shared_ptr<job_info_t> info = ctx->jobs[jobid];
    get_jobstate_str (info->state, mode);
    std::cout << "INFO: " << info->jobid << ", " << mode << ", "
              << info->scheduled_at << ", " << info->jobspec_fn << ", "
              << info->overhead << std::endl;
    return 0;
}

int cmd_stat (std::shared_ptr<resource_context_t> &ctx,
              std::vector<std::string> &args)
{
    if (args.size () != 1) {
        std::cerr << "ERROR: malformed command" << std::endl;
        return 0;
    }
    double avg = 0.0f;
    double min = 0.0f;

    if (ctx->jobs.size ()) {
        avg = ctx->perf.accum / (double)ctx->jobs.size ();
        min = ctx->perf.min;
    }
    std::cout << "INFO: Num. of Jobs Matched: " << ctx->jobs.size ()
              << std::endl;
    std::cout << "INFO: Min. Match Time: " << min << std::endl;
    std::cout << "INFO: Max. Match Time: " << ctx->perf.max << std::endl;
    std::cout << "INFO: Avg. Match Time: " << avg << std::endl;
    return 0;
}

int cmd_cat (std::shared_ptr<resource_context_t> &ctx,
             std::vector<std::string> &args)
{
    std::string &jspec_filename = args[1];
    std::ifstream jspec_in;
    jspec_in.open (jspec_filename);
    std::string line;
    while (getline (jspec_in, line))
        std::cout << line << std::endl;
    std::cout << "INFO: " << "Jobspec in " << jspec_filename
              << std::endl;
    jspec_in.close ();
    return 0;
}

int cmd_help (std::shared_ptr<resource_context_t> &ctx,
              std::vector<std::string> &args)
{
    bool multi = true;
    bool found = false;
    std::string cmd = "unknown";

    if (args.size () == 2) {
        multi = false;
        cmd = args[1];
    }

    for (int i = 0; commands[i].name != "NA"; ++i) {
        if (multi || cmd == commands[i].name || cmd == commands[i].abbr) {
            std::cout << "INFO: " << commands[i].name << " ("
                      << commands[i].abbr << ")" << " -- "
                      << commands[i].note << std::endl;
            found = true;
        }
    }
    if (!multi && !found)
        std::cout << "ERROR: unknown command: " << cmd << std::endl;

    return 0;
}

int cmd_quit (std::shared_ptr<resource_context_t> &ctx,
              std::vector<std::string> &args)
{
    return -1;
}

cmd_func_f *find_cmd (const std::string &cmd_str)
{
    for (int i = 0; commands[i].name != "NA"; ++i) {
        if (cmd_str == commands[i].name)
            return commands[i].cmd;
        else if (cmd_str == commands[i].abbr)
            return commands[i].cmd;
    }
    return (cmd_func_f *)NULL;
}

} // namespace resource_model
} // namespace Flux

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

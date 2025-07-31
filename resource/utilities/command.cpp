/*****************************************************************************\
 * Copyright 2014 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

extern "C" {
#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <flux/idset.h>
}

#include <sys/time.h>
#include "command.hpp"
#include <readers/resource_reader_factory.hpp>

namespace Flux {
namespace resource_model {

using namespace Flux::Jobspec;

struct command_t {
    std::string name;
    std::string abbr;
    cmd_func_f *cmd;
    std::string note;
};

command_t commands[] =
    {{"match",
      "m",
      cmd_match,
      "Allocate or reserve matching resources (subcmd: "
      "allocate | allocate_with_satisfiability | allocate_orelse_reserve) | "
      "satisfiability: "
      "resource-query> match allocate jobspec"},
     {"multi-match",
      "M",
      cmd_match_multi,
      "Allocate or reserve for "
      "multiple jobspecs (subcmd: allocate | allocate_with_satisfiability | "
      "allocate_orelse_reserve): "
      "resource-query> multi-match allocate jobspec1 jobspec2 ..."},
     {"update",
      "u",
      cmd_update,
      "Update resources with a JGF subgraph (subcmd: "
      "allocate | reserve), (reader: jgf | rv1exec): "
      "resource-query> update allocate jgf jgf_file jobid starttime duration"},
     {"attach",
      "j",
      cmd_attach,
      "Attach a JGF subgraph to the "
      "resource graph: resource-query> attach jgf_file"},
     {"remove",
      "j",
      cmd_remove,
      "Remove a subgraph from the resource graph, specifying whether the target is "
      "a path (true) or idset (false): resource-query> remove (/path/to/node/ | idset) "
      "(true|false)"},
     {"find",
      "f",
      cmd_find,
      "Find resources matched with criteria "
      "(predicates: status={up|down} sched-now={allocated|free} sched-future={reserved|free} "
      "agfilter={true|false} names=hostlist jobid-alloc=jobid jobid-span=jobid jobid-tag=jobid "
      "jobid-reserved=jobid property=name): "
      "resource-query> find status=down and sched-now=allocated"},
     {"cancel",
      "c",
      cmd_cancel,
      "Cancel an allocation or reservation: "
      "resource-query> cancel jobid (optional subcmd: stats)"},
     {"partial-cancel",
      "pc",
      cmd_partial_cancel,
      "Partially release an allocation: "
      "resource-query> partial-cancel jobid (file format: jgf | rv1exec) R_to_cancel.file "
      "(optional subcmd: stats)"},
     {"set-property",
      "p",
      cmd_set_property,
      "Add a property to a resource: "
      "resource-query> set-property resource PROPERTY=VALUE"},
     {"get-property",
      "g",
      cmd_get_property,
      "Get all properties of a resource: "
      "resource-query> get-property resource"},
     {"set-status",
      "t",
      cmd_set_status,
      "Set resource status on vertex: "
      "resource-query> set-status PATH_TO_VERTEX {up|down}"},
     {"get-status",
      "e",
      cmd_get_status,
      "Get the graph resource vertex status: "
      "resource-query> get-status PATH_TO_VERTEX"},
     {"list", "l", cmd_list, "List all jobs: resource-query> list"},
     {"info", "i", cmd_info, "Print info on a jobid: resource-query> info jobid"},
     {"stat", "s", cmd_stat, "Print overall stats: resource-query> stat jobid"},
     {"cat", "a", cmd_cat, "Print jobspec file: resource-query> cat jobspec"},
     {"help", "h", cmd_help, "Print help message: resource-query> help"},
     {"quit", "q", cmd_quit, "Quit the session: resource-query> quit"},
     {"NA", "NA", (cmd_func_f *)NULL, "NA"}};

static int do_partial_remove (std::shared_ptr<detail::resource_query_t> &ctx,
                              std::shared_ptr<resource_reader_base_t> &reader,
                              int64_t jobid,
                              const std::string &R_cancel,
                              bool &full_cancel)
{
    int rc = -1;

    if ((rc = ctx->traverser->remove (R_cancel, reader, (int64_t)jobid, full_cancel)) == 0) {
        if (full_cancel && (ctx->jobs.find (jobid) != ctx->jobs.end ())) {
            std::shared_ptr<job_info_t> info = ctx->jobs[jobid];
            info->state = job_lifecycle_t::CANCELED;
        }
    } else {
        std::cout << ctx->traverser->err_message ();
    }
    return rc;
}

static void print_sat_info (std::shared_ptr<detail::resource_query_t> &ctx,
                            std::ostream &out,
                            bool sat,
                            double elapse)
{
    unsigned int pre = ctx->preorder_count ();
    unsigned int post = ctx->postorder_count ();

    std::string satstr = sat ? "Satisfiable" : "Unsatisfiable";
    out << "INFO:"
        << " =============================" << std::endl;
    out << "INFO: " << satstr << " request" << std::endl;
    out << "INFO:"
        << " =============================" << std::endl;
    if (ctx->params.elapse_time) {
        std::cout << "INFO:"
                  << " ELAPSE=" << std::to_string (elapse) << std::endl;
        std::cout << "INFO:"
                  << " PREORDER VISIT COUNT=" << pre << std::endl;
        std::cout << "INFO:"
                  << " POSTORDER VISIT COUNT=" << post << std::endl;
    }
}

static void print_schedule_info (std::shared_ptr<detail::resource_query_t> &ctx,
                                 std::ostream &out,
                                 uint64_t jobid,
                                 const std::string &jobspec_fn,
                                 bool matched,
                                 int64_t at,
                                 bool sat,
                                 double elapse)
{
    unsigned int pre = ctx->preorder_count ();
    unsigned int post = ctx->postorder_count ();

    if (matched) {
        job_lifecycle_t st;
        std::string mode = (at == 0) ? "ALLOCATED" : "RESERVED";
        std::string scheduled_at = (at == 0) ? "Now" : std::to_string (at);
        out << "INFO:"
            << " =============================" << std::endl;
        out << "INFO:"
            << " JOBID=" << jobid << std::endl;
        out << "INFO:"
            << " RESOURCES=" << mode << std::endl;
        out << "INFO:"
            << " SCHEDULED AT=" << scheduled_at << std::endl;
        if (ctx->params.elapse_time) {
            std::cout << "INFO:"
                      << " ELAPSE=" << std::to_string (elapse) << std::endl;
            std::cout << "INFO:"
                      << " PREORDER VISIT COUNT=" << pre << std::endl;
            std::cout << "INFO:"
                      << " POSTORDER VISIT COUNT=" << post << std::endl;
        }

        out << "INFO:"
            << " =============================" << std::endl;
        st = (at == 0) ? job_lifecycle_t::ALLOCATED : job_lifecycle_t::RESERVED;
        ctx->jobs[jobid] = std::make_shared<job_info_t> (jobid, st, at, jobspec_fn, "", elapse);
        if (at == 0)
            ctx->allocations[jobid] = jobid;
        else
            ctx->reservations[jobid] = jobid;
    } else {
        out << "INFO:"
            << " =============================" << std::endl;
        out << "INFO: "
            << "No matching resources found" << std::endl;
        if (!sat)
            out << "INFO: "
                << "Unsatisfiable request" << std::endl;
        out << "INFO:"
            << " JOBID=" << jobid << std::endl;
        if (ctx->params.elapse_time) {
            out << "INFO:"
                << " ELAPSE=" << std::to_string (elapse) << std::endl;
            std::cout << "INFO:"
                      << " PREORDER VISIT COUNT=" << pre << std::endl;
            std::cout << "INFO:"
                      << " POSTORDER VISIT COUNT=" << post << std::endl;
        }
        out << "INFO:"
            << " =============================" << std::endl;
    }
}

static void update_match_perf (std::shared_ptr<detail::resource_query_t> &ctx, double elapse)
{
    ctx->perf.min = (ctx->perf.min > elapse) ? elapse : ctx->perf.min;
    ctx->perf.max = (ctx->perf.max < elapse) ? elapse : ctx->perf.max;
    ctx->perf.accum += elapse;
}

double get_elapse_time (timeval &st, timeval &et)
{
    double ts1 = (double)st.tv_sec + (double)st.tv_usec / 1000000.0f;
    double ts2 = (double)et.tv_sec + (double)et.tv_usec / 1000000.0f;
    return ts2 - ts1;
}

static int run_match (std::shared_ptr<detail::resource_query_t> &ctx,
                      int64_t jobid,
                      const match_op_t match_op,
                      const std::string &jobspec_fn,
                      std::ostream &out)
{
    int rc = 0;
    int64_t at = 0;
    bool reserved = false;
    bool sat = true;
    bool matched = true;
    double ov = 0.0;
    std::string R = "";

    std::ifstream jobspec_in (jobspec_fn);
    if (!jobspec_in) {
        std::cerr << "ERROR: can't open " << jobspec_fn << std::endl;
        return 0;
    }
    std::string jobspec ((std::istreambuf_iterator<char> (jobspec_in)),
                         (std::istreambuf_iterator<char> ()));

    jobspec_in.close ();

    detail::reapi_cli_t::match_allocate (ctx.get (), match_op, jobspec, jobid, reserved, R, at, ov);

    // check for match success
    if ((errno == ENODEV) || (errno == EBUSY) || (errno == EINVAL) || (errno == ENOENT)) {
        matched = false;
        rc = -1;
    }

    // check for satisfiability
    if (errno == ENODEV || (errno == ENOENT))
        sat = false;

    if (detail::reapi_cli_t::get_err_message () != "") {
        std::cerr << detail::reapi_cli_t::get_err_message ();
        detail::reapi_cli_t::clear_err_message ();
    }

    out << R;

    if (match_op != match_op_t::MATCH_SATISFIABILITY)
        print_schedule_info (ctx, out, jobid, jobspec_fn, matched, at, sat, ov);
    else
        print_sat_info (ctx, out, sat, ov);

    return rc;
}

int cmd_match (std::shared_ptr<detail::resource_query_t> &ctx,
               std::vector<std::string> &args,
               std::ostream &out)
{
    match_op_t match_op;

    if (args.size () != 3) {
        std::cerr << "ERROR: malformed command" << std::endl;
        return 0;
    }
    std::string subcmd = args[1];
    if (subcmd == "allocate") {
        match_op = match_op_t::MATCH_ALLOCATE;
    } else if (subcmd == "allocate_orelse_reserve") {
        match_op = match_op_t::MATCH_ALLOCATE_ORELSE_RESERVE;
    } else if (subcmd == "allocate_with_satisfiability") {
        match_op = match_op_t::MATCH_ALLOCATE_W_SATISFIABILITY;
    } else if (subcmd == "satisfiability") {
        match_op = match_op_t::MATCH_SATISFIABILITY;
    } else {
        std::cerr << "ERROR: unknown subcmd " << args[1] << std::endl;
        return 0;
    }

    uint64_t jobid = ctx->get_job_counter ();
    std::string &jobspec_fn = args[2];

    run_match (ctx, jobid, match_op, jobspec_fn, out);

    return 0;
}

int cmd_match_multi (std::shared_ptr<detail::resource_query_t> &ctx,
                     std::vector<std::string> &args,
                     std::ostream &out)
{
    int rc = 0;
    size_t i;
    match_op_t match_op;

    if (args.size () <= 3) {
        std::cerr << "ERROR: malformed command" << std::endl;
        return 0;
    }
    std::string subcmd = args[1];
    if (subcmd == "allocate") {
        match_op = match_op_t::MATCH_ALLOCATE;
    } else if (subcmd == "allocate_orelse_reserve") {
        match_op = match_op_t::MATCH_ALLOCATE_ORELSE_RESERVE;
    } else if (subcmd == "allocate_with_satisfiability") {
        match_op = match_op_t::MATCH_ALLOCATE_W_SATISFIABILITY;
    } else if (subcmd == "satisfiability") {
        match_op = match_op_t::MATCH_SATISFIABILITY;
    } else {
        std::cerr << "ERROR: unknown subcmd " << args[1] << std::endl;
        return 0;
    }

    for (i = 2; i < args.size (); i++) {
        int64_t jobid = ctx->jobid_counter;
        std::string &jobspec_fn = args[i];
        rc = run_match (ctx, jobid, match_op, jobspec_fn, out);
        if (rc != 0)
            break;
    }
    return 0;
}

static int update_run (std::shared_ptr<detail::resource_query_t> &ctx,
                       const std::string &fn,
                       const std::string &str,
                       int64_t id,
                       int64_t at,
                       uint64_t d,
                       const std::string &reader,
                       std::ostream &out)
{
    int rc = -1;
    double elapse = 0.0f;
    std::stringstream o;
    struct timeval st, et;
    std::shared_ptr<resource_reader_base_t> rd;

    if (reader == "jgf") {
        if ((rd = create_resource_reader ("jgf")) == nullptr) {
            std::cerr << "ERROR: can't create JGF reader " << std::endl;
            return -1;
        }
    } else {
        if ((rd = create_resource_reader ("rv1exec")) == nullptr) {
            std::cerr << "ERROR: can't create rv1exec reader " << std::endl;
            return -1;
        }
    }

    gettimeofday (&st, NULL);

    {
        auto guard = resource_type_t::storage_t::open_for_scope ();
        if ((rc = ctx->traverser->run (str, ctx->writers, rd, id, at, d)) != 0) {
            std::cerr << "ERROR: traverser run () returned error " << std::endl;
            if (ctx->traverser->err_message () != "") {
                std::cerr << "ERROR: " << ctx->traverser->err_message ();
            }
        }
    }

    ctx->writers->emit (o);
    out << o.str ();
    gettimeofday (&et, NULL);

    elapse = get_elapse_time (st, et);
    update_match_perf (ctx, elapse);
    ctx->jobid_counter = id;
    print_schedule_info (ctx, out, id, fn, rc == 0, at, true, elapse);
    ctx->jobid_counter++;

    return 0;
}

static int update (std::shared_ptr<detail::resource_query_t> &ctx,
                   std::vector<std::string> &args,
                   std::ostream &out)
{
    uint64_t d = 0;
    int64_t at = 0;
    int64_t jobid = 0;
    std::string subcmd = args[1];
    std::string reader = args[2];
    std::stringstream buffer{};

    if (!(subcmd == "allocate" || subcmd == "reserve")) {
        std::cerr << "ERROR: unknown subcmd " << args[1] << std::endl;
        return -1;
    }
    if (!(reader == "jgf" || reader == "rv1exec")) {
        std::cerr << "ERROR: unsupported reader " << args[2] << std::endl;
        return -1;
    }
    std::ifstream jgf_file (args[3]);
    if (!jgf_file) {
        std::cerr << "ERROR: can't open " << args[3] << std::endl;
        return -1;
    }

    jobid = static_cast<int64_t> (std::strtoll (args[4].c_str (), NULL, 10));
    if (ctx->allocations.find (jobid) != ctx->allocations.end ()
        || ctx->reservations.find (jobid) != ctx->reservations.end ()) {
        std::cerr << "ERROR: existing Jobid " << std::endl;
        return -1;
    }
    at = static_cast<int64_t> (std::strtoll (args[5].c_str (), NULL, 10));
    d = static_cast<int64_t> (std::strtoll (args[6].c_str (), NULL, 10));
    if (at < 0 || d == 0) {
        std::cerr << "ERROR: invalid time (" << at << ", " << d << ")" << std::endl;
        return -1;
    }

    buffer << jgf_file.rdbuf ();
    jgf_file.close ();

    return update_run (ctx, args[3], buffer.str (), jobid, at, d, reader, out);
}

int cmd_update (std::shared_ptr<detail::resource_query_t> &ctx,
                std::vector<std::string> &args,
                std::ostream &out)
{
    try {
        if (args.size () != 7) {
            std::cerr << "ERROR: malformed command" << std::endl;
            return 0;
        }
        update (ctx, args, out);

    } catch (std::ifstream::failure &e) {
        std::cerr << "ERROR: file I/O exception: " << e.what () << std::endl;
    } catch (std::out_of_range &e) {
        std::cerr << "ERROR: " << e.what () << std::endl;
    }
    return 0;
}

static int attach (std::shared_ptr<detail::resource_query_t> &ctx,
                   std::vector<std::string> &args,
                   std::ostream &out)
{
    std::stringstream buffer{};
    std::shared_ptr<resource_reader_base_t> rd;

    std::ifstream jgf_file (args[1]);
    if (!jgf_file) {
        std::cerr << "ERROR: can't open " << args[1] << std::endl;
        return -1;
    }
    buffer << jgf_file.rdbuf ();
    jgf_file.close ();

    if ((rd = create_resource_reader ("jgf")) == nullptr) {
        std::cerr << "ERROR: can't create JGF reader " << std::endl;
        return -1;
    }

    // Unpack_at currently does not use the vertex attachment point.
    // This functionality is currently experimental.
    vtx_t v = boost::graph_traits<resource_graph_t>::null_vertex ();
    if ((rd->unpack_at (ctx->db->resource_graph, ctx->db->metadata, v, buffer.str (), -1)) != 0) {
        std::cerr << "ERROR: can't attach JGF subgraph " << std::endl;
        std::cerr << "ERROR: " << rd->err_message ();
        return -1;
    }
    if (ctx->traverser->initialize (ctx->db, ctx->matcher) != 0) {
        std::cerr << "ERROR: can't reinitialize traverser after attach" << std::endl;
        return -1;
    }

    return 0;
}

static int remove (std::shared_ptr<detail::resource_query_t> &ctx,
                   std::vector<std::string> &args,

                   std::ostream &out)
{
    const std::string target = args[1];
    const std::string is_path = args[2];
    bool path = false;
    struct idset *r_ids = nullptr;
    int64_t rank = -1;
    std::set<int64_t> ranks;
    std::shared_ptr<resource_reader_base_t> rd;

    if (is_path == "true") {
        path = true;
        if ((ctx->traverser->remove_subgraph (target)) != 0) {
            std::cerr << "ERROR: can't remove subgraph " << std::endl;
            std::cerr << "ERROR: " << ctx->traverser->err_message ();
            return -1;
        }
    } else if (is_path == "false") {
        if ((r_ids = idset_decode (target.c_str ())) == NULL) {
            std::cerr << "ERROR: failed to decode ranks.\n";
            return -1;
        }
        rank = idset_first (r_ids);
        while (rank != IDSET_INVALID_ID) {
            ranks.insert (rank);
            rank = idset_next (r_ids, rank);
        }

        if ((ctx->traverser->remove (ranks)) != 0) {
            std::cerr << "ERROR: can't partial cancel subgraph " << std::endl;
            std::cerr << "ERROR: " << ctx->traverser->err_message ();
            return -1;
        }
        if ((ctx->traverser->remove_subgraph (ranks)) != 0) {
            std::cerr << "ERROR: can't remove subgraph " << std::endl;
            std::cerr << "ERROR: " << ctx->traverser->err_message ();
            return -1;
        }
    } else {
        std::cerr << "ERROR: invalid path boolean input " << std::endl;
        return -1;
    }

    ctx->traverser->initialize ();

    return 0;
}

int cmd_attach (std::shared_ptr<detail::resource_query_t> &ctx,
                std::vector<std::string> &args,
                std::ostream &out)
{
    try {
        if (args.size () != 2) {
            std::cerr << "ERROR: malformed command" << std::endl;
            return 0;
        }
        attach (ctx, args, out);

    } catch (std::ifstream::failure &e) {
        std::cerr << "ERROR: file I/O exception: " << e.what () << std::endl;
    } catch (std::out_of_range &e) {
        std::cerr << "ERROR: " << e.what () << std::endl;
    }
    return 0;
}

int cmd_remove (std::shared_ptr<detail::resource_query_t> &ctx,
                std::vector<std::string> &args,
                std::ostream &out)
{
    try {
        if (args.size () != 3) {
            std::cerr << "ERROR: malformed command" << std::endl;
            return 0;
        }
        remove (ctx, args, out);

    } catch (std::ifstream::failure &e) {
        std::cerr << "ERROR: file I/O exception: " << e.what () << std::endl;
    } catch (std::out_of_range &e) {
        std::cerr << "ERROR: " << e.what () << std::endl;
    }
    return 0;
}

int cmd_find (std::shared_ptr<detail::resource_query_t> &ctx,
              std::vector<std::string> &args,
              std::ostream &out)
{
    int rc = -1;
    int i = 0;
    json_t *o = nullptr;
    char *json_str = nullptr;

    if (args.size () < 2) {
        std::cerr << "ERROR: malformed command: " << std::endl;
        return 0;
    }
    std::string criteria = args[1];
    for (int i = 2; i < static_cast<int> (args.size ()); ++i)
        criteria += " " + args[i];
    if ((rc = detail::reapi_cli_t::find (ctx.get (), criteria, o)) < 0) {
        if (detail::reapi_cli_t::get_err_message () != "") {
            std::cerr << detail::reapi_cli_t::get_err_message ();
            detail::reapi_cli_t::clear_err_message ();
        }
        goto done;
    }
    if (o) {
        if (json_is_string (o)) {
            out << json_string_value (o);
        } else if (!(json_str = json_dumps (o, JSON_INDENT (0)))) {
            json_decref (o);
            o = NULL;
            errno = ENOMEM;
            goto done;
        } else if (json_str) {
            out << json_str << std::endl;
            free (json_str);
        }
        json_decref (o);
    }
    out << "INFO:"
        << " =============================" << std::endl;
    out << "INFO:"
        << " EXPRESSION=\"" << criteria << "\"" << std::endl;
    out << "INFO:"
        << " =============================" << std::endl;

done:
    return 0;
}

int cmd_cancel (std::shared_ptr<detail::resource_query_t> &ctx,
                std::vector<std::string> &args,
                std::ostream &out)
{
    if (args.size () < 2 || args.size () > 3) {
        std::cerr << "ERROR: malformed command" << std::endl;
        return 0;
    }

    int rc = -1;
    std::string jobid_str = args[1];
    uint64_t jobid = (uint64_t)std::strtoll (jobid_str.c_str (), NULL, 10);
    std::string stats = "";
    unsigned int preorder_count = 0;
    unsigned int postorder_count = 0;

    if (args.size () == 3) {
        stats = args[2];
    }

    if (detail::reapi_cli_t::cancel (ctx.get (), jobid, false) != 0) {
        std::cerr << detail::reapi_cli_t::get_err_message ();
        detail::reapi_cli_t::clear_err_message ();
        return 0;
    }

    if (stats == "stats") {
        preorder_count = detail::reapi_cli_t::preorder_count (ctx.get ());
        postorder_count = detail::reapi_cli_t::postorder_count (ctx.get ());
        out << "INFO:"
            << " =============================" << std::endl;
        out << "INFO:"
            << " CANCEL PREORDER COUNT=\"" << preorder_count << "\"" << std::endl;
        out << "INFO:"
            << " CANCEL POSTORDER COUNT=\"" << postorder_count << "\"" << std::endl;
        out << "INFO:"
            << " =============================" << std::endl;
    }

done:
    return 0;
}

int cmd_partial_cancel (std::shared_ptr<detail::resource_query_t> &ctx,
                        std::vector<std::string> &args,
                        std::ostream &out)
{
    int rc = -1;
    std::stringstream buffer{};
    std::shared_ptr<resource_reader_base_t> rd;

    if (args.size () < 4 || args.size () > 5) {
        std::cerr << "ERROR: malformed command" << std::endl;
        return 0;
    }

    std::string jobid_str = args[1];
    std::string reader = args[2];
    std::ifstream cancel_file (args[3]);
    uint64_t jobid = (uint64_t)std::strtoll (jobid_str.c_str (), NULL, 10);
    bool full_cancel = false;
    std::string stats = "";
    unsigned int preorder_count = 0;
    unsigned int postorder_count = 0;

    if (args.size () == 5) {
        stats = args[4];
    }

    if (!(reader == "jgf" || reader == "rv1exec")) {
        std::cerr << "ERROR: unsupported reader " << args[2] << std::endl;
        goto done;
    }

    if (!cancel_file) {
        std::cerr << "ERROR: can't open " << args[3] << std::endl;
        goto done;
    }
    buffer << cancel_file.rdbuf ();
    cancel_file.close ();

    if (reader == "rv1exec") {
        if ((rd = create_resource_reader ("rv1exec")) == nullptr) {
            std::cerr << "ERROR: can't create rv1exec reader " << std::endl;
            goto done;
        }
    } else {  // must be JGF
        if ((rd = create_resource_reader ("jgf")) == nullptr) {
            std::cerr << "ERROR: can't create rv1exec reader " << std::endl;
            goto done;
        }
    }

    if (ctx->allocations.find (jobid) != ctx->allocations.end ()) {
        if ((rc = do_partial_remove (ctx, rd, jobid, buffer.str (), full_cancel)) == 0) {
            if (full_cancel)
                ctx->allocations.erase (jobid);
        }
    } else if (ctx->reservations.find (jobid) != ctx->reservations.end ()) {
        std::cerr << "ERROR: reservations not currently supported by partial cancel" << std::endl;
        goto done;
    } else {
        std::cerr << "ERROR: nonexistent job " << jobid << std::endl;
        goto done;
    }

    if (rc != 0) {
        std::cerr << "ERROR: error encountered while removing job " << jobid << std::endl;
    }

    if (stats == "stats") {
        preorder_count = ctx->traverser->get_total_preorder_count ();
        postorder_count = ctx->traverser->get_total_postorder_count ();
        out << "INFO:"
            << " =============================" << std::endl;
        out << "INFO:"
            << " PARTIAL CANCEL PREORDER COUNT=\"" << preorder_count << "\"" << std::endl;
        out << "INFO:"
            << " PARTIAL CANCEL POSTORDER COUNT=\"" << postorder_count << "\"" << std::endl;
        out << "INFO:"
            << " =============================" << std::endl;
    }

done:
    return 0;
}

int cmd_set_property (std::shared_ptr<detail::resource_query_t> &ctx,
                      std::vector<std::string> &args,
                      std::ostream &out)
{
    if (args.size () != 3) {
        std::cerr << "ERROR: malformed command" << std::endl;
        return 0;
    }

    std::string resource_path = args[1];
    std::string property_key, property_value;
    size_t pos = args[2].find ('=');

    if (pos == 0 || (pos == args[2].size () - 1) || pos == std::string::npos) {
        out << "Incorrect input format. " << std::endl
            << "Please use `set-property <resource> PROPERTY=VALUE`." << std::endl;
        return 0;
    } else {
        property_key = args[2].substr (0, pos);
        property_value = args[2].substr (pos + 1);
    }

    std::map<std::string, std::vector<vtx_t>>::const_iterator it =
        ctx->db->metadata.by_path.find (resource_path);

    if (it == ctx->db->metadata.by_path.end ()) {
        out << "Couldn't find path " << resource_path << " in resource graph." << std::endl;
    } else {
        for (auto &v : it->second) {
            /* Note that map.insert () does not insert if the key exists.
             * Assuming we want to update the value though, we do an erase
             * before we insert. */

            if (ctx->db->resource_graph[v].properties.find (property_key)
                != ctx->db->resource_graph[v].properties.end ()) {
                ctx->db->resource_graph[v].properties.erase (property_key);
            }
            ctx->db->resource_graph[v].properties.insert (
                std::pair<std::string, std::string> (property_key, property_value));
        }
    }
    return 0;
}

int cmd_get_property (std::shared_ptr<detail::resource_query_t> &ctx,
                      std::vector<std::string> &args,
                      std::ostream &out)
{
    if (args.size () != 2) {
        std::cerr << "ERROR: malformed command" << std::endl;
        return 0;
    }

    std::string resource_path = args[1];

    std::map<std::string, std::vector<vtx_t>>::const_iterator it =
        ctx->db->metadata.by_path.find (resource_path);

    if (it == ctx->db->metadata.by_path.end ()) {
        out << "Could not find path " << resource_path << " in resource graph." << std::endl;
    } else {
        for (auto &v : it->second) {
            if (ctx->db->resource_graph[v].properties.size () == 0) {
                out << "No properties were found for " << resource_path
                    << " (vtx's uniq_id=" << ctx->db->resource_graph[v].uniq_id << ")."
                    << std::endl;
            } else {
                std::map<std::string, std::string>::const_iterator p_it;
                for (p_it = ctx->db->resource_graph[v].properties.begin ();
                     p_it != ctx->db->resource_graph[v].properties.end ();
                     p_it++)
                    out << p_it->first << "=" << p_it->second << std::endl;
            }
        }
    }
    return 0;
}

int cmd_set_status (std::shared_ptr<detail::resource_query_t> &ctx,
                    std::vector<std::string> &args,
                    std::ostream &out)
{
    if (args.size () != 3) {
        std::cerr << "ERROR: malformed command" << std::endl;
        return 0;
    }
    std::string vtx_path = args[1];
    std::string status = args[2];
    std::map<std::string, std::vector<vtx_t>>::const_iterator it =
        ctx->db->metadata.by_path.find (vtx_path);
    resource_pool_t::string_to_status sts = resource_pool_t::str_to_status;

    if (it == ctx->db->metadata.by_path.end ()) {
        out << "Could not find path " << vtx_path << " in resource graph." << std::endl;
        return 0;
    }

    auto status_it = sts.find (status);
    if (status_it == sts.end ()) {
        std::cerr << "ERROR: unrecognized status" << std::endl;
        return 0;
    }

    return ctx->traverser->mark (vtx_path, status_it->second);
}

int cmd_get_status (std::shared_ptr<detail::resource_query_t> &ctx,
                    std::vector<std::string> &args,
                    std::ostream &out)
{
    if (args.size () != 2) {
        std::cerr << "ERROR: malformed command" << std::endl;
        return 0;
    }
    std::string vtx_path = args[1];
    std::map<std::string, std::vector<vtx_t>>::const_iterator it =
        ctx->db->metadata.by_path.find (vtx_path);
    resource_pool_t::string_to_status sts = resource_pool_t::str_to_status;
    std::string status = "";

    if (it == ctx->db->metadata.by_path.end ()) {
        out << "Could not find path " << vtx_path << " in resource graph." << std::endl;
        return 0;
    }

    for (auto &v : it->second) {
        for (auto &status_it : sts) {
            if (status_it.second == ctx->db->resource_graph[v].status) {
                status = status_it.first;
                break;
            }
        }

        if (status == "") {
            std::cerr << "ERROR: vertex " << vtx_path
                      << "(vtx's uniq_id=" << ctx->db->resource_graph[v].uniq_id
                      << ") has unknown status." << std::endl;
            return 0;
        }

        out << vtx_path << " is " << status << std::endl;
    }

    return 0;
}

int cmd_list (std::shared_ptr<detail::resource_query_t> &ctx,
              std::vector<std::string> &args,
              std::ostream &out)
{
    for (auto &kv : ctx->jobs) {
        std::shared_ptr<job_info_t> info = kv.second;
        std::string mode;
        get_jobstate_str (info->state, mode);
        std::cout << "INFO: " << info->jobid << ", " << mode << ", " << info->scheduled_at << ", "
                  << info->jobspec_fn << ", " << info->overhead << std::endl;
    }
    return 0;
}

int cmd_info (std::shared_ptr<detail::resource_query_t> &ctx,
              std::vector<std::string> &args,
              std::ostream &out)
{
    if (args.size () != 2) {
        std::cerr << "ERROR: malformed command" << std::endl;
        return 0;
    }
    uint64_t jobid = (uint64_t)std::atoll (args[1].c_str ());
    if (ctx->jobs.find (jobid) == ctx->jobs.end ()) {
        std::cout << "ERROR: jobid doesn't exist: " << args[1] << std::endl;
        return 0;
    }
    std::string mode;
    std::shared_ptr<job_info_t> info = ctx->jobs[jobid];
    get_jobstate_str (info->state, mode);
    std::cout << "INFO: " << info->jobid << ", " << mode << ", " << info->scheduled_at << ", "
              << info->jobspec_fn << ", " << info->overhead << std::endl;
    return 0;
}

int cmd_stat (std::shared_ptr<detail::resource_query_t> &ctx,
              std::vector<std::string> &args,
              std::ostream &out)
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
    std::cout << "INFO: Num. of Jobs Matched: " << ctx->jobs.size () << std::endl;
    std::cout << "INFO: Min. Match Time: " << min << std::endl;
    std::cout << "INFO: Max. Match Time: " << ctx->perf.max << std::endl;
    std::cout << "INFO: Avg. Match Time: " << avg << std::endl;
    return 0;
}

int cmd_cat (std::shared_ptr<detail::resource_query_t> &ctx,
             std::vector<std::string> &args,
             std::ostream &out)
{
    std::string &jspec_filename = args[1];
    std::ifstream jspec_in;
    jspec_in.open (jspec_filename);
    std::string line;
    while (getline (jspec_in, line))
        std::cout << line << std::endl;
    std::cout << "INFO: "
              << "Jobspec in " << jspec_filename << std::endl;
    jspec_in.close ();
    return 0;
}

int cmd_help (std::shared_ptr<detail::resource_query_t> &ctx,
              std::vector<std::string> &args,
              std::ostream &out)
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
            std::cout << "INFO: " << commands[i].name << " (" << commands[i].abbr << ")"
                      << " -- " << commands[i].note << std::endl;
            found = true;
        }
    }
    if (!multi && !found)
        std::cout << "ERROR: unknown command: " << cmd << std::endl;

    return 0;
}

int cmd_quit (std::shared_ptr<detail::resource_query_t> &ctx,
              std::vector<std::string> &args,
              std::ostream &out)
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

}  // namespace resource_model
}  // namespace Flux

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

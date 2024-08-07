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
}

#include <cstdint>
#include <cstdlib>
#include <getopt.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iterator>
#include <limits>
#include <memory>
#include <editline/readline.h>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include "resource/utilities/command.hpp"
#include "resource/store/resource_graph_store.hpp"
#include "resource/policies/dfu_match_policy_factory.hpp"

namespace fs = boost::filesystem;
using namespace Flux::resource_model;
using boost::tie;

#define OPTIONS "L:f:W:S:P:F:g:o:p:t:r:edh"
static const struct option longopts[] = {
    {"load-file", required_argument, 0, 'L'},
    {"load-format", required_argument, 0, 'f'},
    {"load-allowlist", required_argument, 0, 'W'},
    {"match-subsystems", required_argument, 0, 'S'},
    {"match-policy", required_argument, 0, 'P'},
    {"match-format", required_argument, 0, 'F'},
    {"graph-format", required_argument, 0, 'g'},
    {"graph-output", required_argument, 0, 'o'},
    {"prune-filters", required_argument, 0, 'p'},
    {"test-output", required_argument, 0, 't'},
    {"reserve-vtx-vec", required_argument, 0, 'r'},
    {"elapse-time", no_argument, 0, 'e'},
    {"disable-prompt", no_argument, 0, 'd'},
    {"help", no_argument, 0, 'h'},
    {0, 0, 0, 0},
};

static void usage (int code)
{
    std::cerr << R"(
usage: resource-query [OPTIONS...]

Command-line utility that takes in an HPC resource request written in
Flux's Canonical Job Specification (or simply a jobspec) (RFC 14) and
selects the best-matching compute and other resources in accordance
with a selection policy.

Read in a resource-graph generation recipe written in the GRUG format
and populate the resource-graph data store representing the compute and
other HPC resources and their relationships (RFC 4).

Provide a simple command-line interface (cli) to allow users to allocate
or reserve the resource set in this resource-graph data store
using a jobspec as an input.
Traverse the resource graph in a predefined order for resource selection.
Currently only support one traversal type: depth-first traversal on the
dominant subsystem and up-walk traversal on one or more auxiliary
subsystems.

OPTIONS allow for using a predefined matcher that is configured
to use a different set of subsystems as its dominant and/or auxiliary
ones to perform the matches on.

OPTIONS also allow for instantiating a different resource-matching
selection policy--e.g., select resources with high or low IDs first.

OPTIONS allow for exporting the filtered graph of the used matcher
in a selected graph format at the end of the cli session.

To see cli commands, type in "help" in the cli: i.e.,
  % resource-query> help

OPTIONS:
    -h, --help
            Display this usage information

    -L, --load-file=filepath
            Input file from which to load the resource graph data store
            (default=conf/default)

    -f, --load-format=<grug|hwloc|jgf|rv1exec>
            Format of the load file (default=grug)

    -W, --load-allowlist=<resource1[,resource2[,resource3...]]>
            Allowlist of resource types to be loaded
            Resources that are not included in this list will be filtered out

    -S, --match-subsystems="
         "<CA|IBA|IBBA|PFS1BA|PA|C+IBA|C+PFS1BA|C+PA|IB+IBBA|"
              "C+P+IBA|VA|V+PFS1BA|ALL>
            Set the predefined matcher to use. Available matchers are:
                CA: Containment Aware
                IBA: InfiniBand connection-Aware
                IBBA: InfiniBand Bandwidth-Aware
                PFS1BA: Parallel File System 1 Bandwidth-aware
                PA: Power-Aware
                C+IBA: Containment- and InfiniBand connection-Aware
                C+PFS1BA: Containment- and PFS1 Bandwidth-Aware
                C+PA: Containment- and Power-Aware
                IB+IBBA: InfiniBand connection and Bandwidth-Aware
                C+P+IBA: Containment-, Power- and InfiniBand connection-Aware
                VA: Virtual Hierarchy-Aware
                V+PFS1BA: Virtual Hierarchy and PFS1 Bandwidth-Aware
                ALL: Aware of everything.
            (default=CA).

    -P, --match-policy=<low|high|lonode|hinode|lonodex|hinodex|first|firstnodex|locality|variation>
            Set the resource match selection policy. Available policies are:
                low: Select resources with low ID first
                high: Select resources with high ID first
                lonode: Select resources with lowest node ID first,
                        low ID first otherwise (e.g., node-local resource types)
                hinode: Select resources with highest node ID first,
                        high ID first otherwise (e.g., node-local resource types)
                lonodex: Same as lonode except each node is exclusively allocated
                hinodex: Same as hinode except each node is exclusively allocated
                first: Select the first matching resources and stop the search
                firstnodex: Select the first matching resources, node exclusive,
                        and stop the search
                locality: Select contiguous resources first in their ID space
                variation: Allocate resources based on performance classes.
                                (perf_class must be set using set-property).
                (default=first).

    -F, --match-format=<simple|pretty_simple|jgf|rlite|rv1|rv1_nosched>
            Specify the emit format of the matched resource set.
            (default=simple).

    -p, --prune-filters=<HL-resource1:LL-resource1[,HL-resource2:LL-resource2...]...]>
            Install a planner-based filter at each High-Level (HL) resource
                vertex which tracks the state of the Low-Level (LL) resources
                in aggregate, residing under its subtree. If a jobspec requests
                1 node with 4 cores, and the visiting compute-node vertex has
                only a total of 2 available cores in aggregate at its
                subtree, this filter allows the traverser to prune a further descent
                to accelerate the search.
                Use the ALL keyword for HL-resource if you want LL-resource to be
                tracked at all of the HL-resource vertices. Examples:
                    rack:node,node:core
                    ALL:core,cluster:node,rack:node
                (default=ALL:core).

    -g, --graph-format=<dot|graphml>
            Specify the graph format of the output file
            (default=dot).

    -r, --reserve-vtx-vec=<size>
            Reserve the graph vertex size to optimize resource graph loading.
            The size value must be a non-zero integer up to 2000000.

    -e, --elapse-time
            Print the elapse time per scheduling operation.

    -d, --disable-prompt
            Don't print the prompt.

    -o, --graph-output=<basename>
            Set the basename of the graph output file
            For AT&T Graphviz dot, <basename>.dot
            For GraphML, <basename>.graphml.

    -t, --test-output=<filename>
            Set the output filename where allocated or reserved resource
            information is stored into.

)";
    exit (code);
}

static void set_default_params (std::shared_ptr<resource_context_t> &ctx)
{
    ctx->params.load_file = "conf/default";
    ctx->params.load_format = "grug";
    ctx->params.load_allowlist = "";
    ctx->params.matcher_name = "CA";
    ctx->params.matcher_policy = "first";
    ctx->params.o_fname = "";
    ctx->params.r_fname = "";
    ctx->params.o_fext = "dot";
    ctx->params.match_format = "simple";
    ctx->params.o_format = emit_format_t::GRAPHVIZ_DOT;
    ctx->params.prune_filters = "ALL:core";
    ctx->params.reserve_vtx_vec = 0;
    ctx->params.elapse_time = false;
    ctx->params.disable_prompt = false;
}

static int string_to_graph_format (std::string st, emit_format_t &format)
{
    int rc = 0;
    if (boost::iequals (st, std::string ("dot")))
        format = emit_format_t::GRAPHVIZ_DOT;
    else if (boost::iequals (st, std::string ("graphml")))
        format = emit_format_t::GRAPH_ML;
    else
        rc = -1;
    return rc;
}

static int graph_format_to_ext (emit_format_t format, std::string &st)
{
    int rc = 0;
    switch (format) {
        case emit_format_t::GRAPHVIZ_DOT:
            st = "dot";
            break;
        case emit_format_t::GRAPH_ML:
            st = "graphml";
            break;
        default:
            rc = -1;
    }
    return rc;
}

static int subsystem_exist (std::shared_ptr<resource_context_t> &ctx, subsystem_t n)
{
    int rc = 0;
    if (ctx->db->metadata.roots.find (n) == ctx->db->metadata.roots.end ())
        rc = -1;
    return rc;
}

static int set_subsystems_use (std::shared_ptr<resource_context_t> &ctx, std::string n)
{
    int rc = 0;
    ctx->matcher->set_matcher_name (n);
    const std::string &matcher_type = ctx->matcher->matcher_name ();
    subsystem_t ibnet_sub{"ibnet"};
    subsystem_t pfs1bw_sub{"pfs1bw"};
    subsystem_t power_sub{"power"};
    subsystem_t ibnetbw_sub{"ibnetbw"};
    subsystem_t virtual1_sub{"virtual1"};
    std::map<std::string, std::vector<subsystem_t>>
        subsystem_map{{"CA", {containment_sub}},
                      {"IBA", {ibnet_sub}},
                      {"IBBA", {ibnetbw_sub}},
                      {"PA", {power_sub}},
                      {"PFS1BA", {pfs1bw_sub}},
                      {"C+IBA", {containment_sub, ibnet_sub}},
                      {"C+IBBA", {containment_sub, ibnetbw_sub}},
                      {"C+PA", {containment_sub, power_sub}},
                      {"C+PFS1BA", {containment_sub, pfs1bw_sub}},
                      {"IB+IBBA", {ibnet_sub, ibnetbw_sub}},
                      {"C+P+IBA", {containment_sub, power_sub, ibnet_sub}},
                      {"V+PFS1BA", {virtual1_sub, pfs1bw_sub}},
                      {"VA", {virtual1_sub}},
                      {"ALL", {containment_sub, ibnet_sub, ibnetbw_sub, pfs1bw_sub, power_sub}}};
    {
        // add lower case versions
        auto lower_case = subsystem_map;
        for (auto &[k, v] : lower_case) {
            std::string tmp = k;
            boost::algorithm::to_lower (tmp);
            subsystem_map.emplace (tmp, v);
        }
    }
    std::map<subsystem_t, std::string> subsys_to_edge_name = {
        {containment_sub, "contains"},
        {ibnet_sub, "connected_down"},
        {ibnetbw_sub, "*"},
        {pfs1bw_sub, "*"},
        {virtual1_sub, "*"},
        {power_sub, "supplies_to"},
    };

    for (auto &sub : subsystem_map.at (n)) {
        ctx->matcher->add_subsystem (sub, subsys_to_edge_name.at (sub));
    }

    return 0;
}

static void write_to_graphviz (resource_graph_t &fg, subsystem_t ss, std::fstream &o)
{
    f_res_name_map_t vmap = get (&resource_t::name, fg);
    f_edg_infra_map_t emap = get (&resource_relation_t::idata, fg);
    label_writer_t<f_res_name_map_t, vtx_t> vwr (vmap);
    edg_label_writer_t ewr (emap, ss);
    write_graphviz (o, fg, vwr, ewr);
}

static void flatten (resource_graph_t &fg,
                     std::map<vtx_t, std::string> &paths,
                     std::map<vtx_t, std::string> &subsystems,
                     std::map<edg_t, std::string> &esubsystems,
                     std::map<vtx_t, std::string> &properties)
{
    f_vtx_iterator_t vi, v_end;
    f_edg_iterator_t ei, e_end;

    for (boost::tie (vi, v_end) = vertices (fg); vi != v_end; ++vi) {
        paths[*vi] = "{";
        for (auto &kv : fg[*vi].paths) {
            if (paths[*vi].size () > 1)
                paths[*vi] += ",";
            paths[*vi] += kv.first + "=" + kv.second;
        }
        paths[*vi] += "}";
        subsystems[*vi] = "{";
        for (auto &kv : fg[*vi].idata.member_of) {
            if (subsystems[*vi].size () > 1)
                subsystems[*vi] += ",";
            subsystems[*vi] += kv.first + "=" + kv.second;
        }
        subsystems[*vi] += "}";
        properties[*vi] = "{";
        for (auto &kv : fg[*vi].properties) {
            if (properties[*vi].size () > 1)
                properties[*vi] += ",";
            properties[*vi] += kv.first + "=" + kv.second;
        }
        properties[*vi] += "}";
    }
    for (tie (ei, e_end) = edges (fg); ei != e_end; ++ei) {
        esubsystems[*ei] = "{";
        for (auto &kv : fg[*ei].idata.member_of) {
            if (esubsystems[*ei].size () > 0)
                esubsystems[*ei] += ",";
            esubsystems[*ei] += kv.first + "=" + kv.second;
        }
        esubsystems[*ei] += "}";
    }
}

static void write_to_graphml (resource_graph_t &fg, std::fstream &o)
{
    boost::dynamic_properties dp;
    std::map<edg_t, std::string> esubsystems;
    std::map<vtx_t, std::string> subsystems, properties, paths;
    boost::associative_property_map<std::map<vtx_t, std::string>> subsystems_map (subsystems);
    boost::associative_property_map<std::map<edg_t, std::string>> esubsystems_map (esubsystems);
    boost::associative_property_map<std::map<vtx_t, std::string>> props_map (properties);
    boost::associative_property_map<std::map<vtx_t, std::string>> paths_map (paths);

    flatten (fg, paths, subsystems, esubsystems, properties);

    // Resource pool vertices
    dp.property ("paths", paths_map);
    dp.property ("props", props_map);
    dp.property ("member_of", subsystems_map);
    dp.property ("type", get (&resource_pool_t::type, fg));
    dp.property ("basename", get (&resource_pool_t::basename, fg));
    dp.property ("name", get (&resource_pool_t::name, fg));
    dp.property ("id", get (&resource_pool_t::id, fg));
    dp.property ("uniq_id", get (&resource_pool_t::uniq_id, fg));
    dp.property ("size", get (&resource_pool_t::size, fg));
    dp.property ("unit", get (&resource_pool_t::unit, fg));
    // Relation edges
    dp.property ("member_of", esubsystems_map);

    write_graphml (o, fg, dp, true);
}

static void write_to_graph (std::shared_ptr<resource_context_t> &ctx)
{
    std::fstream o;
    std::string fn, mn;
    mn = ctx->matcher->matcher_name ();
    fn = ctx->params.o_fname + "." + ctx->params.o_fext;

    std::cout << "INFO: Write the target graph of the matcher..." << std::endl;
    o.open (fn, std::fstream::out);

    switch (ctx->params.o_format) {
        case emit_format_t::GRAPHVIZ_DOT:
            write_to_graphviz (ctx->db->resource_graph, ctx->matcher->dom_subsystem (), o);
            break;
        case emit_format_t::GRAPH_ML:
            write_to_graphml (ctx->db->resource_graph, o);
            break;
        default:
            std::cout << "ERROR: Unknown graph format" << std::endl;
            break;
    }
    if (o.bad ()) {
        std::cerr << "ERROR: Failure encountered in writing" << std::endl;
        o.clear ();
    }
    o.close ();
}

static void control_loop (std::shared_ptr<resource_context_t> &ctx)
{
    cmd_func_f *cmd = NULL;
    while (1) {
        char *line = ctx->params.disable_prompt ? readline ("") : readline ("resource-query> ");
        if (line == NULL)
            continue;
        else if (*line)
            add_history (line);

        std::vector<std::string> tokens;
        std::istringstream iss (line);
        std::copy (std::istream_iterator<std::string> (iss),
                   std::istream_iterator<std::string> (),
                   back_inserter (tokens));
        free (line);
        if (tokens.empty ())
            continue;

        std::string &cmd_str = tokens[0];
        if (!(cmd = find_cmd (cmd_str)))
            continue;
        if (cmd (ctx, tokens) != 0)
            break;
    }
}

static int populate_resource_db (std::shared_ptr<resource_context_t> &ctx)
{
    int rc = -1;
    double elapse;
    std::ifstream in_file;
    struct timeval st, et;
    std::stringstream buffer{};
    std::shared_ptr<resource_reader_base_t> rd;

    if (ctx->params.reserve_vtx_vec != 0)
        ctx->db->resource_graph.m_vertices.reserve (ctx->params.reserve_vtx_vec);
    if ((rd = create_resource_reader (ctx->params.load_format)) == nullptr) {
        std::cerr << "ERROR: Can't create load reader " << std::endl;
        goto done;
    }
    if (ctx->params.load_allowlist != "") {
        if (rd->set_allowlist (ctx->params.load_allowlist) < 0)
            std::cerr << "ERROR: Can't set allowlist" << std::endl;
        if (!rd->is_allowlist_supported ())
            std::cout << "WARN: allowlist unsupported" << std::endl;
    }

    in_file.open (ctx->params.load_file.c_str (), std::ifstream::in);
    if (!in_file.good ()) {
        std::cerr << "ERROR: Can't open " << ctx->params.load_file << std::endl;
        goto done;
    }
    buffer << in_file.rdbuf ();
    in_file.close ();

    gettimeofday (&st, NULL);
    if ((rc = ctx->db->load (buffer.str (), rd)) != 0) {
        std::cerr << "ERROR: " << rd->err_message () << std::endl;
        std::cerr << "ERROR: error in generating resources" << std::endl;
        goto done;
    }
    gettimeofday (&et, NULL);

    elapse = get_elapse_time (st, et);
    if (ctx->params.elapse_time) {
        resource_graph_t &g = ctx->db->resource_graph;
        std::cout << "INFO: Graph Load Time: " << elapse << std::endl;
        std::cout << "INFO: Vertex Count: " << num_vertices (g) << std::endl;
        std::cout << "INFO: Edge Count: " << num_edges (g) << std::endl;
        std::cout << "INFO: by_type Key-Value Pairs: " << ctx->db->metadata.by_type.size ()
                  << std::endl;
        std::cout << "INFO: by_name Key-Value Pairs: " << ctx->db->metadata.by_name.size ()
                  << std::endl;
        std::cout << "INFO: by_path Key-Value Pairs: " << ctx->db->metadata.by_path.size ()
                  << std::endl;
        for (auto it = ctx->db->metadata.by_rank.begin (); it != ctx->db->metadata.by_rank.end ();
             ++it) {
            std::cout << "INFO: number of vertices with rank " << it->first << ": "
                      << it->second.size () << "\n";
        }
    }

done:
    return rc;
}

static int init_resource_graph (std::shared_ptr<resource_context_t> &ctx)
{
    int rc = 0;

    if ((rc = populate_resource_db (ctx)) != 0) {
        std::cerr << "ERROR: can't populate graph resource database" << std::endl;
        return rc;
    }

    resource_graph_t &g = ctx->db->resource_graph;
    // Configure the matcher and its subsystem selector
    std::cout << "INFO: Loading a matcher: " << ctx->params.matcher_name << std::endl;
    if ((rc = set_subsystems_use (ctx, ctx->params.matcher_name)) != 0) {
        std::cerr << "ERROR: Not all subsystems found" << std::endl;
        return rc;
    }

    ctx->jobid_counter = 1;
    if (ctx->params.prune_filters != ""
        && ctx->matcher->set_pruning_types_w_spec (ctx->matcher->dom_subsystem (),
                                                   ctx->params.prune_filters)
               < 0) {
        std::cerr << "ERROR: setting pruning filters with ctx->params.prune_filters: "
                  << ctx->params.prune_filters << std::endl;
        return -1;
    }

    if (ctx->params.r_fname != "") {
        ctx->params.r_out.exceptions (std::ofstream::failbit | std::ofstream::badbit);
        ctx->params.r_out.open (ctx->params.r_fname);
    }

    try {
        ctx->traverser = std::make_shared<dfu_traverser_t> ();
    } catch (std::bad_alloc &e) {
        errno = ENOMEM;
        std::cerr << "ERROR: out of memory allocating traverser" << std::endl;
        return -1;
    }

    if ((rc = ctx->traverser->initialize (ctx->db, ctx->matcher)) != 0) {
        std::cerr << "ERROR: initializing traverser" << std::endl;
        return -1;
    }
    match_format_t format = match_writers_factory_t::get_writers_type (ctx->params.match_format);
    if (!(ctx->writers = match_writers_factory_t::create (format))) {
        std::cerr << "ERROR: out of memory allocating traverser" << std::endl;
        return -1;
    }

    return rc;
}

static void process_args (std::shared_ptr<resource_context_t> &ctx, int argc, char *argv[])
{
    int rc = 0;
    int ch = 0;
    std::string token;

    while ((ch = getopt_long (argc, argv, OPTIONS, longopts, NULL)) != -1) {
        switch (ch) {
            case 'h': /* --help */
                usage (0);
                break;
            case 'L': /* --load-file */
                ctx->params.load_file = optarg;
                if (!fs::exists (ctx->params.load_file)) {
                    std::cerr << "[ERROR] file does not exist for --load-file: ";
                    std::cerr << optarg << std::endl;
                    usage (1);
                } else if (fs::is_directory (ctx->params.load_file)) {
                    std::cerr << "[ERROR] path passed to --load-file is a directory: ";
                    std::cerr << optarg << std::endl;
                    usage (1);
                }
                break;
            case 'f': /* --load-format */
                ctx->params.load_format = optarg;
                if (!known_resource_reader (ctx->params.load_format)) {
                    std::cerr << "[ERROR] unknown format for --load-format: ";
                    std::cerr << optarg << std::endl;
                    usage (1);
                }
                break;
            case 'W': /* --hwloc-allowlist */
                token = optarg;
                if (token.find_first_not_of (' ') != std::string::npos) {
                    ctx->params.load_allowlist += "cluster,";
                    ctx->params.load_allowlist += token;
                }
                break;
            case 'S': /* --match-subsystems */
                ctx->params.matcher_name = optarg;
                break;
            case 'P': /* --match-policy */
                ctx->params.matcher_policy = optarg;
                break;
            case 'F': /* --match-format */
                ctx->params.match_format = optarg;
                if (!known_match_format (ctx->params.match_format)) {
                    std::cerr << "[ERROR] unknown format for --match-format: ";
                    std::cerr << optarg << std::endl;
                    usage (1);
                }
                break;
            case 'g': /* --graph-format */
                rc = string_to_graph_format (optarg, ctx->params.o_format);
                if (rc != 0) {
                    std::cerr << "[ERROR] unknown format for --graph-format: ";
                    std::cerr << optarg << std::endl;
                    usage (1);
                }
                graph_format_to_ext (ctx->params.o_format, ctx->params.o_fext);
                break;
            case 'o': /* --graph-output */
                ctx->params.o_fname = optarg;
                break;
            case 'p': /* --prune-filters */
                token = optarg;
                if (token.find_first_not_of (' ') != std::string::npos) {
                    ctx->params.prune_filters += ",";
                    ctx->params.prune_filters += token;
                }
                break;
            case 't': /* --test-output */
                ctx->params.r_fname = optarg;
                break;
            case 'r': /* --reserve-vtx-vec */
                // If atoi fails, it defaults to 0, which is fine for us
                ctx->params.reserve_vtx_vec = atoi (optarg);
                if ((ctx->params.reserve_vtx_vec < 0) || (ctx->params.reserve_vtx_vec > 2000000)) {
                    ctx->params.reserve_vtx_vec = 0;
                    std::cerr << "WARN: out of range specified for --reserve-vtx-vec: ";
                    std::cerr << optarg << std::endl;
                }
                break;
            case 'e': /* --elapse-time */
                ctx->params.elapse_time = true;
                break;
            case 'd': /* --disable-prompt */
                ctx->params.disable_prompt = true;
                break;
            default:
                usage (1);
                break;
        }
    }

    if (optind != argc)
        usage (1);
}

static std::shared_ptr<resource_context_t> init_resource_query (int c, char *v[])
{
    std::shared_ptr<resource_context_t> ctx = nullptr;

    try {
        ctx = std::make_shared<resource_context_t> ();
        ctx->db = std::make_shared<resource_graph_db_t> ();
    } catch (std::bad_alloc &e) {
        std::cerr << "ERROR: out of memory allocating resource context" << std::endl;
        errno = ENOMEM;
        goto done;
    }

    set_default_params (ctx);
    process_args (ctx, c, v);
    ctx->perf.min = std::numeric_limits<double>::max ();
    ctx->perf.max = 0.0f;
    ctx->perf.accum = 0.0f;
    if (!(ctx->matcher = create_match_cb (ctx->params.matcher_policy))) {
        std::cerr << "ERROR: unknown match policy " << std::endl;
        std::cerr << "ERROR: " << ctx->params.matcher_policy << std::endl;
        ctx = nullptr;
    }
    if (init_resource_graph (ctx) != 0) {
        std::cerr << "ERROR: resource graph initialization" << std::endl;
        ctx = nullptr;
    }

done:
    return ctx;
}

static void fini_resource_query (std::shared_ptr<resource_context_t> &ctx)
{
    if (ctx->params.r_fname != "")
        ctx->params.r_out.close ();
    if (ctx->params.o_fname != "")
        write_to_graph (ctx);
}

int main (int argc, char *argv[])
{
    std::shared_ptr<resource_context_t> ctx = nullptr;
    if (!(ctx = init_resource_query (argc, argv))) {
        std::cerr << "ERROR: resource query initialization" << std::endl;
        return EXIT_FAILURE;
    }

    control_loop (ctx);

    fini_resource_query (ctx);

    return EXIT_SUCCESS;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

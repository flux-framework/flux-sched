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

#include <cstdint>
#include <cstdlib>
#include <getopt.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iterator>
#include <cstdint>
#include <limits>
#include <readline/readline.h>
#include <readline/history.h>
#include <boost/algorithm/string.hpp>
#include "resource/utilities/command.hpp"
#include "resource/policies/dfu_match_high_id_first.hpp"
#include "resource/policies/dfu_match_low_id_first.hpp"
#include "resource/policies/dfu_match_locality.hpp"

extern "C" {
#if HAVE_CONFIG_H
#include "config.h"
#endif
}

using namespace std;
using namespace Flux::resource_model;

#define OPTIONS "S:P:F:G:X:W:g:o:p:t:r:edh"
static const struct option longopts[] = {
    {"match-subsystems", required_argument,  0, 'S'},
    {"match-policy",     required_argument,  0, 'P'},
    {"match-format",     required_argument,  0, 'F'},
    {"grug",             required_argument,  0, 'G'},
    {"hwloc-xml",        required_argument,  0, 'X'},
    {"hwloc-whitelist",  required_argument,  0, 'W'},
    {"graph-format",     required_argument,  0, 'g'},
    {"graph-output",     required_argument,  0, 'o'},
    {"prune-filters",    required_argument,  0, 'p'},
    {"test-output",      required_argument,  0, 't'},
    {"reserve-vtx-vec",  required_argument,  0, 'r'},
    {"elapse-time",      no_argument,        0, 'e'},
    {"disable-prompt",   no_argument,        0, 'd'},
    {"help",             no_argument,        0, 'h'},
    { 0, 0, 0, 0 },
};

static void usage (int code)
{
    cerr <<
"usage: resource-query [OPTIONS...]\n"
"\n"
"Command-line utility that takes in an HPC resource request written in\n"
"Flux's Canonical Job Specification (or simply a jobspec) (RFC 14) and\n"
"selects the best-matching compute and other resources in accordance\n"
"with a selection policy.\n"
"\n"
"Read in a resource-graph generation recipe written in the GRUG format\n"
"and populate the resource-graph data store representing the compute and\n"
"other HPC resources and their relationships (RFC 4).\n"
"\n"
"Provide a simple command-line interface (cli) to allow users to allocate\n"
"or reserve the resource set in this resource-graph data store \n"
"using a jobspec as an input.\n"
"Traverse the resource graph in a predefined order for resource selection.\n"
"Currently only support one traversal type: depth-first traversal on the\n"
"dominant subsystem and up-walk traversal on one or more auxiliary \n"
"subsystems.\n"
"\n"
"OPTIONS allow for using a predefined matcher that is configured\n"
"to use a different set of subsystems as its dominant and/or auxiliary\n"
"ones to perform the matches on.\n"
"\n"
"OPTIONS also allow for instantiating a different resource-matching\n"
"selection policy--e.g., select resources with high or low IDs first.\n"
"\n"
"OPTIONS allow for exporting the filtered graph of the used matcher\n"
"in a selected graph format at the end of the cli session.\n"
"\n"
"To see cli commands, type in \"help\" in the cli: i.e., \n"
"  % resource-query> help"
"\n"
"\n"
"\n"
"OPTIONS:\n"
"    -h, --help\n"
"            Display this usage information\n"
"\n"
"    -G, --grug=<genspec>.graphml\n"
"            GRUG resource graph generator specification file in graphml\n"
"\n"
"    -X, --hwloc-xml=<hwloc>.xml\n"
"            xml output from hwloc\n"
"\n"
"    -W, --hwloc-whitelist=<resource1[,resource2[,resource3...]]>\n"
"            Specify a whitelist of hwloc resource names to be used.\n"
"            Other hwloc resources will be filtered out.\n"
"\n"
"    -S, --match-subsystems="
         "<CA|IBA|IBBA|PFS1BA|PA|C+IBA|C+PFS1BA|C+PA|IB+IBBA|"
              "C+P+IBA|VA|V+PFS1BA|ALL>\n"
"            Set the predefined matcher to use. Available matchers are:\n"
"                CA: Containment Aware\n"
"                IBA: InfiniBand connection-Aware\n"
"                IBBA: InfiniBand Bandwidth-Aware\n"
"                PFS1BA: Parallel File System 1 Bandwidth-aware\n"
"                PA: Power-Aware\n"
"                C+IBA: Containment- and InfiniBand connection-Aware\n"
"                C+PFS1BA: Containment- and PFS1 Bandwidth-Aware\n"
"                C+PA: Containment- and Power-Aware\n"
"                IB+IBBA: InfiniBand connection and Bandwidth-Aware\n"
"                C+P+IBA: Containment-, Power- and InfiniBand connection-Aware\n"
"                VA: Virtual Hierarchy-Aware \n"
"                V+PFS1BA: Virtual Hierarchy and PFS1 Bandwidth-Aware \n"
"                ALL: Aware of everything.\n"
"            (default=CA).\n"
"\n"
"    -P, --match-policy=<low|high|locality>\n"
"            Set the resource match selection policy. Available policies are:\n"
"                high: Select resources with high ID first\n"
"                low: Select resources with low ID first\n"
"                locality: Select contiguous resources first in their ID space\n"
"            (default=high).\n"
"\n"
"    -F, --match-format=<simple|pretty_simple|jgf|rlite|rv1|rv1_nosched>\n"
"            Specify the emit format of the matched resource set.\n"
"            (default=simple).\n"
"\n"
"    -p, --prune-filters=<HL-resource1:LL-resource1[,HL-resource2:LL-resource2...]...]>\n"
"            Install a planner-based filter at each High-Level (HL) resource\n"
"                vertex which tracks the state of the Low-Level (LL) resources\n"
"                in aggregate, residing under its subtree. If a jobspec requests\n"
"                1 node with 4 cores, and the visiting compute-node vertex has\n"
"                only a total of 2 available cores in aggregate at its\n"
"                subtree, this filter allows the traverser to prune a further descent\n"
"                to accelerate the search.\n"
"                Use the ALL keyword for HL-resource if you want LL-resource to be\n"
"                tracked at all of the HL-resource vertices. Examples:\n"
"                    rack:node,node:core\n"
"                    ALL:core,cluster:node,rack:node\n"
"                (default=ALL:core).\n"
"\n"
"    -g, --graph-format=<dot|graphml>\n"
"            Specify the graph format of the output file\n"
"            (default=dot).\n"
"\n"
"    -r, --reserve-vtx-vec=<size>\n"
"            Reserve the graph vertex size to optimize resource graph loading.\n"
"            The size value must be a non-zero integer up to 2000000.\n"
"\n"
"    -e, --elapse-time\n"
"            Print the elapse time per scheduling operation.\n"
"\n"
"    -d, --disable-prompt\n"
"            Don't print the prompt.\n"
"\n"
"    -o, --graph-output=<basename>\n"
"            Set the basename of the graph output file\n"
"            For AT&T Graphviz dot, <basename>.dot\n"
"            For GraphML, <basename>.graphml.\n"
"\n"
"    -t, --test-output=<filename>\n"
"            Set the output filename where allocated or reserved resource\n"
"            information is stored into.\n"
"\n";
    exit (code);
}

static dfu_match_cb_t *create_match_cb (const string &policy)
{
    dfu_match_cb_t *matcher = NULL;
    if (policy == "high")
        matcher = (dfu_match_cb_t *)new high_first_t ();
    else if (policy == "low")
        matcher = (dfu_match_cb_t *)new low_first_t ();
    else if (policy == "locality")
        matcher = (dfu_match_cb_t *)new greater_interval_first_t ();
    return matcher;
}

static void set_default_params (resource_context_t *ctx)
{
    ctx->params.grug = "";
    ctx->params.hwloc_xml = "";
    ctx->params.hwloc_whitelist = "";
    ctx->params.matcher_name = "CA";
    ctx->params.matcher_policy = "high";
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

static int string_to_graph_format (string st, emit_format_t &format)
{
    int rc = 0;
    if (boost::iequals (st, string ("dot")))
        format = emit_format_t::GRAPHVIZ_DOT;
    else if (boost::iequals (st, string ("graphml")))
        format = emit_format_t::GRAPH_ML;
    else
        rc = -1;
    return rc;
}

static int graph_format_to_ext (emit_format_t format, string &st)
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

static int subsystem_exist (resource_context_t *ctx, string n)
{
    int rc = 0;
    if (ctx->db.roots.find (n) == ctx->db.roots.end ())
        rc = -1;
    return rc;
}

static int set_subsystems_use (resource_context_t *ctx, string n)
{
    int rc = 0;
    ctx->matcher->set_matcher_name (n);
    dfu_match_cb_t &matcher = *(ctx->matcher);
    const string &matcher_type = matcher.matcher_name ();

    if (boost::iequals (matcher_type, string ("CA"))) {
        if ( (rc = subsystem_exist (ctx, "containment")) == 0)
            matcher.add_subsystem ("containment", "*");
    } else if (boost::iequals (matcher_type, string ("IBA"))) {
        if ( (rc = subsystem_exist (ctx, "ibnet")) == 0)
            matcher.add_subsystem ("ibnet", "*");
    } else if (boost::iequals (matcher_type, string ("IBBA"))) {
        if ( (rc = subsystem_exist (ctx, "ibnetbw")) == 0)
            matcher.add_subsystem ("ibnetbw", "*");
    } else if (boost::iequals (matcher_type, string ("PFS1BA"))) {
        if ( (rc = subsystem_exist (ctx, "pfs1bw")) == 0)
            matcher.add_subsystem ("pfs1bw", "*");
    } else if (boost::iequals (matcher_type, string ("PA"))) {
        if ( (rc = subsystem_exist (ctx, "power")) == 0)
            matcher.add_subsystem ("power", "*");
    } else if (boost::iequals (matcher_type, string ("C+PFS1BA"))) {
        if ( (rc = subsystem_exist (ctx, "containment")) == 0)
            matcher.add_subsystem ("containment", "contains");
        if ( !rc && (rc = subsystem_exist (ctx, "pfs1bw")) == 0)
            matcher.add_subsystem ("pfs1bw", "*");
    } else if (boost::iequals (matcher_type, string ("C+IBA"))) {
        if ( (rc = subsystem_exist (ctx, "containment")) == 0)
            matcher.add_subsystem ("containment", "contains");
        if ( !rc && (rc = subsystem_exist (ctx, "ibnet")) == 0)
            matcher.add_subsystem ("ibnet", "connected_up");
    } else if (boost::iequals (matcher_type, string ("C+PA"))) {
        if ( (rc = subsystem_exist (ctx, "containment")) == 0)
            matcher.add_subsystem ("containment", "*");
        if ( !rc && (rc = subsystem_exist (ctx, "power")) == 0)
            matcher.add_subsystem ("power", "draws_from");
    } else if (boost::iequals (matcher_type, string ("IB+IBBA"))) {
        if ( (rc = subsystem_exist (ctx, "ibnet")) == 0)
            matcher.add_subsystem ("ibnet", "connected_down");
        if ( !rc && (rc = subsystem_exist (ctx, "ibnetbw")) == 0)
            matcher.add_subsystem ("ibnetbw", "*");
    } else if (boost::iequals (matcher_type, string ("C+P+IBA"))) {
        if ( (rc = subsystem_exist (ctx, "containment")) == 0)
            matcher.add_subsystem ("containment", "contains");
        if ( (rc = subsystem_exist (ctx, "power")) == 0)
            matcher.add_subsystem ("power", "draws_from");
        if ( !rc && (rc = subsystem_exist (ctx, "ibnet")) == 0)
            matcher.add_subsystem ("ibnet", "connected_up");
    } else if (boost::iequals (matcher_type, string ("V+PFS1BA"))) {
        if ( (rc = subsystem_exist (ctx, "virtual1")) == 0)
            matcher.add_subsystem ("virtual1", "*");
        if ( !rc && (rc = subsystem_exist (ctx, "pfs1bw")) == 0)
            matcher.add_subsystem ("pfs1bw", "*");
    } else if (boost::iequals (matcher_type, string ("VA"))) {
        if ( (rc = subsystem_exist (ctx, "virtual1")) == 0)
            matcher.add_subsystem ("virtual1", "*");
    } else if (boost::iequals (matcher_type, string ("ALL"))) {
        if ( (rc = subsystem_exist (ctx, "containment")) == 0)
            matcher.add_subsystem ("containment", "*");
        if ( !rc && (rc = subsystem_exist (ctx, "ibnet")) == 0)
            matcher.add_subsystem ("ibnet", "*");
        if ( !rc && (rc = subsystem_exist (ctx, "ibnetbw")) == 0)
            matcher.add_subsystem ("ibnetbw", "*");
        if ( !rc && (rc = subsystem_exist (ctx, "pfs1bw")) == 0)
            matcher.add_subsystem ("pfs1bw", "*");
        if ( (rc = subsystem_exist (ctx, "power")) == 0)
            matcher.add_subsystem ("power", "*");
    } else {
        rc = -1;
    }
    return rc;
}

static void write_to_graphviz (f_resource_graph_t &fg, subsystem_t ss,
                               fstream &o)
{
    f_res_name_map_t vmap = get (&resource_pool_t::name, fg);
    f_edg_infra_map_t emap = get (&resource_relation_t::idata, fg);
    label_writer_t<f_res_name_map_t, vtx_t> vwr (vmap);
    edg_label_writer_t ewr (emap, ss);
    write_graphviz (o, fg, vwr, ewr);
}

static void flatten (f_resource_graph_t &fg, map<vtx_t, string> &paths,
                     map<vtx_t, string> &subsystems,
                     map<edg_t, string> &esubsystems)
{
    f_vtx_iterator_t vi, v_end;
    f_edg_iterator_t ei, e_end;

    for (tie (vi, v_end) = vertices (fg); vi != v_end; ++vi) {
        paths[*vi] = "{";
        for (auto &kv : fg[*vi].paths) {
            paths[*vi] += kv.first + ": \"" + kv.second + "\"";
        }
        paths[*vi] += "}";
        subsystems[*vi] = "{";
        for (auto &kv : fg[*vi].idata.member_of) {
            subsystems[*vi] += kv.first + ": \"" + kv.second + "\"";
        }
        subsystems[*vi] += "}";
    }
    for (tie (ei, e_end) = edges (fg); ei != e_end; ++ei) {
        esubsystems[*ei] = "{";
        for (auto &kv : fg[*ei].idata.member_of) {
            esubsystems[*ei] += kv.first + ": \"" + kv.second + "\"";
        }
        esubsystems[*ei] += "}";
    }
}

static void write_to_graphml (f_resource_graph_t &fg, fstream &o)
{
    boost::dynamic_properties dp;
    map<edg_t, string> esubsystems;
    map<vtx_t, string> subsystems, properties, paths;
    boost::associative_property_map<map<vtx_t, string>> subsystems_map (subsystems);
    boost::associative_property_map<map<edg_t, string>> esubsystems_map (esubsystems);
    boost::associative_property_map<map<vtx_t, string>> props_map (properties);
    boost::associative_property_map<map<vtx_t, string>> paths_map (paths);

    flatten (fg, paths, subsystems, esubsystems);

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

static void write_to_graph (resource_context_t *ctx)
{
    fstream o;
    string fn, mn;
    mn = ctx->matcher->matcher_name ();
    fn = ctx->params.o_fname + "." + ctx->params.o_fext;

    cout << "INFO: Write the target graph of the matcher..." << endl;
    o.open (fn, fstream::out);

    switch (ctx->params.o_format) {
    case emit_format_t::GRAPHVIZ_DOT:
        write_to_graphviz (*(ctx->fgraph), ctx->matcher->dom_subsystem (), o);
        break;
    case emit_format_t::GRAPH_ML:
        write_to_graphml (*(ctx->fgraph), o);
        break;
    default:
        cout << "ERROR: Unknown graph format" << endl;
        break;
    }
    if (o.bad ()) {
        cerr << "ERROR: Failure encountered in writing" << endl;
        o.clear ();
    }
    o.close ();
}

static void destory_resource_ctx (resource_context_t *ctx)
{
    delete ctx->matcher;
    delete ctx->traverser;
    delete ctx->fgraph;
    delete ctx->writers;
    for (auto &kv : ctx->jobs)
        delete kv.second;    /* job_info_t* type */
    ctx->jobs.clear ();
    ctx->allocations.clear ();
    ctx->reservations.clear ();
    delete ctx;
}

static void control_loop (resource_context_t *ctx)
{
    cmd_func_f *cmd = NULL;
    while (1) {
        char *line = ctx->params.disable_prompt? readline ("")
                                               : readline ("resource-query> ");
        if (line == NULL)
            continue;
        else if(*line)
            add_history (line);

        vector<string> tokens;
        istringstream iss (line);
        copy(istream_iterator<string>(iss), istream_iterator<string>(),
             back_inserter (tokens));
        free(line);
        if (tokens.empty ())
            continue;

        string &cmd_str = tokens[0];
        if (!(cmd = find_cmd (cmd_str)))
            continue;
        if (cmd (ctx, tokens) != 0)
            break;
    }
}

static int populate_resource_db (resource_context_t *ctx)
{
    int rc = 0;
    struct timeval st, et;
    double elapse;
    resource_generator_t rgen;

    if (ctx->params.reserve_vtx_vec != 0)
        ctx->db.resource_graph.m_vertices.reserve (ctx->params.reserve_vtx_vec);
    if (ctx->params.grug == "" && ctx->params.hwloc_xml == "")
        ctx->params.grug = "conf/default";

    gettimeofday (&st, NULL);

    if (ctx->params.grug != "") {
        if (ctx->params.hwloc_xml != "") {
            cout << "WARN: multiple resource inputs provided, using grug" << endl;
        }
        if ( (rc = rgen.read_graphml (ctx->params.grug, ctx->db)) != 0) {
            cerr << "ERROR: " << rgen.err_message () << endl;
            cerr << "ERROR: error in generating resources" << endl;
            rc = -1;
        }
    } else if (ctx->params.hwloc_xml != "") {
        if ( (rc = rgen.set_hwloc_whitelist (
                       ctx->params.hwloc_whitelist)) < 0) {
            cerr << "ERROR: error in setting hwloc whitelist." << endl;
            rc = -1;
        }
        if ( (rc = rgen.read_hwloc_xml_file (ctx->params.hwloc_xml.c_str(),
                                             ctx->db)) != 0) {
            cerr << "ERROR: " << rgen.err_message () << endl;
            cerr << "ERROR: error in generating resources" << endl;
            rc = -1;
        }
    }

    gettimeofday (&et, NULL);
    elapse = get_elapse_time (st, et);
    if (ctx->params.elapse_time) {
        cout << "INFO: Graph Load Time: " << elapse
             << endl;
        cout << "INFO: Vertex Count: " << num_vertices (ctx->db.resource_graph)
             << endl;
        cout << "INFO: Edge Count: " << num_edges (ctx->db.resource_graph)
             << endl;
    }

    return rc;
}

static int init_resource_graph (resource_context_t *ctx)
{
    int rc = 0;

    if ( (rc = populate_resource_db (ctx)) != 0) {
        cerr << "ERROR: can't populate graph resource database" << endl;
        return rc;
    }

    resource_graph_t &g = ctx->db.resource_graph;
    // Configure the matcher and its subsystem selector
    cout << "INFO: Loading a matcher: " << ctx->params.matcher_name << endl;
    if ( (rc = set_subsystems_use (ctx, ctx->params.matcher_name)) != 0) {
        cerr << "ERROR: Not all subsystems found" << endl;
        return rc;
    }

    vtx_infra_map_t vmap = get (&resource_pool_t::idata, g);
    edg_infra_map_t emap = get (&resource_relation_t::idata, g);
    const multi_subsystemsS &filter = ctx->matcher->subsystemsS ();
    subsystem_selector_t<vtx_t, f_vtx_infra_map_t> vtxsel (vmap, filter);
    subsystem_selector_t<edg_t, f_edg_infra_map_t> edgsel (emap, filter);

    if (!(ctx->fgraph = new (nothrow)f_resource_graph_t (g, edgsel, vtxsel))) {
        cerr << "ERROR: out of memory allocating f_resource_graph_t" << endl;
        return -1;
    }
    ctx->jobid_counter = 1;
    if (ctx->params.prune_filters != ""
        && ctx->matcher->set_pruning_types_w_spec (ctx->matcher->dom_subsystem (),
                                                   ctx->params.prune_filters)
                                                   < 0) {
        cerr << "ERROR: setting pruning filters with ctx->params.prune_filters: "
             << ctx->params.prune_filters << endl;
        return -1;
    }

    if (ctx->params.r_fname != "") {
        ctx->params.r_out.exceptions (std::ofstream::failbit
                                          | std::ofstream::badbit);
        ctx->params.r_out.open (ctx->params.r_fname);
    }
    if ( !(ctx->traverser = new (nothrow)dfu_traverser_t ())) {
        cerr << "ERROR: out of memory allocating traverser" << endl;
        return -1;
    }
    if ( (rc = ctx->traverser->initialize (ctx->fgraph, &(ctx->db.roots),
                                           ctx->matcher)) != 0) {
        cerr << "ERROR: initializing traverser" << endl;
        return -1;
    }
    match_format_t format = match_writers_factory_t::
                                get_writers_type (ctx->params.match_format);
    if (!(ctx->writers = match_writers_factory_t::create (format))) {
        cerr << "ERROR: out of memory allocating traverser" << endl;
        return -1;
    }

    return rc;
}

static void process_args (resource_context_t *ctx, int argc, char *argv[])
{
    int rc = 0;
    int ch = 0;
    std::string token;

    while ((ch = getopt_long (argc, argv, OPTIONS, longopts, NULL)) != -1) {
        switch (ch) {
            case 'h': /* --help */
                usage (0);
                break;
            case 'G': /* --grug*/
                ctx->params.grug = optarg;
                break;
            case 'X': /* --hwloc-xml*/
                ctx->params.hwloc_xml = optarg;
                break;
            case 'W': /* --hwloc-whitelist */
                token = optarg;
                if(token.find_first_not_of(' ') != std::string::npos) {
                    ctx->params.hwloc_whitelist += "cluster,";
                    ctx->params.hwloc_whitelist += token;
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
                    cerr << "[ERROR] unknown format for --match-format: ";
                    cerr << optarg << endl;
                    usage (1);
                }
                break;
            case 'g': /* --graph-format */
                rc = string_to_graph_format (optarg, ctx->params.o_format);
                if ( rc != 0) {
                    cerr << "[ERROR] unknown format for --graph-format: ";
                    cerr << optarg << endl;
                    usage (1);
                }
                graph_format_to_ext (ctx->params.o_format, ctx->params.o_fext);
                break;
            case 'o': /* --graph-output */
                ctx->params.o_fname = optarg;
                break;
            case 'p': /* --prune-filters */
                token = optarg;
                if(token.find_first_not_of(' ') != std::string::npos) {
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
                if ( (ctx->params.reserve_vtx_vec < 0)
                    || (ctx->params.reserve_vtx_vec > 2000000)) {
                    ctx->params.reserve_vtx_vec = 0;
                    cerr << "WARN: out of range specified for --reserve-vtx-vec: ";
                    cerr << optarg << endl;
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

static int init_resource_query (resource_context_t **ctx, int c, char *v[])
{
    int rc = 0;

    if (!(*ctx = new (nothrow)resource_context_t ())) {
        cerr << "ERROR: out of memory allocating resource context" << endl;
        errno = ENOMEM;
        rc = -1;
        goto done;
    }
    set_default_params (*ctx);
    process_args (*ctx, c, v);
    (*ctx)->perf.min = DBL_MAX;
    (*ctx)->perf.max = 0.0f;
    (*ctx)->perf.accum = 0.0f;
    if ( !((*ctx)->matcher = create_match_cb ((*ctx)->params.matcher_policy))) {
        cerr << "ERROR: unknown match policy " << endl;
        cerr << "ERROR: " << (*ctx)->params.matcher_policy << endl;
        rc = -1;
    }
    if (init_resource_graph (*ctx) != 0) {
        cerr << "ERROR: resource graph initialization" << endl;
        rc = -1;
    }

done:
    return rc;
}

static void fini_resource_query (resource_context_t *ctx)
{
    if (ctx->params.r_fname != "")
        ctx->params.r_out.close ();
    if (ctx->params.o_fname != "")
        write_to_graph (ctx);
    destory_resource_ctx (ctx);
}

int main (int argc, char *argv[])
{
    resource_context_t *ctx = NULL;
    if (init_resource_query (&ctx, argc, argv) != 0) {
        cerr << "ERROR: resource query initialization" << endl;
        return EXIT_FAILURE;
    }

    control_loop (ctx);

    fini_resource_query (ctx);

    return EXIT_SUCCESS;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

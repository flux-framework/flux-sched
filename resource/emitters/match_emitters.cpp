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

#include <new>
#include <cerrno>
#include "resource/schema/resource_data.hpp"
#include "resource/schema/resource_graph.hpp"
#include "resource/emitters/match_emitters.hpp"

namespace Flux {
namespace resource_model {

using namespace std;

/****************************************************************************
 *                                                                          *
 *              Base Writers Class Public Method Definitions                *
 *                                                                          *
 ****************************************************************************/

void match_writers_t::compress (stringstream &o, const set<int64_t> &ids)
{
    int64_t base = INT64_MIN;
    int64_t runlen = 0;
    // set is already sorted, so we iterate in ascending order
    for (auto &id : ids) {
        if (id == (base + runlen + 1)) {
            runlen++;
        } else {
            if (runlen != 0)
                o << "-" + to_string (base + runlen) + ",";
            else if (base != INT64_MIN)
                o << ",";
            o << id;
            base = id;
            runlen = 0;
        }
    }
    if (runlen)
        o << "-" << (base + runlen);
}


/****************************************************************************
 *                                                                          *
 *              Simple Writers Class Public Method Definitions              *
 *                                                                          *
 ****************************************************************************/

void sim_match_writers_t::emit (stringstream &out)
{
    out << m_out.str ();
}

void sim_match_writers_t::reset ()
{
    m_out.str ("");
    m_out.clear ();
}

void sim_match_writers_t::emit_vtx (const string &prefix,
                                    const f_resource_graph_t &g, const vtx_t &u,
                                    unsigned int needs, bool exclusive)
{
    string mode = (exclusive)? "x" : "s";
    m_out << prefix << g[u].name << "[" << needs << ":" << mode  << "]" << endl;
}


/****************************************************************************
 *                                                                          *
 *       JSON Graph Format (JGF) Writers Classs Public Definitions          *
 *                                                                          *
 ****************************************************************************/

void jgf_match_writers_t::emit (stringstream &out, bool newline)
{
    size_t vout_size = m_vout.str ().size ();
    size_t eout_size = m_eout.str ().size ();

    if (vout_size > 0 && eout_size > 0) {
        out << "{\"graph\":{\"nodes\":[";
        out << m_vout.str ().substr (0, vout_size - 1);
        out << "],\"edges\":[ ";
        out << m_eout.str ().substr (0, eout_size - 1);
        out << "]}}";
        if (newline)
           out << endl;
    }
}

void jgf_match_writers_t::emit (stringstream &out)
{
    emit (out, true);
}

void jgf_match_writers_t::reset ()
{
    m_vout.str ("");
    m_vout.clear ();
    m_eout.str ("");
    m_eout.clear ();
}

void jgf_match_writers_t::emit_vtx (const string &prefix,
                                    const f_resource_graph_t &g, const vtx_t &u,
                                    unsigned int needs, bool exclusive)
{
    string x = (exclusive)? "true" : "false";
    m_vout << "{";
    m_vout <<     "\"id\":\"" << u << "\",";
    m_vout <<     "\"metadata\":{";
    m_vout <<         "\"type\":" << "\"" << g[u].type << "\"" << ",";
    m_vout <<         "\"basename\":" << "\"" << g[u].basename << "\"" << ",";
    m_vout <<         "\"name\":" << "\"" << g[u].name << "\"" << ",";
    m_vout <<         "\"id\":" << g[u].id << ",";
    m_vout <<         "\"uniq_id\":" << g[u].uniq_id << ",";
    m_vout <<         "\"rank\":" << g[u].rank << ",";
    m_vout <<         "\"exclusive\":" << x << ",";
    m_vout <<         "\"unit\":\"" << g[u].unit << "\",";
    m_vout <<         "\"size\":" << needs;
    if (!g[u].properties.empty ()) {
        stringstream props;
        m_vout <<                         ", ";
        m_vout <<     "\"properties\":{";
        for (auto &kv : g[u].properties)
            props << "\"" << kv.first << "\":\"" << kv.second << "\",";
        m_vout << props.str ().substr (0, props.str ().size () - 1);
        m_vout <<      "}";
    }
    m_vout <<    "}";
    m_vout << "},";
}

void jgf_match_writers_t::emit_edg (const string &prefix,
                                    const f_resource_graph_t &g, const edg_t &e)
{
    m_eout << "{";
    m_eout <<     "\"source\":\"" << source (e, g) << "\",";
    m_eout <<     "\"target\":\"" << target (e, g) << "\",";
    m_eout <<     "\"metadata\":{";
    m_eout <<         "\"name\":" << "\"" << g[e].name << "\"";
    m_eout <<    "}";
    m_eout << "},";
}


/****************************************************************************
 *                                                                          *
 *                 RLITE Writers Class Method Definitions                   *
 *                                                                          *
 ****************************************************************************/

rlite_match_writers_t::rlite_match_writers_t()
{
    m_reducer["core"] = set<int64_t> ();
    m_reducer["gpu"] = set<int64_t> ();
    // Note: GCC 4.8 doesn't support move constructor for stringstream
    // we use a pointer type although it is not ideal.
    m_gatherer["node"] = new stringstream ();
}

rlite_match_writers_t::~rlite_match_writers_t ()
{
    for (auto kv : m_gatherer)
        delete kv.second;
}

void rlite_match_writers_t::reset ()
{
    m_out.str ("");
    m_out.clear ();
}

void rlite_match_writers_t::emit (stringstream &out, bool newline)
{
    size_t size = m_out.str ().size ();
    if (size > 1) {
        out << "{\"R_lite\":[" << m_out.str ().substr (0, size - 1) << "]}";
        if (newline)
            out << endl;
    }
}

void rlite_match_writers_t::emit (stringstream &out)
{
    emit (out, true);
}

bool rlite_match_writers_t::m_reducer_set ()
{
    bool set = false;
    for (auto &kv : m_reducer) {
        if (!kv.second.empty ()) {
            set = true;
            break;
        }
    }
    return set;
}

void rlite_match_writers_t::emit_vtx (const string &prefix,
                                      const f_resource_graph_t &g,
                                      const vtx_t &u,
                                      unsigned int needs,
                                      bool exclusive)
{
    if (m_reducer.find (g[u].type) != m_reducer.end ()) {
        m_reducer[g[u].type].insert (g[u].id);
    } else if (m_gatherer.find (g[u].type) != m_gatherer.end ()) {
        if (m_reducer_set ()) {
            stringstream &gout = *(m_gatherer[g[u].type]);
            gout << "{";
            gout << "\"rank\":" << g[u].rank << ",";
            gout << "\"node\":\"" << g[u].name << "\",";
            gout << "\"children\":{";
            for (auto &kv : m_reducer) {
                if (kv.second.empty ())
                    continue;
                gout << "\"" << kv.first << "\":\"";
                compress (gout, kv.second);
                gout << "\",";
                kv.second.clear ();
            }
            size_t size = gout.str ().size ();
            m_out << gout.str ().substr (0, size - 1) << "}},";
            gout.clear ();
            gout.str ("");
        }
    }
}


/****************************************************************************
 *                                                                          *
 *                  RV1 Writers Class Method Definitions                    *
 *                                                                          *
 ****************************************************************************/

void rv1_match_writers_t::reset ()
{
    rlite.reset ();
    jgf.reset ();
}

void rv1_match_writers_t::emit (stringstream &out)
{
    string exec_key = "\"execution\":";
    string sched_key = "\"scheduling\":";
    size_t base = out.str ().size ();
    out << "{" << exec_key;
    rlite.emit (out, false);
    out << "," << sched_key;
    jgf.emit (out, false);
    out << "}" << endl;
    if (out.str ().size () <= (base + exec_key.size () + sched_key.size () + 3))
        out.str (out.str ().substr (0, base));
}

void rv1_match_writers_t::emit_vtx (const string &prefix,
                                    const f_resource_graph_t &g,
                                    const vtx_t &u,
                                    unsigned int needs,
                                    bool exclusive)
{
    rlite.emit_vtx (prefix, g, u, needs, exclusive);
    jgf.emit_vtx (prefix, g, u, needs, exclusive);
}

void rv1_match_writers_t::emit_edg (const string &prefix,
                                    const f_resource_graph_t &g,
                                    const edg_t &e)
{
    rlite.emit_edg (prefix, g, e);
    jgf.emit_edg (prefix, g, e);
}


/****************************************************************************
 *                                                                          *
 *              RV1 Nosched Writers Class Method Definitions                *
 *                                                                          *
 ****************************************************************************/

void rv1_nosched_match_writers_t::reset ()
{
    rlite.reset ();
}

void rv1_nosched_match_writers_t::emit (stringstream &out)
{
    string exec_key = "\"execution\":";
    size_t base = out.str ().size ();
    out << "{" << exec_key;
    rlite.emit (out, false);
    out << "}" << endl;
    if (out.str ().size () <= (base + exec_key.size () + 2))
        out.str (out.str ().substr (0, base));
}

void rv1_nosched_match_writers_t::emit_vtx (const string &prefix,
                                            const f_resource_graph_t &g,
                                            const vtx_t &u,
                                            unsigned int needs,
                                            bool exclusive)
{
    rlite.emit_vtx (prefix, g, u, needs, exclusive);
}


/****************************************************************************
 *                                                                          *
 *         PRETTY Simple Writers Class Public Method Definitions            *
 *                                                                          *
 ****************************************************************************/

void pretty_sim_match_writers_t::emit (stringstream &out)
{
    for (auto &s: m_out)
        out << s;
}

void pretty_sim_match_writers_t::reset ()
{
    m_out.clear ();
}

void pretty_sim_match_writers_t::emit_vtx (const string &prefix,
                                           const f_resource_graph_t &g,
                                           const vtx_t &u,
                                           unsigned int needs, bool exclusive)
{
    stringstream out;
    string mode = (exclusive)? "exclusive" : "shared";
    out << prefix << g[u].name << "[" << needs << ":" << mode  << "]" << endl;
    m_out.push_front (out.str ());
}


/****************************************************************************
 *                                                                          *
 *    Pretty JSON Graph Format (JGF) Writers Classs Public Definitions      *
 *                                                                          *
 ****************************************************************************/

void pretty_jgf_match_writers_t::emit (stringstream &out)
{
    size_t vout_size = m_vout.str ().size ();
    size_t eout_size = m_eout.str ().size ();

    out << "     {" << endl;
    out << "      \"graph\": {" << endl;
    out << "        \"nodes\": [" << endl;
    if (vout_size > 1)
        out << m_vout.str ().substr (0, vout_size - 2) << endl;
    out << "       ]," << endl;
    out << "        \"edges\": [" << endl;
    if (eout_size > 1)
        out << m_eout.str ().substr (0, eout_size - 2) << endl;
    out << "       ]" << endl;
    out << "      }" << endl;
    out << "     }" << endl;
}

void pretty_jgf_match_writers_t::reset ()
{
    m_vout.str ("");
    m_vout.clear ();
    m_eout.str ("");
    m_eout.clear ();
}

void pretty_jgf_match_writers_t::emit_vtx (const string &prefix,
                                           const f_resource_graph_t &g,
                                           const vtx_t &u, unsigned int needs,
                                           bool exclusive)
{
    string x = (exclusive)? "true" : "false";
    string indent = "        ";
    m_vout << indent << "  { " << endl;
    m_vout << indent << "    \"id\": \"" << u << "\", " << endl;
    m_vout << indent << "    \"metadata\": { " << endl;
    m_vout << indent << "          \"type\": " << "\"" << g[u].type
           << "\", " << endl;
    m_vout << indent << "          \"basename\": " << "\"" << g[u].basename
           << "\", " << endl;
    m_vout << indent << "          \"name\": " << "\"" << g[u].name
           << "\", " << endl;
    m_vout << indent << "          \"id\": " << g[u].id << ", " << endl;
    m_vout << indent << "          \"uuid\": " << g[u].uniq_id << ", " << endl;
    m_vout << indent << "          \"rank\": " << g[u].rank << ", " << endl;
    m_vout << indent << "          \"exclusive\": " << x << ", " << endl;
    m_vout << indent << "          \"unit\": \"" << g[u].unit << "\", " << endl;
    m_vout << indent << "          \"size\": " << needs << endl;
    if (!g[u].properties.empty ()) {
        stringstream props;
        m_vout <<                                     ", ";
        m_vout << indent << "           \"properties\": { ";
        for (auto &kv : g[u].properties) {
            props << indent << "                \"" << kv.first << "\": \""
                            << kv.second << "\"," << endl;
        }
        m_vout << props.str ().substr (0, props.str ().size () - 2) << endl;
        m_vout << indent << "                     }";
    }
    m_vout << indent << "    }" << endl;
    m_vout << indent << "  }," << endl;
}

void pretty_jgf_match_writers_t::emit_edg (const string &prefix,
                                           const f_resource_graph_t &g,
                                           const edg_t &e)
{
    string indent = "        ";
    m_eout << indent << "{ " << endl;
    m_eout << indent << "    \"source\": \"" << source (e, g) << "\", " << endl;
    m_eout << indent << "    \"target\": \"" << target (e, g) << "\", " << endl;
    m_eout << indent << "    \"metadata\": { " << endl;
    m_eout << indent << "        \"name\": " << "\"" << g[e].name
           << "\"" << endl;
    m_eout << indent << "    }" << endl;
    m_eout << indent << "}," << endl;
}


/****************************************************************************
 *                                                                          *
 *             Pretty RLITE Writers Class Method Definitions                *
 *                                                                          *
 ****************************************************************************/

pretty_rlite_match_writers_t::pretty_rlite_match_writers_t()
{
    m_reducer["core"] = set<int64_t> ();
    m_reducer["gpu"] = set<int64_t> ();
    m_gatherer["node"] = new stringstream ();
}

pretty_rlite_match_writers_t::~pretty_rlite_match_writers_t ()
{
    for (auto kv : m_gatherer)
        delete kv.second;
}

void pretty_rlite_match_writers_t::reset ()
{
    m_out.str ("");
    m_out.clear ();
}

void pretty_rlite_match_writers_t::emit (stringstream &out)
{
    size_t size = m_out.str ().size ();
    if (size > 1) {
        out << "    {" << endl;
        out << "      \"R_lite\": [" << endl;
        out << m_out.str ().substr (0, size - 2) << endl;
        out << "      ]" << endl;
        out << "    }" << endl;
    }
}

bool pretty_rlite_match_writers_t::m_reducer_set ()
{
    bool set = false;
    for (auto &kv : m_reducer) {
        if (!kv.second.empty ()) {
            set = true;
            break;
        }
    }
    return set;
}

void pretty_rlite_match_writers_t::emit_vtx (const string &prefix,
                                             const f_resource_graph_t &g,
                                             const vtx_t &u,
                                             unsigned int needs,
                                             bool exclusive)
{
    string indent = "    ";
    if (m_reducer.find (g[u].type) != m_reducer.end ()) {
        m_reducer[g[u].type].insert (g[u].id);
    } else if (m_gatherer.find (g[u].type) != m_gatherer.end ()) {
        if (m_reducer_set ()) {
            stringstream &gout = *(m_gatherer[g[u].type]);
            gout << indent << "    {" << endl;
            gout << indent << "      \"rank\": " << g[u].rank << "," << endl;
            gout << indent << "      \"node\": \"" << g[u].name << "\","
                 << endl;
            gout << indent << "      \"children\": {" << endl;
            for (auto &kv : m_reducer) {
                if (kv.second.empty ())
                    continue;
                gout << indent << "         \"" << kv.first << "\": \"";
                compress (gout, kv.second);
                gout << "\"," << endl;
                kv.second.clear ();
	        }
            size_t size = gout.str ().size ();
            m_out << gout.str ().substr (0, size - 2) << endl;
            m_out << indent << "      }" << endl;
            m_out << indent << "    }," << endl;
            gout.clear ();
            gout.str ("");
        }
    }
}


/****************************************************************************
 *                                                                          *
 *              Pretty RV1 Writers Class Method Definitions                 *
 *                                                                          *
 ****************************************************************************/

void pretty_rv1_match_writers_t::reset ()
{
    rlite.reset ();
    jgf.reset ();
}

void pretty_rv1_match_writers_t::emit (stringstream &out)
{
    out << "{" << endl;
    out << "  \"execution\": " << endl;
    rlite.emit (out);
    out << "     ," << endl;
    out << "  \"scheduling\": " << endl;
    jgf.emit (out);
    out << "}" << endl;
}

void pretty_rv1_match_writers_t::emit_vtx (const string &prefix,
                                           const f_resource_graph_t &g,
                                           const vtx_t &u,
                                           unsigned int needs,
                                           bool exclusive)
{
    rlite.emit_vtx (prefix, g, u, needs, exclusive);
    jgf.emit_vtx (prefix, g, u, needs, exclusive);
}

void pretty_rv1_match_writers_t::emit_edg (const string &prefix,
                                           const f_resource_graph_t &g,
                                           const edg_t &e)
{
    rlite.emit_edg (prefix, g, e);
    jgf.emit_edg (prefix, g, e);
}


/****************************************************************************
 *                                                                          *
 *             Match Writers Factory Class Method Definitions               *
 *                                                                          *
 ****************************************************************************/

match_writers_t *match_writers_factory_t::create (match_format_t f)
{
    match_writers_t *w = NULL;

    switch (f) {
    case match_format_t::SIMPLE:
        w = new (nothrow)sim_match_writers_t ();
        break;
    case match_format_t::JGF:
        w = new (nothrow)jgf_match_writers_t ();
        break;
    case match_format_t::RLITE:
        w = new (nothrow)rlite_match_writers_t ();
        break;
    case match_format_t::RV1_NOSCHED:
        w = new (nothrow)rv1_nosched_match_writers_t ();
        break;
    case match_format_t::PRETTY_SIMPLE:
        w = new (nothrow)pretty_sim_match_writers_t ();
        break;
    case match_format_t::PRETTY_JGF:
        w = new (nothrow)pretty_jgf_match_writers_t ();
        break;
    case match_format_t::PRETTY_RLITE:
        w = new (nothrow)pretty_rlite_match_writers_t ();
        break;
    case match_format_t::PRETTY_RV1:
        w = new (nothrow)pretty_rv1_match_writers_t ();
        break;
    case match_format_t::RV1:
    default:
        w = new (nothrow)rv1_match_writers_t ();
        break;
    }

    if (!w)
        errno = ENOMEM;

    return w;
}

match_format_t match_writers_factory_t::get_writers_type (const string &n)
{
    match_format_t format = match_format_t::SIMPLE;
    if (n == "jgf")
        format = match_format_t::JGF;
    else if (n == "rlite")
        format = match_format_t::RLITE;
    else if (n == "rv1")
        format = match_format_t::RV1;
    else if (n == "rv1_nosched")
        format = match_format_t::RV1_NOSCHED;
    else if (n == "pretty_simple")
        format = match_format_t::PRETTY_SIMPLE;
    else if (n == "pretty_jgf")
        format = match_format_t::PRETTY_JGF;
    else if (n == "pretty_rlite")
        format = match_format_t::PRETTY_RLITE;
    else if (n == "pretty_rv1")
        format = match_format_t::PRETTY_RV1;
    return format;
}

bool known_match_format (const string &format)
{
   return (format == "simple"
           || format == "jgf"
           || format == "rlite"
           || format == "rv1"
           || format == "rv1_nosched"
           || format == "pretty_simple"
           || format == "pretty_jgf"
           || format == "pretty_rlite"
           || format == "pretty_rv1");
}

} // namespace resource_model
} // namespace Flux

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

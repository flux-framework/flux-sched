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
#include "resource/writers/match_writers.hpp"

namespace Flux {
namespace resource_model {


/****************************************************************************
 *                                                                          *
 *              Base Writers Class Public Method Definitions                *
 *                                                                          *
 ****************************************************************************/

void match_writers_t::compress (std::stringstream &o,
                                const std::set<int64_t> &ids)
{
    int64_t base = INT64_MIN;
    int64_t runlen = 0;
    // set is already sorted, so we iterate in ascending order
    for (auto &id : ids) {
        if (id == (base + runlen + 1)) {
            runlen++;
        } else {
            if (runlen != 0)
                o << "-" + std::to_string (base + runlen) + ",";
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

int sim_match_writers_t::emit (std::stringstream &out)
{
    out << m_out.str ();
    return 0;
}

void sim_match_writers_t::reset ()
{
    m_out.str ("");
    m_out.clear ();
}

int sim_match_writers_t::emit_vtx (const std::string &prefix,
                                   const f_resource_graph_t &g, const vtx_t &u,
                                   unsigned int needs, bool exclusive)
{
    std::string mode = (exclusive)? "x" : "s";
    m_out << prefix << g[u].name << "[" << needs << ":" << mode  << "]"
          << std::endl;
    return 0;
}


/****************************************************************************
 *                                                                          *
 *       JSON Graph Format (JGF) Writers Classs Public Definitions          *
 *                                                                          *
 ****************************************************************************/

int jgf_match_writers_t::emit (std::stringstream &out, bool newline)
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
           out << std::endl;
    }
    return 0;
}

int jgf_match_writers_t::emit (std::stringstream &out)
{
    return emit (out, true);
}

void jgf_match_writers_t::reset ()
{
    m_vout.str ("");
    m_vout.clear ();
    m_eout.str ("");
    m_eout.clear ();
}

int jgf_match_writers_t::emit_vtx (const std::string &prefix,
                                   const f_resource_graph_t &g, const vtx_t &u,
                                   unsigned int needs, bool exclusive)
{
    std::string x = (exclusive)? "true" : "false";
    m_vout << "{";
    m_vout <<     "\"id\":\"" << g[u].uniq_id << "\",";
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
        std::stringstream props;
        m_vout <<                         ", ";
        m_vout <<     "\"properties\":{";
        for (auto &kv : g[u].properties)
            props << "\"" << kv.first << "\":\"" << kv.second << "\",";
        m_vout << props.str ().substr (0, props.str ().size () - 1);
        m_vout <<      "}";
    }
    if (!g[u].paths.empty ()) {
        std::stringstream paths;
        m_vout <<                         ", ";
        m_vout <<     "\"paths\":{";
        for (auto &kv : g[u].paths)
            paths << "\"" << kv.first << "\":\"" << kv.second << "\",";
        m_vout << paths.str ().substr (0, paths.str ().size () - 1);
        m_vout <<      "}";
    }

    m_vout <<    "}";
    m_vout << "},";
    return 0;
}

int jgf_match_writers_t::emit_edg (const std::string &prefix,
                                   const f_resource_graph_t &g, const edg_t &e)
{
    m_eout << "{";
    m_eout <<     "\"source\":\"" << g[source (e, g)].uniq_id << "\",";
    m_eout <<     "\"target\":\"" << g[target (e, g)].uniq_id << "\",";
    m_eout <<     "\"metadata\":{";

    if (!g[e].name.empty ()) {
        std::stringstream names;
        m_eout <<     "\"name\":{";
        for (auto &kv : g[e].name)
            names << "\"" << kv.first << "\":\"" << kv.second << "\",";
        m_eout << names.str ().substr (0, names.str ().size () - 1);
        m_eout <<      "}";
    }

    m_eout <<    "}";
    m_eout << "},";
    return 0;
}


/****************************************************************************
 *                                                                          *
 *                 RLITE Writers Class Method Definitions                   *
 *                                                                          *
 ****************************************************************************/

rlite_match_writers_t::rlite_match_writers_t()
{
    m_reducer["core"] = std::set<int64_t> ();
    m_reducer["gpu"] = std::set<int64_t> ();
    m_gatherer["node"] = std::make_shared<std::stringstream> ();
}

rlite_match_writers_t::~rlite_match_writers_t ()
{

}

void rlite_match_writers_t::reset ()
{
    m_out.str ("");
    m_out.clear ();
}

int rlite_match_writers_t::emit (std::stringstream &out, bool newline)
{
    size_t size = m_out.str ().size ();
    if (size > 1) {
        out << "{\"R_lite\":[" << m_out.str ().substr (0, size - 1) << "]}";
        if (newline)
            out << std::endl;
    }
    return 0;
}

int rlite_match_writers_t::emit (std::stringstream &out)
{
    return emit (out, true);
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

int rlite_match_writers_t::emit_vtx (const std::string &prefix,
                                     const f_resource_graph_t &g,
                                     const vtx_t &u,
                                     unsigned int needs,
                                     bool exclusive)
{
    if (m_reducer.find (g[u].type) != m_reducer.end ()) {
        m_reducer[g[u].type].insert (g[u].id);
    } else if (m_gatherer.find (g[u].type) != m_gatherer.end ()) {
        if (m_reducer_set ()) {
            std::stringstream &gout = *(m_gatherer[g[u].type]);
            gout << "{";
            gout << "\"rank\":\"" << g[u].rank << "\",";
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
    return 0;
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

int rv1_match_writers_t::emit (std::stringstream &out)
{
    std::string ver = "\"version\":1";
    std::string exec_key = "\"execution\":";
    std::string sched_key = "\"scheduling\":";
    size_t base = out.str ().size ();
    out << "{" << ver;
    out << "," << exec_key;
    rlite.emit (out, false);
    out << "," << sched_key;
    jgf.emit (out, false);
    out << "}" << std::endl;
    if (out.str ().size () <= (base + ver.size () + exec_key.size ()
                                    + sched_key.size () + 5))
        out.str (out.str ().substr (0, base));
    return 0;
}

int rv1_match_writers_t::emit_vtx (const std::string &prefix,
                                   const f_resource_graph_t &g,
                                   const vtx_t &u,
                                   unsigned int needs,
                                   bool exclusive)
{
    int rc = rlite.emit_vtx (prefix, g, u, needs, exclusive);
    rc += jgf.emit_vtx (prefix, g, u, needs, exclusive);
    return (!rc)? 0 : -1;
}

int rv1_match_writers_t::emit_edg (const std::string &prefix,
                                   const f_resource_graph_t &g,
                                   const edg_t &e)
{
    int rc = rlite.emit_edg (prefix, g, e);
    rc += jgf.emit_edg (prefix, g, e);
    return (!rc)? 0 : -1;
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

int rv1_nosched_match_writers_t::emit (std::stringstream &out)
{
    std::string ver = "\"version\":1";
    std::string exec_key = "\"execution\":";
    size_t base = out.str ().size ();
    out << "{" << ver;
    out << "," << exec_key;
    rlite.emit (out, false);
    out << "}" << std::endl;
    if (out.str ().size () <= (base + ver.size () + exec_key.size () + 4))
        out.str (out.str ().substr (0, base));
    return 0;
}

int rv1_nosched_match_writers_t::emit_vtx (const std::string &prefix,
                                           const f_resource_graph_t &g,
                                           const vtx_t &u,
                                           unsigned int needs,
                                           bool exclusive)
{
    return rlite.emit_vtx (prefix, g, u, needs, exclusive);
}


/****************************************************************************
 *                                                                          *
 *         PRETTY Simple Writers Class Public Method Definitions            *
 *                                                                          *
 ****************************************************************************/

int pretty_sim_match_writers_t::emit (std::stringstream &out)
{
    for (auto &s: m_out)
        out << s;
    return 0;
}

void pretty_sim_match_writers_t::reset ()
{
    m_out.clear ();
}

int pretty_sim_match_writers_t::emit_vtx (const std::string &prefix,
                                          const f_resource_graph_t &g,
                                          const vtx_t &u,
                                          unsigned int needs, bool exclusive)
{
    std::stringstream out;
    std::string mode = (exclusive)? "exclusive" : "shared";
    out << prefix << g[u].name << "[" << needs << ":" << mode  << "]"
        << std::endl;
    m_out.push_front (out.str ());
    return 0;
}


/****************************************************************************
 *                                                                          *
 *             Match Writers Factory Class Method Definitions               *
 *                                                                          *
 ****************************************************************************/

std::shared_ptr<match_writers_t> match_writers_factory_t::
                                     create (match_format_t f)
{
    std::shared_ptr<match_writers_t> w = nullptr;

    try {
        switch (f) {
        case match_format_t::SIMPLE:
            w = std::make_shared<sim_match_writers_t> ();
            break;
        case match_format_t::JGF:
            w = std::make_shared<jgf_match_writers_t> ();
            break;
        case match_format_t::RLITE:
            w = std::make_shared<rlite_match_writers_t> ();
            break;
        case match_format_t::RV1_NOSCHED:
            w = std::make_shared<rv1_nosched_match_writers_t> ();
            break;
        case match_format_t::PRETTY_SIMPLE:
            w = std::make_shared<pretty_sim_match_writers_t> ();
            break;
        case match_format_t::RV1:
        default:
            w = std::make_shared<rv1_match_writers_t> ();
            break;
        }
    } catch (std::bad_alloc &e) {
        errno = ENOMEM;
        w = nullptr;
    }

    return w;
}

match_format_t match_writers_factory_t::get_writers_type (const std::string &n)
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
    return format;
}

bool known_match_format (const std::string &format)
{
   return (format == "simple"
           || format == "jgf"
           || format == "rlite"
           || format == "rv1"
           || format == "rv1_nosched"
           || format == "pretty_simple");
}

} // namespace resource_model
} // namespace Flux

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

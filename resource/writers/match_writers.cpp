/*****************************************************************************\
 *  Copyright (c) 2014 - 2020 Lawrence Livermore National Security, LLC.
 *  Produced at
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

bool sim_match_writers_t::empty ()
{
    return m_out.str ().empty ();
}

int sim_match_writers_t::emit (std::stringstream &out)
{
    out << m_out.str ();
    m_out.str ("");
    m_out.clear ();
    return 0;
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

jgf_match_writers_t::~jgf_match_writers_t ()
{
    json_decref (m_vout);
    m_vout = NULL;
    json_decref (m_eout);
    m_eout = NULL;
}

int jgf_match_writers_t::initialize ()
{
    if (!(m_vout = json_array ())) {
        errno = ENOMEM;
        return -1;
    }
    if (!(m_eout = json_array ())) {
        errno = ENOMEM;
        return -1;
    }
    return 0;
}

bool jgf_match_writers_t::empty ()
{
    size_t v_size = json_array_size (m_vout);
    size_t e_size = json_array_size (m_eout);
    return (v_size == 0) && (e_size == 0);
}

json_t *jgf_match_writers_t::emit_json ()
{
    json_t *o = NULL;
    if (json_array_size (m_vout) == 0)
        return NULL;
    if (!(o = json_pack ("{s:{s:o s:o}}",
                             "graph",
                                 "nodes", m_vout,
                                 "edges", m_eout))) {
        errno = ENOMEM;
        goto ret;
    }
    if (!(m_vout = json_array ())) {
        errno = ENOMEM;
    }
    if (!(m_eout = json_array ())) {
        errno = ENOMEM;
    }
ret:
    return o;
}

int jgf_match_writers_t::emit (std::stringstream &out)
{
    json_t *o = NULL;
    if ((o = emit_json ()) != NULL) {
        char *json_str = NULL;
        if (!(json_str = json_dumps (o, JSON_INDENT (0)))) {
            json_decref (o);
            o = NULL;
            errno = ENOMEM;
            goto out;
        }
        out << json_str << std::endl;
        free (json_str);
        json_decref (o);
    }
out:
    return (!o && errno == ENOMEM)? -1 : 0;
}

int jgf_match_writers_t::emit_vtx (const std::string &prefix,
                                   const f_resource_graph_t &g, const vtx_t &u,
                                   unsigned int needs, bool exclusive)
{
    int rc = 0;
    json_t *o = NULL;
    json_t *b = NULL;

    if (!m_vout) {
        errno = EINVAL;
        rc = -1;
        goto out;
    }
    if (!(b = emit_vtx_base (g, u, needs, exclusive)))
        goto out;
    if ((rc = emit_vtx_prop (b, g, u, needs, exclusive)) < 0)
        goto out;
    if ((rc = emit_vtx_path (b, g, u, needs, exclusive)) < 0)
        goto out;
    if ((o = json_pack ("{s:s s:o}",
                            "id", std::to_string (g[u].uniq_id).c_str (),
                            "metadata", b)) == NULL) {
        errno = ENOMEM;
        rc = -1;
        goto out;
    }
    if ((rc = json_array_append_new (m_vout, o)) < 0) {
        errno = ENOMEM;
        goto out;
    }

out:
    return rc;
}

int jgf_match_writers_t::emit_edg (const std::string &prefix,
                                   const f_resource_graph_t &g, const edg_t &e)
{
    int rc = 0;
    json_t *o = NULL;
    json_t *m = NULL;

    if (!m_eout) {
        errno = EINVAL;
        rc = -1;
        goto out;
    }
    if (!(m = json_object ())) {
        errno = ENOMEM;
        rc = -1;
        goto out;
    }
    if ((rc = emit_edg_meta (m, g, e)) < 0) {
        errno = ENOMEM;
        goto out;
    }
    if (!(o = json_pack ("{s:s s:s s:o}",
                  "source", std::to_string (g[source (e, g)].uniq_id).c_str (),
                  "target", std::to_string (g[target (e, g)].uniq_id).c_str (),
                  "metadata", m))) {
        rc = -1;
        errno = ENOMEM;
        goto out;
    }
    if ((rc = json_array_append_new (m_eout, o)) == -1) {
        errno = ENOMEM;
        goto out;
    }

out:
    return rc;
}


/****************************************************************************
 *                                                                          *
 *       JSON Graph Format (JGF) Writers Classs Private Definitions         *
 *                                                                          *
 ****************************************************************************/

json_t *jgf_match_writers_t::emit_vtx_base (const f_resource_graph_t &g,
                                            const vtx_t &u,
                                            unsigned int needs, bool exclusive)
{
    json_t *o = NULL;

    if (!(o = json_pack ("{s:s s:s s:s s:I s:I s:i s:b s:s s:I}",
                            "type", g[u].type.c_str (),
                            "basename", g[u].basename.c_str (),
                            "name", g[u].name.c_str (),
                            "id", g[u].id,
                            "uniq_id", g[u].uniq_id,
                            "rank", g[u].rank,
                            "exclusive", (exclusive)? 1 : 0,
                            "unit", g[u].unit.c_str (),
                            "size", needs))) {
        errno = ENOMEM;
    }
    return o;
}


int jgf_match_writers_t::emit_vtx_prop (json_t *o, const f_resource_graph_t &g,
                                        const vtx_t &u,
                                        unsigned int needs, bool exclusive)
{
    int rc = 0;
    if (!g[u].properties.empty ()) {
        json_t *p = NULL;
        if (!(p = json_object ())) {
            errno = ENOMEM;
            rc = -1;
            goto out;
        }
        for (auto &kv : g[u].properties) {
            json_t *vo = NULL;
            if (!(vo = json_string (kv.second.c_str ()))) {
                rc = -1;
                errno = ENOMEM;
                goto out;
            }
            if ((rc = json_object_set_new (p, kv.first.c_str (), vo)) == -1) {
                errno = ENOMEM;
                goto out;
            }
        }
        if ((rc = json_object_set_new (o, "properties", p)) == -1) {
            json_decref (p);
            errno = ENOMEM;
            goto out;
        }
    }

out:
    return rc;
}

int jgf_match_writers_t::emit_vtx_path (json_t *o, const f_resource_graph_t &g,
                                        const vtx_t &u,
                                        unsigned int needs, bool exclusive)
{
    int rc = 0;
    if (!g[u].paths.empty ()) {
        json_t *p = NULL;
        if (!(p = json_object ())) {
            errno = ENOMEM;
            rc = -1;
            goto out;
        }
        for (auto &kv : g[u].paths) {
            json_t *vo = NULL;
            if (!(vo = json_string (kv.second.c_str ()))) {
                rc = -1;
                errno = ENOMEM;
                goto out;
            }
            if ((rc = json_object_set_new (p, kv.first.c_str (), vo)) == -1) {
                errno = ENOMEM;
                goto out;
            }
        }
        if ((rc = json_object_set_new (o, "paths", p)) == -1) {
            json_decref (p);
            errno = ENOMEM;
            goto out;
        }
    }

out:
    return rc;
}

int jgf_match_writers_t::emit_edg_meta (json_t *o, const f_resource_graph_t &g,
                                        const edg_t &e)
{
    int rc = 0;
    if (!o) {
        errno = EINVAL;
        rc = -1;
        goto out;
    }
    if (!g[e].name.empty ()) {
        json_t *n = NULL;
        if (!(n = json_object ())) {
            errno = ENOMEM;
            rc = -1;
            goto out;
        }
        for (auto &kv : g[e].name) {
            json_t *vo = NULL;
            if (!(vo = json_string (kv.second.c_str ()))) {
                errno = ENOMEM;
                goto out;
            }
            if ((rc = json_object_set_new (n, kv.first.c_str (), vo) == -1) {
                errno = ENOMEM;
                goto out;
            }
        }
        if ((rc = json_object_set_new (o, "name", n)) == -1) {
            errno = ENOMEM;
            goto out;
        }
    }

out:
    return rc;
}


/****************************************************************************
 *                                                                          *
 *            RLITE Writers Class Public Method Definitions                 *
 *                                                                          *
 ****************************************************************************/

rlite_match_writers_t::~rlite_match_writers_t ()
{
    json_decref (m_out);
}

int rlite_match_writers_t::initialize ()
{
    m_reducer["core"] = std::set<int64_t> ();
    m_reducer["gpu"] = std::set<int64_t> ();
    m_gatherer.insert ("node");
    if (!(m_out = json_array ())) {
        errno = ENOMEM;
        return -1;
    }
    return 0;
}

bool rlite_match_writers_t::empty ()
{
    return (json_array_size (m_out) == 0)? true : false;
}

json_t *rlite_match_writers_t::emit_json ()
{
    json_t *o = NULL;
    if (json_array_size (m_out) != 0) {
        o = m_out;
        if (!(m_out = json_array ())) {
            json_decref (o);
            o = NULL;
            errno = ENOMEM;
        }
    }
    return o;
}

int rlite_match_writers_t::emit (std::stringstream &out)
{
    json_t *o = NULL;
    if ((o = emit_json ()) != NULL) {
        char *json_str = NULL;
        if (!(json_str = json_dumps (o, JSON_INDENT (0)))) {
            json_decref (o);
            o = NULL;
            errno = ENOMEM;
            goto out;
        }
        out << json_str << std::endl;
        free (json_str);
        json_decref (o);
     }
out:
    return (!o && errno == ENOMEM)? -1 : 0;
}

int rlite_match_writers_t::emit_vtx (const std::string &prefix,
                                     const f_resource_graph_t &g,
                                     const vtx_t &u,
                                     unsigned int needs,
                                     bool exclusive)
{
    int rc = 0;

    if (!m_out) {
        errno = EINVAL;
        rc = -1;
        goto out;
    }
    if (m_reducer.find (g[u].type) != m_reducer.end ()) {
        m_reducer[g[u].type].insert (g[u].id);
    } else if (m_gatherer.find (g[u].type) != m_gatherer.end ()) {
        if ((rc = emit_gatherer (g, u)) < 0)
            goto out;
    }
out:
    return rc;
}


/****************************************************************************
 *                                                                          *
 *            RLITE Writers Class Private Method Definitions                *
 *                                                                          *
 ****************************************************************************/

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

int rlite_match_writers_t::emit_gatherer (const f_resource_graph_t &g,
                                          const vtx_t &u)
{
    int rc = 0;
    json_t *o = NULL;
    json_t *co = NULL;

    if (!m_reducer_set ())
        goto out;
    if (!(co = json_object ())) {
        errno = ENOMEM;
        rc = -1;
        goto out;
    }
    for (auto &kv : m_reducer) {
        if (kv.second.empty ())
            continue;
        std::stringstream s;
        compress (s, kv.second);
        json_t *vo = NULL;
        if (!(vo = json_string (kv.second.c_str ()))) {
            rc = -1;
            errno = ENOMEM;
            goto out;
        }
        if ((rc = json_object_set_new (co, kv.first.c_str (), vo)) < 0) {
            errno = ENOMEM;
            goto out;
        }
        kv.second.clear ();
    }
    if (!(o = json_pack ("{s:s s:s s:o}",
                             "rank", std::to_string (g[u].rank).c_str (),
                             "node", g[u].name.c_str (),
                             "children", co))) {
        errno = ENOMEM;
        rc = -1;
        goto out;
    }
    if ((rc = json_array_append_new (m_out, o)) != 0) {
        errno = ENOMEM;
        goto out;
    }
    return 0;
}


/****************************************************************************
 *                                                                          *
 *                  RV1 Writers Class Method Definitions                    *
 *                                                                          *
 ****************************************************************************/

int rv1_match_writers_t::initialize ()
{
    int rc = rlite.initialize ();
    rc += jgf.initialize ();
    return 0;
}

bool rv1_match_writers_t::empty ()
{
    return (rlite.empty () && jgf.empty ());
}

int rv1_match_writers_t::emit (std::stringstream &out)
{
    int rc = 0;
    json_t *o = NULL;
    json_t *rlite_o = NULL;
    json_t *jgf_o = NULL;
    char *json_str = NULL;

    if (empty ())
        goto out;

    if (!(rlite_o = rlite.emit_json ()) && errno == ENOMEM) {
        rc = -1;
        goto out;
    }
    if (!(jgf_o = jgf.emit_json ()) && errno == ENOMEM) {
        rc = -1;
        goto out;
    }
    if (!(o = json_pack ("{s:i s:{s:o s:I s:I} s:o}",
                             "version", 1,
                             "execution",
                             "R_lite",  rlite_o,
                             "starttime", m_starttime,
                             "expiration", m_expiration,
                             "scheduling", jgf_o))) {
        errno = ENOMEM;
        rc = -1;
        goto out;
    }
    if (!(json_str = json_dumps (o, JSON_INDENT (0)))) {
        json_decref (o);
        o = NULL;
        errno = ENOMEM;
        rc = -1;
        goto out;
    }
    out << json_str << std::endl;
    free (json_str);
    json_decref (o);

out:
    return rc;
}

int rv1_match_writers_t::emit_vtx (const std::string &prefix,
                                   const f_resource_graph_t &g,
                                   const vtx_t &u,
                                   unsigned int needs,
                                   bool exclusive)
{
    int rc = 0;
    if ((rc = rlite.emit_vtx (prefix, g, u, needs, exclusive)) == 0)
        rc = jgf.emit_vtx (prefix, g, u, needs, exclusive);
    return rc;
}

int rv1_match_writers_t::emit_edg (const std::string &prefix,
                                   const f_resource_graph_t &g,
                                   const edg_t &e)
{
    int rc = 0;
    if ((rc = rlite.emit_edg (prefix, g, e)) == 0)
        rc = jgf.emit_edg (prefix, g, e);
    return rc;
}

int rv1_match_writers_t::emit_tm (uint64_t start_tm, uint64_t end_tm)
{
    m_starttime = start_tm;
    m_expiration = end_tm;
    return 0;
}


/****************************************************************************
 *                                                                          *
 *              RV1 Nosched Writers Class Method Definitions                *
 *                                                                          *
 ****************************************************************************/

int rv1_nosched_match_writers_t::initialize ()
{
    return rlite.initialize ();
}

bool rv1_nosched_match_writers_t::empty ()
{
    return rlite.empty ();
}

int rv1_nosched_match_writers_t::emit (std::stringstream &out)
{
    int rc = 0;
    json_t *o = NULL;
    json_t *rlite_o = NULL;
    char *json_str = NULL;

    if (rlite.empty ())
        goto out;
    if (!(rlite_o = rlite.emit_json ()) && errno == ENOMEM) {
        rc = -1;
        goto out;
    }
    if (!(o = json_pack ("{s:i s:{s:o s:I s:I}}",
                             "version", 1,
                             "execution",
                             "R_lite",  rlite_o,
                             "starttime", m_starttime,
                             "expiration", m_expiration))) {
        errno = ENOMEM;
        rc = -1;
        goto out;
    }
    if (!(json_str = json_dumps (o, JSON_INDENT (0)))) {
        json_decref (o);
        o = NULL;
        errno = ENOMEM;
        rc = -1;
        goto out;
    }
    out << json_str << std::endl;
    free (json_str);
    json_decref (o);

out:
    return rc;
}

int rv1_nosched_match_writers_t::emit_vtx (const std::string &prefix,
                                           const f_resource_graph_t &g,
                                           const vtx_t &u,
                                           unsigned int needs,
                                           bool exclusive)
{
    return rlite.emit_vtx (prefix, g, u, needs, exclusive);
}

int rv1_nosched_match_writers_t::emit_tm (uint64_t start_tm, uint64_t end_tm)
{
    m_starttime = start_tm;
    m_expiration = end_tm;
    return 0;
}


/****************************************************************************
 *                                                                          *
 *         PRETTY Simple Writers Class Public Method Definitions            *
 *                                                                          *
 ****************************************************************************/

bool pretty_sim_match_writers_t::empty ()
{
    bool empty = true;
    for (auto &s: m_out) {
        if (!s.empty ()) {
            empty = false;
            break;
        }
    }
    return empty;
}

int pretty_sim_match_writers_t::emit (std::stringstream &out)
{
    for (auto &s: m_out)
        out << s;
    m_out.clear ();
    return 0;
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
        w->initialize ();
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

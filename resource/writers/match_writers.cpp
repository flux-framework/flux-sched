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

jgf_match_writers_t::jgf_match_writers_t ()
{
    if (!(m_vout = json_array ()))
        throw std::bad_alloc ();
    if (!(m_eout = json_array ())) {
        json_decref (m_vout);
        m_vout = NULL;
        throw std::bad_alloc ();
    }
}

jgf_match_writers_t::jgf_match_writers_t (const jgf_match_writers_t &w)
{
    if (!(m_vout = json_deep_copy (w.m_vout)))
        throw std::bad_alloc ();
    if (!(m_eout = json_deep_copy (w.m_eout))) {
        json_decref (m_vout);
        m_vout = NULL;
        throw std::bad_alloc ();
    }
}

jgf_match_writers_t &jgf_match_writers_t::operator=(
                                              const jgf_match_writers_t &w)
{
    if (!(m_vout = json_deep_copy (w.m_vout)))
        throw std::bad_alloc ();
    if (!(m_eout = json_deep_copy (w.m_eout))) {
        json_decref (m_vout);
        m_vout = NULL;
        throw std::bad_alloc ();
    }
    return *this;
}

jgf_match_writers_t::~jgf_match_writers_t ()
{
    json_decref (m_vout);
    json_decref (m_eout);
}

bool jgf_match_writers_t::empty ()
{
    size_t v_size = json_array_size (m_vout);
    size_t e_size = json_array_size (m_eout);
    return (v_size == 0) && (e_size == 0);
}

int jgf_match_writers_t::emit_json (json_t **o)
{
    int rc = 0;

    if ((rc = check_array_sizes ()) <= 0)
        goto ret;
    if (!(*o = json_pack ("{s:{s:o s:o}}",
                              "graph",
                                  "nodes", m_vout,
                                  "edges", m_eout))) {
        json_decref (m_vout);
        json_decref (m_eout);
        m_vout = NULL;
        m_eout = NULL;
        rc = -1;
        errno = ENOMEM;
        goto ret;
    }
    m_vout = NULL;
    m_eout = NULL;
    if ((alloc_json_arrays ()) < 0) {
        json_decref (*o);
        rc = -1;
        *o = NULL;
        goto ret;
    }

ret:
    return rc;
}

int jgf_match_writers_t::emit (std::stringstream &out)
{
    int rc = 0;
    json_t *o = NULL;
    if ((rc = emit_json (&o)) > 0) {
        char *json_str = NULL;
        if (!(json_str = json_dumps (o, JSON_INDENT (0)))) {
            json_decref (o);
            o = NULL;
            rc = -1;
            errno = ENOMEM;
            goto ret;
        }
        out << json_str << std::endl;
        free (json_str);
        json_decref (o);
    }
ret:
    return (rc == -1)? -1 : 0;
}

int jgf_match_writers_t::emit_vtx (const std::string &prefix,
                                   const f_resource_graph_t &g, const vtx_t &u,
                                   unsigned int needs, bool exclusive)
{
    int rc = 0;
    json_t *o = NULL;
    json_t *b = NULL;

    if (!m_vout || !m_eout) {
        rc = -1;
        errno = EINVAL;
        goto out;
    }
    if (!(b = emit_vtx_base (g, u, needs, exclusive))) {
        rc = -1;
        goto out;
    }
    if ((rc = map2json (b, g[u].properties, "properties") < 0)) {
        json_decref (b);
        goto out;
    }
    if ((rc = map2json (b, g[u].paths, "paths") < 0)) {
        json_decref (b);
        goto out;
    }
    if ((o = json_pack ("{s:s s:o}",
                            "id", std::to_string (g[u].uniq_id).c_str (),
                            "metadata", b)) == NULL) {
        json_decref (b);
        rc = -1;
        errno = ENOMEM;
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

    if (!m_eout || !m_eout) {
        rc = -1;
        errno = EINVAL;
        goto out;
    }
    if (!(m = json_object ())) {
        rc = -1;
        errno = ENOMEM;
        goto out;
    }
    if ((rc = emit_edg_meta (m, g, e)) < 0) {
        json_decref (m);
        goto out;
    }
    if (!(o = json_pack ("{s:s s:s s:o}",
                  "source", std::to_string (g[source (e, g)].uniq_id).c_str (),
                  "target", std::to_string (g[target (e, g)].uniq_id).c_str (),
                  "metadata", m))) {
        json_decref (m);
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

int jgf_match_writers_t::alloc_json_arrays ()
{
    int rc = -1;

    if (m_vout || m_eout)
        goto ret;
    if (!(m_vout = json_array ())) {
        errno = ENOMEM;
        goto ret;
    }
    if (!(m_eout = json_array ())) {
        json_decref (m_vout);
        errno = ENOMEM;
        goto ret;
    }
    rc = 0;
ret:
    return rc;
}

int jgf_match_writers_t::check_array_sizes ()
{
    int rc = 0;
    int s1 = 0;
    int s2 = 0;

    if (!m_vout || !m_eout) {
        rc = -1;
        errno = EINVAL;
        goto ret;
    }

    s1 = json_array_size (m_vout);
    s2 = json_array_size (m_eout);
    rc = s1 + s2;
    if (s1 == 0 && s2 == 0)
        goto ret;
    if (s1 == 0 || s2 == 0) {
        rc = -1;
        errno = ENOENT;
        goto ret;
    }
ret:
    return rc;
}

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
                            "size", static_cast<int64_t> (needs)))) {
        errno = ENOMEM;
    }
    return o;
}

int jgf_match_writers_t::map2json (json_t *o,
                                   const std::map<std::string, std::string> &mp,
                                   const char *key)
{
    int rc = 0;
    if (!mp.empty ()) {
        json_t *p = NULL;
        if (!(p = json_object ())) {
            rc = -1;
            errno = ENOMEM;
            goto out;
        }
        for (auto &kv : mp) {
            json_t *vo = NULL;
            if (!(vo = json_string (kv.second.c_str ()))) {
                json_decref (p);
                rc = -1;
                errno = ENOMEM;
                goto out;
            }
            if ((rc = json_object_set_new (p, kv.first.c_str (), vo)) == -1) {
                json_decref (p);
                errno = ENOMEM;
                goto out;
            }
        }
	if ((rc = json_object_set_new (o, key, p)) == -1) {
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
        rc = -1;
        errno = EINVAL;
        goto out;
    }
    if (!g[e].name.empty ()) {
        json_t *n = NULL;
        if (!(n = json_object ())) {
            rc = -1;
            errno = ENOMEM;
            goto out;
        }
        for (auto &kv : g[e].name) {
            json_t *vo = NULL;
            if (!(vo = json_string (kv.second.c_str ()))) {
                json_decref (n);
                rc = -1;
                errno = ENOMEM;
                goto out;
            }
            if ((rc = json_object_set_new (n, kv.first.c_str (), vo))) {
                json_decref (n);
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

rlite_match_writers_t::rlite_match_writers_t ()
{
    m_reducer["core"] = std::set<int64_t> ();
    m_reducer["gpu"] = std::set<int64_t> ();
    m_gatherer.insert ("node");
    if (!(m_out = json_array ()))
        throw std::bad_alloc ();
}

rlite_match_writers_t::rlite_match_writers_t (const rlite_match_writers_t &w)
{
    m_reducer = w.m_reducer;
    m_gatherer = w.m_gatherer;
    if (!(m_out = json_deep_copy (w.m_out)))
        throw std::bad_alloc ();
}

rlite_match_writers_t &rlite_match_writers_t::operator=(
                                                 const rlite_match_writers_t &w)
{
    m_reducer = w.m_reducer;
    m_gatherer = w.m_gatherer;
    if (!(m_out = json_deep_copy (w.m_out)))
        throw std::bad_alloc ();
    return *this;
}

rlite_match_writers_t::~rlite_match_writers_t ()
{
    json_decref (m_out);
}

bool rlite_match_writers_t::empty ()
{
    return (json_array_size (m_out) == 0)? true : false;
}

int rlite_match_writers_t::emit_json (json_t **o)
{
    int rc = 0;
    if (!m_out) {
        errno = EINVAL;
        rc = -1;
        goto ret;
    }
    if ((rc = json_array_size (m_out)) != 0) {
        *o = m_out;
        if (!(m_out = json_array ())) {
            json_decref (*o);
            *o = NULL;
            rc = -1;
            errno = ENOMEM;
            goto ret;
        }
    }
ret:
    return rc;
}

int rlite_match_writers_t::emit (std::stringstream &out, bool newline)
{
    int rc = 0;
    json_t *o = NULL;
    if ((rc = emit_json (&o)) > 0) {
        char *json_str = NULL;
        if (!(json_str = json_dumps (o, JSON_INDENT (0)))) {
            json_decref (o);
            rc = -1;
            errno = ENOMEM;
            goto ret;
        }
        out << json_str;
        if (newline)
            out << std::endl;
        free (json_str);
        json_decref (o);
     }
ret:
    return (rc == -1)? -1 : 0;
}

int rlite_match_writers_t::emit (std::stringstream &out)
{
    return emit (out, true);
}

int rlite_match_writers_t::emit_vtx (const std::string &prefix,
                                     const f_resource_graph_t &g,
                                     const vtx_t &u,
                                     unsigned int needs,
                                     bool exclusive)
{
    int rc = 0;

    if (!m_out) {
        rc = -1;
        errno = EINVAL;
        goto ret;
    }
    if (m_reducer.find (g[u].type) != m_reducer.end ()) {
        m_reducer[g[u].type].insert (g[u].id);
    } else if (m_gatherer.find (g[u].type) != m_gatherer.end ()) {
        if ((rc = emit_gatherer (g, u)) < 0)
            goto ret;
    }
ret:
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
        goto ret;
    if (!(co = json_object ())) {
        rc = -1;
        errno = ENOMEM;
        goto ret;
    }
    for (auto &kv : m_reducer) {
        if (kv.second.empty ())
            continue;
        std::stringstream s;
        compress (s, kv.second);
        json_t *vo = NULL;
        if (!(vo = json_string (s.str ().c_str ()))) {
            json_decref (co);
            rc = -1;
            errno = ENOMEM;
            goto ret;
        }
        if ((rc = json_object_set_new (co, kv.first.c_str (), vo)) < 0) {
            json_decref (co);
            errno = ENOMEM;
            goto ret;
        }
        kv.second.clear ();
    }
    if (!(o = json_pack ("{s:s s:s s:o}",
                             "rank", std::to_string (g[u].rank).c_str (),
                             "node", g[u].name.c_str (),
                             "children", co))) {
        json_decref (co);
        rc = -1;
        errno = ENOMEM;
        goto ret;
    }
    if ((rc = json_array_append_new (m_out, o)) != 0) {
        errno = ENOMEM;
        goto ret;
    }

ret:
    return rc;
}


/****************************************************************************
 *                                                                          *
 *                  RV1 Writers Class Method Definitions                    *
 *                                                                          *
 ****************************************************************************/

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

    if (rlite.empty () || jgf.empty ())
        goto ret;
    if ((rc = rlite.emit_json (&rlite_o)) < 0)
        goto ret;
    if ((rc = jgf.emit_json (&jgf_o)) < 0) {
        json_decref (rlite_o);
        goto ret;
    }
    if (!(o = json_pack ("{s:i s:{s:o s:I s:I} s:o}",
                             "version", 1,
                             "execution",
                             "R_lite", rlite_o,
                             "starttime", m_starttime,
                             "expiration", m_expiration,
                             "scheduling", jgf_o))) {
        json_decref (rlite_o);
        json_decref (jgf_o);
        rc = -1;
        errno = ENOMEM;
        goto ret;
    }
    if (!(json_str = json_dumps (o, JSON_INDENT (0)))) {
        json_decref (o);
        o = NULL;
        rc = -1;
        errno = ENOMEM;
        goto ret;
    }
    out << json_str << std::endl;
    free (json_str);
    json_decref (o);

ret:
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
        goto ret;
    if ((rc = rlite.emit_json (&rlite_o)) < 0)
        goto ret;
    if (!(o = json_pack ("{s:i s:{s:o s:I s:I}}",
                             "version", 1,
                             "execution",
                             "R_lite",  rlite_o,
                             "starttime", m_starttime,
                             "expiration", m_expiration))) {
        json_decref (rlite_o);
        rc = -1;
        errno = ENOMEM;
        goto ret;
    }
    if (!(json_str = json_dumps (o, JSON_INDENT (0)))) {
        json_decref (o);
        o = NULL;
        rc = -1;
        errno = ENOMEM;
        goto ret;
    }
    out << json_str << std::endl;
    free (json_str);
    json_decref (o);

ret:
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

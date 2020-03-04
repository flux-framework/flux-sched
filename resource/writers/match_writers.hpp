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

#ifndef MATCH_WRITERS_HPP
#define MATCH_WRITERS_HPP

#include <memory>
#include <string>
#include <sstream>
#include <set>

namespace Flux {
namespace resource_model {

enum class match_format_t { SIMPLE,
                            JGF,
                            RLITE,
                            RV1,
                            RV1_NOSCHED,
                            PRETTY_SIMPLE };


/*! Base match writers class for a matched resource set
 */
class match_writers_t {
public:
    virtual ~match_writers_t () {}
    virtual int initialize () {
        return 0;
    }
    virtual bool empty () = 0;
    virtual int emit (std::stringstream &out) = 0;
    virtual int emit_vtx (const std::string &prefix,
                          const f_resource_graph_t &g, const vtx_t &u,
                          unsigned int needs, bool exclusive) = 0;
    virtual int emit_edg (const std::string &prefix,
                          const f_resource_graph_t &g, const edg_t &e) {
        return 0;
    }
    virtual int emit_tm (uint64_t start_tm, uint64_t end_tm) {
        return 0;
    }
    void compress (std::stringstream &o, const std::set<int64_t> &ids);
};


/*! Simple match writers class for a matched resource set
 */
class sim_match_writers_t : public match_writers_t
{
public:
    virtual ~sim_match_writers_t () {}
    virtual bool empty ();
    virtual int emit (std::stringstream &out);
    virtual int emit_vtx (const std::string &prefix,
                          const f_resource_graph_t &g, const vtx_t &u,
                          unsigned int needs, bool exclusive);
private:
    std::stringstream m_out;
};


/*! JGF match writers class for a matched resource set
 */
class jgf_match_writers_t : public match_writers_t
{
public:
    virtual ~jgf_match_writers_t ();
    virtual int initialize ();
    virtual bool empty ();
    json_t *emit_json ();
    virtual int emit (std::stringstream &out);
    virtual int emit_vtx (const std::string &prefix,
                          const f_resource_graph_t &g, const vtx_t &u,
                          unsigned int needs, bool exclusive);
    virtual int emit_edg (const std::string &prefix,
                          const f_resource_graph_t &g, const edg_t &e);
private:
    json_t *emit_vtx_base (const f_resource_graph_t &g, const vtx_t &u,
                           unsigned int needs, bool exclusive);
    int emit_vtx_prop (json_t *o, const f_resource_graph_t &g,
                       const vtx_t &u, unsigned int needs, bool exclusive);
    int emit_vtx_path (json_t *o, const f_resource_graph_t &g,
                       const vtx_t &u, unsigned int needs, bool exclusive);
    int emit_edg_meta (json_t *o, const f_resource_graph_t &g, const edg_t &e);

    json_t *m_vout = NULL;
    json_t *m_eout = NULL;
};


/*! RLITE match writers class for a matched resource set
 */
class rlite_match_writers_t : public match_writers_t
{
public:
    virtual ~rlite_match_writers_t ();
    virtual int initialize ();
    virtual bool empty ();
    json_t *emit_json ();
    virtual int emit (std::stringstream &out);
    virtual int emit_vtx (const std::string &prefix,
                          const f_resource_graph_t &g, const vtx_t &u,
                          unsigned int needs, bool exclusive);
private:
    bool m_reducer_set ();
    int emit_gatherer (const f_resource_graph_t &g, const vtx_t &u);

    std::map<std::string, std::set<int64_t>> m_reducer;
    std::set<std::string> m_gatherer;
    json_t *m_out = NULL;
};


/*! R Version 1 match writers class for a matched resource set
 */
class rv1_match_writers_t : public match_writers_t
{
public:
    virtual int initialize ();
    virtual bool empty ();
    virtual int emit (std::stringstream &out);
    virtual int emit_vtx (const std::string &prefix,
                          const f_resource_graph_t &g, const vtx_t &u,
                          unsigned int needs, bool exclusive);
    virtual int emit_edg (const std::string &prefix,
                          const f_resource_graph_t &g, const edg_t &e);
    virtual int emit_tm (uint64_t start_tm, uint64_t end_tm);
private:
    rlite_match_writers_t rlite;
    int64_t m_starttime = 0;
    int64_t m_expiration = 0;
    jgf_match_writers_t jgf;
};


/*! R Version 1 with no "scheduling" key match writers class
 */
class rv1_nosched_match_writers_t : public match_writers_t
{
public:
    virtual int initialize ();
    virtual bool empty ();
    virtual int emit (std::stringstream &out);
    virtual int emit_vtx (const std::string &prefix,
                          const f_resource_graph_t &g, const vtx_t &u,
                          unsigned int needs, bool exclusive);
    virtual int emit_tm (uint64_t start_tm, uint64_t end_tm);
private:
    rlite_match_writers_t rlite;
    int64_t m_starttime = 0;
    int64_t m_expiration = 0;
};


/*! Human-friendly simple match writers class for a matched resource set
 */
class pretty_sim_match_writers_t : public match_writers_t
{
public:
    virtual bool empty ();
    virtual int emit (std::stringstream &out);
    virtual int emit_vtx (const std::string &prefix,
                          const f_resource_graph_t &g, const vtx_t &u,
                          unsigned int needs, bool exclusive);
private:
    std::list<std::string> m_out;
};


/*! Match writer factory-method class
 */
class match_writers_factory_t {
public:
    static match_format_t get_writers_type (const std::string &n);
    static std::shared_ptr<match_writers_t> create (match_format_t f);
};

bool known_match_format (const std::string &format);

} // namespace resource_model
} // namespace Flux

#endif // MATCH_WRITERS_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

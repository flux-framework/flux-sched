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
                            PRETTY_SIMPLE,
                            PRETTY_JGF,
                            PRETTY_RLITE,
                            PRETTY_RV1 };


/*! Base match writers class for a matched resource set
 */
class match_writers_t {
public:
    virtual ~match_writers_t () {}
    virtual void emit (std::stringstream &out) = 0;
    virtual void reset () = 0;
    virtual void emit_vtx (const std::string &prefix,
                           const f_resource_graph_t &g, const vtx_t &u,
                           unsigned int needs, bool exclusive) = 0;
    virtual void emit_edg (const std::string &prefix,
                           const f_resource_graph_t &g, const edg_t &e) {}
    void compress (std::stringstream &o, const std::set<int64_t> &ids);
};


/*! Simple match writers class for a matched resource set
 */
class sim_match_writers_t : public match_writers_t
{
public:
    virtual ~sim_match_writers_t () {}
    virtual void emit (std::stringstream &out);
    virtual void reset ();
    virtual void emit_vtx (const std::string &prefix,
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
    virtual ~jgf_match_writers_t () {}
    virtual void reset ();
    virtual void emit (std::stringstream &out, bool newline);
    virtual void emit (std::stringstream &out);
    virtual void emit_vtx (const std::string &prefix,
                           const f_resource_graph_t &g, const vtx_t &u,
                           unsigned int needs, bool exclusive);
    virtual void emit_edg (const std::string &prefix,
                           const f_resource_graph_t &g, const edg_t &e);
private:
    std::stringstream m_vout;
    std::stringstream m_eout;
};


/*! RLITE match writers class for a matched resource set
 */
class rlite_match_writers_t : public match_writers_t
{
public:
    rlite_match_writers_t ();
    virtual ~rlite_match_writers_t ();
    virtual void reset ();
    virtual void emit (std::stringstream &out, bool newline);
    virtual void emit (std::stringstream &out);
    virtual void emit_vtx (const std::string &prefix,
                           const f_resource_graph_t &g, const vtx_t &u,
                           unsigned int needs, bool exclusive);
private:
    bool m_reducer_set ();
    std::map<std::string, std::set<int64_t>> m_reducer;
    std::map<std::string, std::stringstream *> m_gatherer;
    std::stringstream m_out;
};


/*! R Version 1 match writers class for a matched resource set
 */
class rv1_match_writers_t : public match_writers_t
{
public:
    virtual void reset ();
    virtual void emit (std::stringstream &out);
    virtual void emit_vtx (const std::string &prefix,
                           const f_resource_graph_t &g, const vtx_t &u,
                           unsigned int needs, bool exclusive);
    virtual void emit_edg (const std::string &prefix,
                           const f_resource_graph_t &g, const edg_t &e);
private:
    rlite_match_writers_t rlite;
    jgf_match_writers_t jgf;
};


/*! R Version 1 with no "scheduling" key match writers class
 */
class rv1_nosched_match_writers_t : public match_writers_t
{
public:
    virtual void reset ();
    virtual void emit (std::stringstream &out);
    virtual void emit_vtx (const std::string &prefix,
                           const f_resource_graph_t &g, const vtx_t &u,
                           unsigned int needs, bool exclusive);
private:
    rlite_match_writers_t rlite;
};


/*! Human-friendly simple match writers class for a matched resource set
 */
class pretty_sim_match_writers_t : public match_writers_t
{
public:
    virtual void emit (std::stringstream &out);
    virtual void reset ();
    virtual void emit_vtx (const std::string &prefix,
                           const f_resource_graph_t &g, const vtx_t &u,
                           unsigned int needs, bool exclusive);
private:
    std::list<std::string> m_out;
};


/*! Human-friendly JGF match writers class for a matched resource set
 */
class pretty_jgf_match_writers_t : public match_writers_t
{
public:
    virtual void reset ();
    virtual void emit (std::stringstream &out);
    virtual void emit_vtx (const std::string &prefix,
                           const f_resource_graph_t &g, const vtx_t &u,
                           unsigned int needs, bool exclusive);
    virtual void emit_edg (const std::string &prefix,
                           const f_resource_graph_t &g, const edg_t &e);
private:
    std::stringstream m_vout;
    std::stringstream m_eout;
};


/*! Human-friendly RLite match writers class for a matched resource set
 */
class pretty_rlite_match_writers_t : public match_writers_t
{
public:
    pretty_rlite_match_writers_t ();
    virtual ~pretty_rlite_match_writers_t ();
    virtual void reset ();
    virtual void emit (std::stringstream &out);
    virtual void emit_vtx (const std::string &prefix,
                           const f_resource_graph_t &g, const vtx_t &u,
                           unsigned int needs, bool exclusive);
private:
    bool m_reducer_set ();
    std::map<std::string, std::set<int64_t>> m_reducer;
    std::map<std::string, std::stringstream *> m_gatherer;
    std::stringstream m_out;
};


/*! Human-friendly R Version 1 match writers class for a matched resource set
 */
class pretty_rv1_match_writers_t : public match_writers_t
{
public:
    virtual void reset ();
    virtual void emit (std::stringstream &out);
    virtual void emit_vtx (const std::string &prefix,
                           const f_resource_graph_t &g, const vtx_t &u,
                           unsigned int needs, bool exclusive);
    virtual void emit_edg (const std::string &prefix,
                           const f_resource_graph_t &g, const edg_t &e);
private:
    pretty_rlite_match_writers_t rlite;
    pretty_jgf_match_writers_t jgf;
};


/*! Match writer factory-method class
 */
class match_writers_factory_t {
public:
    static match_format_t get_writers_type (const std::string &n);
    static match_writers_t *create (match_format_t f);
};

bool known_match_format (const std::string &format);

} // namespace resource_model
} // namespace Flux

#endif // MATCH_WRITERS_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

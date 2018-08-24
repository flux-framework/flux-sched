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

#include <iostream>
#include <cstdlib>
#include "resource/traversers/dfu.hpp"

extern "C" {
#if HAVE_CONFIG_H
#include "config.h"
#endif
}

using namespace std;
using namespace Flux::resource_model;
using namespace Flux::resource_model::detail;
using namespace Flux::Jobspec;

/****************************************************************************
 *                                                                          *
 *                    DFU Traverser Private API Definitions                 *
 *                                                                          *
 ****************************************************************************/

int dfu_traverser_t::schedule (Jobspec::Jobspec &jobspec,
                               detail::jobmeta_t &meta, bool x, match_op_t op,
                               vtx_t root, unsigned int *needs,
                               std::unordered_map<string, int64_t> &dfv)
{
    int rc = -1;
    const subsystem_t &dom = get_match_cb ()->dom_subsystem ();

    /* Allocate */
    rc = detail::dfu_impl_t::select (jobspec, root, meta, x, needs);
    if ((rc != 0) && (op == match_op_t::MATCH_ALLOCATE_ORELSE_RESERVE)) {
        /* Or else reserve */
        meta.allocate = false;
        int64_t t = meta.at + 1;
        vector<uint64_t> agg;
        planner_multi_t *p = (*get_graph ())[root].idata.subplans.at (dom);
        size_t len = planner_multi_resources_len (p);
        uint64_t duration = meta.duration;
        detail::dfu_impl_t::count (p, dfv, agg);
        // TODO: examine correctness when a jobspec doesn't include
        // the subtree plan resource type
        for (t = planner_multi_avail_time_first (p, t, duration, &agg[0], len);
             (t != -1 && rc != 0); t = planner_multi_avail_time_next (p)) {
            meta.at = t;
            rc = detail::dfu_impl_t::select (jobspec, root, meta, x, needs);
        }
    }
    return rc;
}


/****************************************************************************
 *                                                                          *
 *                    DFU Traverser Public API Definitions                  *
 *                                                                          *
 ****************************************************************************/

dfu_traverser_t::dfu_traverser_t ()
{

}

dfu_traverser_t::dfu_traverser_t (f_resource_graph_t *g, dfu_match_cb_t *m,
                                  map<subsystem_t, vtx_t> *roots)
    : detail::dfu_impl_t (g, m, roots)
{

}

dfu_traverser_t::dfu_traverser_t (const dfu_traverser_t &o)
    : detail::dfu_impl_t (o)
{

}

dfu_traverser_t &dfu_traverser_t::operator= (const dfu_traverser_t &o)
{
    detail::dfu_impl_t::operator= (o);
    return *this;
}

dfu_traverser_t::~dfu_traverser_t ()
{

}

const f_resource_graph_t *dfu_traverser_t::get_graph () const
{
   return detail::dfu_impl_t::get_graph ();
}

const map<subsystem_t, vtx_t> *dfu_traverser_t::get_roots () const
{
    return detail::dfu_impl_t::get_roots ();
}

const dfu_match_cb_t *dfu_traverser_t::get_match_cb () const
{
    return detail::dfu_impl_t::get_match_cb ();
}

const string &dfu_traverser_t::err_message () const
{
    return detail::dfu_impl_t::err_message ();
}

void dfu_traverser_t::set_graph (f_resource_graph_t *g)
{
    detail::dfu_impl_t::set_graph (g);
}

void dfu_traverser_t::set_roots (map<subsystem_t, vtx_t> *roots)
{
    detail::dfu_impl_t::set_roots (roots);
}

void dfu_traverser_t::set_match_cb (dfu_match_cb_t *m)
{
    detail::dfu_impl_t::set_match_cb (m);
}

void dfu_traverser_t::clear_err_message ()
{
    detail::dfu_impl_t::clear_err_message ();
}

int dfu_traverser_t::initialize ()
{
    int rc = 0;
    vtx_t root;
    if (!get_graph () || !get_roots () || !get_match_cb ()) {
        errno = EINVAL;
        return -1;
    }

    for (auto &subsystem : get_match_cb ()->subsystems ()) {
        map<string, int64_t> from_dfv;
        if (get_roots ()->find (subsystem) == get_roots ()->end ()) {
            errno = ENOTSUP;
            rc = -1;
            break;
        }
        root = get_roots ()->at(subsystem);
        rc += detail::dfu_impl_t::prime (subsystem, root, from_dfv);
    }
    return rc;
}

int dfu_traverser_t::initialize (f_resource_graph_t *g,
                                 map<subsystem_t, vtx_t> *roots,
                                 dfu_match_cb_t *m)
{
    set_graph (g);
    set_roots (roots);
    set_match_cb (m);
    return initialize ();
}

int dfu_traverser_t::run (Jobspec::Jobspec &jobspec, match_op_t op,
                          int64_t jobid, int64_t *at, stringstream &ss)
{
    const subsystem_t &dom = get_match_cb ()->dom_subsystem ();
    if (!get_graph () || !get_roots ()
        || get_roots ()->find (dom) == get_roots ()->end ()
        || !get_match_cb () || jobspec.resources.empty ()) {
        errno = EINVAL;
        return -1;
    }

    int rc = -1;
    detail::jobmeta_t meta;
    unsigned int needs = 0;
    vtx_t root = get_roots ()->at(dom);
    bool x = detail::dfu_impl_t::exclusivity (jobspec.resources, root);
    std::unordered_map<string, int64_t> dfv;
    detail::dfu_impl_t::prime (jobspec.resources, dfv);
    meta.build (jobspec, true, jobid, *at);
    if ( (rc = schedule (jobspec, meta, x, op, root, &needs, dfv)) ==  0) {
        *at = meta.at;
        rc = detail::dfu_impl_t::update (root, meta, needs, x, ss);
    }
    return rc;
}

int dfu_traverser_t::remove (int64_t jobid)
{
    const subsystem_t &dom = get_match_cb ()->dom_subsystem ();
    if (!get_graph () || !get_roots ()
        || get_roots ()->find (dom) == get_roots ()->end ()
        || !get_match_cb ()) {
        errno = EINVAL;
        return -1;
    }

    vtx_t root = get_roots ()->at(dom);
    return detail::dfu_impl_t::remove (root, jobid);
}

namespace Flux {
namespace resource_model {

bool known_R_format (const string &f)
{
    bool rc = true;
    if (f != R_FORMAT || f != R_LITE_FORMAT || f != R_NATIVE_FORMAT)
        rc = false;
    return rc;
}
}
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

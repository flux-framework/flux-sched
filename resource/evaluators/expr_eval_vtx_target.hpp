/*****************************************************************************\
 *  Copyright (c) 2020 Lawrence Livermore National Security, LLC.  Produced at
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

#ifndef EXPR_EVAL_VTX_TARGET_HPP
#define EXPR_EVAL_VTX_TARGET_HPP

#include <string>
#include <memory>
#include "resource/schema/resource_graph.hpp"
#include "resource/evaluators/expr_eval_target.hpp"

namespace Flux {
namespace resource_model {

/*! Assist expr_eval_vtx_target_t with an ability to override
 *  certain predicates associated with a resource vertex.
 *  Certain vertex predicates cannot be determined
 *  only by inspecting the vertex's own state, and this class
 *  helps override those predicates.
 *  Example: if the status of the parent resource vertex (e.g.,
 *  compute node vertex) is "down", the status of a child vertex (e.g.,
 *  compute core vertex) must also be "down" even if its status
 *  was not explictly marked "down".
 */
struct vtx_predicates_override_t {
    bool status_down{false};
    bool sched_now_allocated{false};
    bool sched_future_reserved{false};
    void set (bool sd, bool sna, bool sfr);
};


/*! Expression evaluation resource vertex target class.
 *  Each resource vertex defines a set of predicates that
 *  return true or false based on its current state.
 *  This adaptor class enables us to evaluate those predicates
 *  without having to modify the resource vertex data types themselves.
 *  It is derived from expr_eval_target_base_t and implements
 *  its validate and evaluate interfaces to do this work.
 */
class expr_eval_vtx_target_t : public expr_eval_target_base_t {
public:

    /*! Validate if a predicate expression is valid w/ respect to
     *  a resource vertex.
     *  As a predicate is often denoted as p(x), the method parameters
     *  follow that convention: i.e., p for predicate name and x
     *  for the input to the predicate.
     *
     *  \param p         predicate name
     *  \param x         input to the predicate
     *  \return          0 on success; -1 on error
     */
    virtual int validate (const std::string &p, const std::string &x) const;

    /*! Evaluate if predicate p(x) is true or false on a resource vertex.
     *  As a predicate is often denoted as p(x), the method parameters
     *  follow that convention: i.e., p for predicate name and x
     *  for the input to the predicate.
     *
     *  \param p         predicate name
     *  \param x         input to the predicate
     *  \param result    return true or false as p(x) is evaluated on
     *                   this vertex expression evaluation target.
     *  \return          0 on success; -1 on error
     */
    virtual int evaluate (const std::string &p,
                          const std::string &x, bool &result) const;

    /*! Initialize the object of this class with a resource vertex.
     *  This must be called before the validate and evaluate interfaces
     *  can be used.
     *
     *  \param p         overriden predicates (see the description
     *                   of vtx_predicates_override_t above)
     *  \param g         shared pointer pointing to a filtered resource graph
     *  \param u         vertex. g[u] must return the corresponding state.
     *  \return          0 on success; -1 on error
     */
    void initialize (const vtx_predicates_override_t &p,
                     const std::shared_ptr<
                         const f_resource_graph_t> g, vtx_t u);

    /*! Is this object initialized?
     */
    bool is_initialized () const;

private:
    bool m_initialized{false};
    vtx_predicates_override_t m_overriden;
    std::shared_ptr<const f_resource_graph_t> m_g;
    vtx_t m_u;
};

} // resource_model
} // Flux

#endif // EXPR_EVAL_VTX_TARGET_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

/*****************************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef DFU_MATCH_MULTILEVEL_ID_HPP
#define DFU_MATCH_MULTILEVEL_ID_HPP

#include <vector>
#include <unordered_map>
#include "resource/policies/base/dfu_match_cb.hpp"

namespace Flux {
namespace resource_model {

/*! MultiLevel ID-based match policy: select each
 *  resource using a user-defined function of numeric IDs
 *  of their containing high-level resources and of itself.
 *  Templated on a binary functor in the fold namespace
 *  (e.g., fold::less, fold::greater etc).
 *  This policy can be used to incorporate one or more
 *  containing (or ancestor) resources into each contained
 *  resource.
 */
template<typename FOLD>
class multilevel_id_t : public dfu_match_cb_t {
   public:
    multilevel_id_t ();
    multilevel_id_t (const std::string &name);
    multilevel_id_t (const multilevel_id_t &o);
    multilevel_id_t &operator= (const multilevel_id_t &o);
    ~multilevel_id_t ();

    /*! Please see its overriding method within
     *  dfu_match_cb_t@base/dfu_match_cb.hpp
     */
    int dom_finish_graph (subsystem_t subsystem,
                          const std::vector<Flux::Jobspec::Resource> &resources,
                          const resource_graph_t &g,
                          scoring_api_t &dfu) override;

    /*! Please see its overriding method within
     *  dfu_match_cb_t@base/dfu_match_cb.hpp
     */
    int dom_finish_slot (subsystem_t subsystem, scoring_api_t &dfu) override;

    /*! Please see its overriding method within
     *  dfu_match_cb_t@base/dfu_match_cb.hpp
     */
    int dom_discover_vtx (vtx_t u,
                          subsystem_t subsystem,
                          const std::vector<Flux::Jobspec::Resource> &resources,
                          const resource_graph_t &g) override;

    /*! Please see its overriding method within
     *  dfu_match_cb_t@base/dfu_match_cb.hpp
     */
    int dom_finish_vtx (vtx_t u,
                        subsystem_t subsystem,
                        const std::vector<Flux::Jobspec::Resource> &resources,
                        const resource_graph_t &g,
                        scoring_api_t &dfu,traverser_match_kind_t sm) override;

    /*! Please see its overriding method within
     *  dfu_match_cb_t@base/dfu_match_cb.hpp
     */
    int set_stop_on_k_matches (unsigned int k) override;

    /*! Please see its overriding method within
     *  dfu_match_cb_t@base/dfu_match_cb.hpp
     */
    int get_stop_on_k_matches () const override;

    /*!
     * Add a high-level resource type to the multilevel factor
     * vector. The preorder visit callback (dom_discover_vtx()) then
     * calculates the factor of the visiting resource vertex
     * of this type and adds that to multilevel score vector:
     * (the numeric ID of the visiting resource
     * of the added type + add_by) * multiply_by.
     * This method may be called multiple times to add multiple
     * high-level resource types to the score factors vector.
     *
     *  \param type      high-level resource type to add to
     *                   the multilevel factors.
     *  \param add_by    unsigned integer to add to the ID of each
     *                   resource of the added type.
     *  \param multiply_by
     *                   unsigned integer to multiply with (ID + add_by)
     *
     *  \return          return 0 on success; -1 on error with errno set:
     *                       EEXIST: type already has been added;
     *                       ENOMEM: out of memory.
     */
    int add_score_factor (const resource_type_t type, unsigned add_by, unsigned multiply_by);

   private:
    class score_factor_t {
       public:
        score_factor_t () = default;
        score_factor_t (resource_type_t type, unsigned int add_by, unsigned int multiply_by);
        score_factor_t (const score_factor_t &o) = default;
        int64_t calc_factor (int64_t base_factor, int64_t break_tie);
        int64_t m_factor = 0;

       private:
        resource_type_t m_type;
        unsigned int m_add_by = 0;
        unsigned int m_multiply_by = 1;
    };

    void set_base_factor (resource_type_t type, unsigned int id);

    FOLD m_comp;
    unsigned m_stop_on_k_matches = 0;
    int64_t m_multilevel_scores = 0;
    std::unordered_map<resource_type_t, score_factor_t> m_multilevel_factors;
};

}  // namespace resource_model
}  // namespace Flux

#endif  // DFU_MATCH_MULTILEVEL_ID_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

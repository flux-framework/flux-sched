/*****************************************************************************\
 * Copyright 2025 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef DFU_FLEXIBLE_HPP
#define DFU_FLEXIBLE_HPP

#include "resource/traversers/dfu_impl.hpp"

using namespace Flux::resource_model::detail;
namespace Flux {
namespace resource_model {

using ResourceList = std::vector<Flux::Jobspec::Resource>;

class dfu_flexible_t : public dfu_impl_t {
    // struct to convert map of resources counts to an index
   public:
    struct Key {
        Key () = default;
        Key (const std::map<resource_type_t, int> &c)
        {
            counts = c;
        };

        std::map<resource_type_t, int> counts;

        bool operator== (const Key &other) const
        {
            if (counts.size () != other.counts.size ())
                return false;

            for (auto &it : counts) {
                if (it.second != other.counts.at (it.first))
                    return false;
            }
            return true;
        }
    };

    // Custom hashing function for resource counts map
    struct Hash {
        std::size_t operator() (const Key &k) const
        {
            std::size_t seed = 0;
            for (auto &it : k.counts) {
                boost::hash_combine (seed, it.second);
            }
            return seed;
        }
    };

    int match (vtx_t u,
               const std::vector<Jobspec::Resource> &resources,
               unsigned int *nslots,
               const Jobspec::Resource **match_resource,
               const std::vector<Jobspec::Resource> **slot_resources);
    const std::vector<Jobspec::Resource> &test (vtx_t u,
                                                const std::vector<Jobspec::Resource> &resources,
                                                bool &pristine,
                                                unsigned int &nslots,
                                                match_kind_t &ko);
    std::tuple<dfu_flexible_t::Key, int, int> select_or_config (
        const std::vector<Jobspec::Resource> &slots,
        const std::map<resource_type_t, int> &resource_counts,
        unsigned int nslots,
        std::unordered_map<Key, std::tuple<std::map<resource_type_t, int>, int, int>, Hash>
            &or_config);

    int select (Jobspec::Jobspec &j, vtx_t root, jobmeta_t &meta, bool excl);

    /*! Find min count if type matches with one of the resource
     *  types used in the scheduler-driven aggregate update (SDAU) scheme.
     *  dfu_match_cb_t provides an interface to configure what types are used
     *  for SDAU scheme.
     */
    int min_if (subsystem_t subsystem,
                resource_type_t type,
                unsigned int count,
                std::unordered_map<resource_type_t, int64_t> &lowest);

    int dom_slot (const jobmeta_t &meta,
                  vtx_t u,
                  const std::vector<Jobspec::Resource> &resources,
                  unsigned int nslots,
                  bool pristine,
                  bool *excl,
                  scoring_api_t &dfu);

    /*! Prime the resource section of the jobspec. Aggregate configured
     *  subtree resources into jobspec's user_data.  For example,
     *  cluster[1]->rack[2]->node[4]->socket[1]->core[2]
     *  with socket and core types configured to be tracked will be augmented
     *  at the end of priming as:
     *      cluster[1](core:16)->rack[2](core:8)->node[4](core:2)->
     *          socket[1](core:2)->core[2]
     *
     *  The subtree aggregate information is used to prune unnecessary
     *  graph traversals
     *
     *  \param resources Resource request vector.
     *  \param[out] to_parent
     *                   output aggregates on the subtree.
     *  \return          none.
     */
    void prime_jobspec (std::vector<Jobspec::Resource> &resources,
                        std::unordered_map<resource_type_t, int64_t> &to_parent);

    std::vector<ResourceList> split_xor_slots (const ResourceList &resources);

    int prune_resources (const jobmeta_t &meta,
                         bool excl,
                         subsystem_t subsystem,
                         vtx_t u,
                         const std::vector<Jobspec::Resource> &resources);

   public:
    dfu_flexible_t ();
    dfu_flexible_t (std::shared_ptr<resource_graph_db_t> db, std::shared_ptr<dfu_match_cb_t> m);
    dfu_flexible_t (const dfu_flexible_t &o);
    dfu_flexible_t (dfu_flexible_t &&o);
    dfu_flexible_t &operator= (const dfu_flexible_t &o);
    dfu_flexible_t &operator= (dfu_flexible_t &&o);
    ~dfu_flexible_t ();
};

}  // namespace resource_model
}  // namespace Flux

#endif  // DFU_TRAVERSE_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

/*****************************************************************************\
 * Copyright 2019 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef RESOURCE_READER_JGF_HPP
#define RESOURCE_READER_JGF_HPP

#include <string>
#include <unordered_set>
#include <jansson.h>
#include "resource/schema/resource_graph.hpp"
#include "resource/readers/resource_reader_base.hpp"

struct vmap_val_t;

namespace Flux {
namespace resource_model {

class fetch_remap_support_t {
   public:
    int64_t get_remapped_id () const;
    int64_t get_remapped_rank () const;
    const std::string &get_remapped_name () const;
    void set_remapped_id (int64_t i);
    void set_remapped_rank (int64_t r);
    void set_remapped_name (const std::string &n);
    bool is_name_remapped () const;
    bool is_id_remapped () const;
    bool is_rank_remapped () const;
    void clear ();

   private:
    int64_t m_remapped_id = -1;
    int64_t m_remapped_rank = -1;
    std::string m_remapped_name = "";
};

struct fetch_helper_t : public fetch_remap_support_t {
    const char *get_proper_name () const;
    int64_t get_proper_id () const;
    int64_t get_proper_rank () const;
    void scrub ();

    int64_t id = -2;
    int64_t rank = -1;
    int64_t size = -1;
    int64_t uniq_id = -1;
    int exclusive = -1;
    resource_pool_t::status_t status = resource_pool_t::status_t::UP;
    const char *type = NULL;
    std::string name;
    const char *unit = NULL;
    const char *basename = NULL;
    const char *vertex_id = NULL;
    std::map<std::string, std::string> properties;
    std::map<subsystem_t, std::string> paths;
};

struct rank_data {
    // store counts of resources to be cancelled
    std::unordered_map<resource_type_t, int64_t> type_to_count;
    // store length of paths to determine subgraph roots
    uint32_t length = std::numeric_limits<uint32_t>::max ();
    // store root vertex of subgraph for this rank
    vtx_t root = boost::graph_traits<resource_graph_t>::null_vertex ();
};

// Struct to track data for updates
struct jgf_updater_data {
    int64_t jobid = 0;
    int64_t at = 0;
    uint64_t duration = 0;
    bool reserved = false;
    // track data of resources to be cancelled
    std::unordered_map<int64_t, rank_data> rank_to_data;
    // track count of rank vertices to determine if rank
    // should be removed from by_rank map
    std::unordered_set<int64_t> ranks;
    // track vertices that are skipped because their ranks are freed
    std::unordered_set<int64_t> skip_vertices;
    bool update = true;        // Updating or partial cancel
    bool isect_ranks = false;  // Updating with partial_ok; intersecting with ranks key
    bool skipped = false;
};

/*! JGF resource reader class.
 */
class resource_reader_jgf_t : public resource_reader_base_t {
   public:
    virtual ~resource_reader_jgf_t ();

    /*! Unpack str into a resource graph.
     *
     * \param g      resource graph
     * \param m      resource graph meta data
     * \param str    string containing a JGF specification
     * \param rank   assign rank to all of the newly created resource vertices
     * \return       0 on success; non-zero integer on an error
     *                   ENOMEM: out of memory
     *                   EINVAL: invalid input or operation
     */
    virtual int unpack (resource_graph_t &g,
                        resource_graph_metadata_t &m,
                        const std::string &str,
                        int rank = -1);

    /*! Unpack str into a resource graph and graft
     *  the top-level vertices to vtx.
     *
     * \param g      resource graph
     * \param m      resource graph meta data
     * \param vtx    parent vtx at which to graft the deserialized graph
     * \param str    string containing a JGF specification
     * \param rank   assign this rank to all the newly created resource vertices
     * \return       -1 with errno=ENOTSUP (Not supported yet)
     */
    virtual int unpack_at (resource_graph_t &g,
                           resource_graph_metadata_t &m,
                           vtx_t &vtx,
                           const std::string &str,
                           int rank = -1);

    /*! Update resource graph g with str.
     *
     * \param g      resource graph
     * \param m      resource graph meta data
     * \param str    resource set string
     * \param jobid  jobid of str
     * \param at     start time of this job
     * \param dur    duration of this job
     * \param rsv    true if this update is for a reservation.
     * \param trav_token
     *               token to be used by traverser
     * \return       0 on success; non-zero integer on an error
     */
    virtual int update (resource_graph_t &g,
                        resource_graph_metadata_t &m,
                        const std::string &str,
                        int64_t jobid,
                        int64_t at,
                        uint64_t dur,
                        bool rsv,
                        uint64_t trav_token);

    /*! Partial cancellation of jobid based on R.
     *
     * \param g      resource graph
     * \param m      resource graph meta data
     * \param mod_data struct containing resource types to counts, mod type,
     *                 and set of ranks removed
     * \param R    resource set string
     * \param jobid  jobid of str
     * \return       0 on success; non-zero integer on an error
     */
    virtual int partial_cancel (resource_graph_t &g,
                                resource_graph_metadata_t &m,
                                modify_data_t &mod_data,
                                const std::string &R,
                                int64_t jobid);

    /*! Is the selected reader format support allowlist
     *
     * \return       false
     */
    virtual bool is_allowlist_supported ();

   protected:
    int apply_defaults (fetch_helper_t &f, const char *name);
    int find_vtx (resource_graph_t &g,
                  resource_graph_metadata_t &m,
                  std::map<std::string, vmap_val_t> &vmap,
                  const fetch_helper_t &fetcher,
                  vtx_t &ret_v);

   private:
    int fetch_jgf (const std::string &str,
                   json_t **jgf_p,
                   json_t **nodes_p,
                   json_t **edges_p,
                   jgf_updater_data &update_data);
    int unpack_and_remap_vtx (fetch_helper_t &f, json_t *paths, json_t *properties);
    int remap_aware_unpack_vtx (fetch_helper_t &f, json_t *paths, json_t *properties);
    int fill_fetcher (json_t *element, fetch_helper_t &f, json_t **path, json_t **properties);
    int unpack_vtx (json_t *element, fetch_helper_t &f);
    vtx_t create_vtx (resource_graph_t &g, const fetch_helper_t &fetcher);
    vtx_t vtx_in_graph (const resource_graph_t &g,
                        const resource_graph_metadata_t &m,
                        const std::map<subsystem_t, std::string> &paths,
                        int rank);
    bool is_root (const std::string &path);
    int check_root (vtx_t v, resource_graph_t &g, std::map<subsystem_t, bool> &is_roots);
    int add_graph_metadata (vtx_t v, resource_graph_t &g, resource_graph_metadata_t &m);
    int remove_graph_metadata (vtx_t v, resource_graph_t &g, resource_graph_metadata_t &m);
    int remove_metadata_outedges (vtx_t source_vertex,
                                  vtx_t dest_vertex,
                                  resource_graph_t &g,
                                  resource_graph_metadata_t &m);
    int update_vmap (std::map<std::string, vmap_val_t> &vmap,
                     vtx_t v,
                     const std::map<subsystem_t, bool> &root_checks,
                     const fetch_helper_t &fetcher);
    int add_vtx (resource_graph_t &g,
                 resource_graph_metadata_t &m,
                 std::map<std::string, vmap_val_t> &vmap,
                 const fetch_helper_t &fetcher);
    int exist (resource_graph_t &g,
               resource_graph_metadata_t &m,
               const std::string &path,
               int rank,
               const std::string &vid,
               vtx_t &v);
    int update_vtx_plan (vtx_t v,
                         resource_graph_t &g,
                         const fetch_helper_t &fetcher,
                         jgf_updater_data &update_data);
    int cancel_vtx (vtx_t v,
                    resource_graph_t &g,
                    resource_graph_metadata_t &m,
                    const fetch_helper_t &fetcher,
                    jgf_updater_data &update_data);
    int update_vtx (resource_graph_t &g,
                    resource_graph_metadata_t &m,
                    std::map<std::string, vmap_val_t> &vmap,
                    const fetch_helper_t &fetcher,
                    jgf_updater_data &updater_data);
    int unpack_vertices (resource_graph_t &g,
                         resource_graph_metadata_t &m,
                         std::map<std::string, vmap_val_t> &vmap,
                         json_t *nodes,
                         std::unordered_set<std::string> &added_vtcs);
    int undo_vertices (resource_graph_t &g,
                       std::map<std::string, vmap_val_t> &vmap,
                       jgf_updater_data &updater_data);
    int update_vertices (resource_graph_t &g,
                         resource_graph_metadata_t &m,
                         std::map<std::string, vmap_val_t> &vmap,
                         json_t *nodes,
                         jgf_updater_data &updater_data);
    int unpack_edge (json_t *element,
                     std::map<std::string, vmap_val_t> &vmap,
                     std::string &source,
                     std::string &target,
                     std::string &subsystem,
                     jgf_updater_data &update_data);
    int update_src_edge (resource_graph_t &g,
                         resource_graph_metadata_t &m,
                         std::map<std::string, vmap_val_t> &vmap,
                         std::string &source,
                         uint64_t token);
    int update_tgt_edge (resource_graph_t &g,
                         resource_graph_metadata_t &m,
                         std::map<std::string, vmap_val_t> &vmap,
                         std::string &source,
                         std::string &target,
                         uint64_t token);
    int unpack_edges (resource_graph_t &g,
                      resource_graph_metadata_t &m,
                      std::map<std::string, vmap_val_t> &vmap,
                      json_t *edges,
                      const std::unordered_set<std::string> &added_vtcs);
    int update_edges (resource_graph_t &g,
                      resource_graph_metadata_t &m,
                      std::map<std::string, vmap_val_t> &vmap,
                      json_t *edges,
                      uint64_t token,
                      jgf_updater_data &update_data);
    int get_subgraph_vertices (resource_graph_t &g, vtx_t node, std::vector<vtx_t> &node_list);
    int get_parent_vtx (resource_graph_t &g, vtx_t node, vtx_t &parent_node);
};

}  // namespace resource_model
}  // namespace Flux

#endif  // RESOURCE_READER_JGF_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

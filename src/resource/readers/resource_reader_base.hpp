/*****************************************************************************\
 * Copyright 2019 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef RESOURCE_READER_BASE_HPP
#define RESOURCE_READER_BASE_HPP

#include <set>
#include <string>
#include <cerrno>
#include "resource/schema/resource_graph.hpp"
#include "resource/store/resource_graph_store.hpp"
#include "resource/readers/resource_namespace_remapper.hpp"

namespace Flux {
namespace resource_model {


/*!  Base resource reader class.
 */
class resource_reader_base_t {
public:

    virtual ~resource_reader_base_t ();

    /*! Unpack str into a resource graph.
     *
     * \param g      resource graph
     * \param m      resource graph meta data
     * \param str    resource set string
     * \param rank   assign rank to all of the newly created resource vertices
     * \return       0 on success; non-zero integer on an error
     */
    virtual int unpack (resource_graph_t &g, resource_graph_metadata_t &m,
                        const std::string &str, int rank = -1) = 0;

    /*! Unpack str into a resource graph and graft
     *  the top-level vertices to vtx.
     *
     * \param g      resource graph
     * \param m      resource graph meta data
     * \param vtx    parent vtx at which to graft the deserialized graph
     * \param str    resource set string
     * \param rank   assign rank to all of the newly created resource vertices
     * \return       0 on success; non-zero integer on an error
     */
    virtual int unpack_at (resource_graph_t &g, resource_graph_metadata_t &m,
                           vtx_t &vtx, const std::string &str, int rank = -1) = 0;

    virtual int remove_subgraph (resource_graph_t &g,
                                 resource_graph_metadata_t &m,
                                 const std::string &path) = 0;

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
    virtual int update (resource_graph_t &g, resource_graph_metadata_t &m,
                        const std::string &str, int64_t jobid, int64_t at,
                        uint64_t dur, bool rsv, uint64_t trav_token) = 0;

    /*! Set the allowlist: only resources that are part of this allowlist
     *  will be unpacked into the graph.
     *
     * \param csl    comma separated allowlist string
     * \return       0 on success; non-zero integer on an error
     */
    int set_allowlist (const std::string &csl);

    /*! Is the allowlist set
     *
     * \return       true when supported
     */
    virtual bool is_allowlist_set ();

    /*! Is the selected reader format support allowlist
     *
     * \return       true when supported
     */
    virtual bool is_allowlist_supported () = 0;

    /*! Return the error message string.
     */
    const std::string &err_message () const;

    /*! Query the numeric ID suffix from hostname.
     *
     * \param hn        hostname string
     * \param id        (Output) integer ID suffix of hostname
     *                  -1 if hostname does not end with a numeric suffix.
     *
     * \return          0 if succeeds or -1 with errno if an error encountered
     */
    int get_hostname_suffix (const std::string &hn, int64_t &id) const;

    /*! Query the host basename from hostname.
     *
     * \param hn        hostname string
     * \param basename  (Output) host basename
     *
     * \return          0 if succeeds or -1 with errno if an error encountered
     */
    int get_host_basename (const std::string &hn, std::string &basename) const;

    /*! Clear the error message string.
     */
    void clear_err_message ();

    /*! Expose resource_namespace_remapper_t interface
     */
    resource_namespace_remapper_t namespace_remapper;

protected:
    bool in_allowlist (const std::string &resource);
    int split_hostname (const std::string &hn,
                        std::string &basename, int64_t &id) const;
    std::set<std::string> allowlist;
    std::string m_err_msg = "";
};

} // namespace resource_model
} // namespace Flux


#endif // RESOURCE_READER_BASE_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

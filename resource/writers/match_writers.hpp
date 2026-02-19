/*****************************************************************************\
 * Copyright 2014 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef MATCH_WRITERS_HPP
#define MATCH_WRITERS_HPP

#include <memory>
#include <string>
#include <sstream>
#include <set>

extern "C" {
#include <jansson.h>
}

namespace Flux {
namespace resource_model {

enum class match_format_t { SIMPLE, JGF, JGF_SHORTHAND, RLITE, RV1, RV1_NOSCHED, PRETTY_SIMPLE };

/*! Base match writers class for a matched resource set
 */
class match_writers_t {
   public:
    virtual ~match_writers_t ()
    {
    }
    virtual bool empty () = 0;
    virtual int emit_json (json_t **o, json_t **aux = nullptr) = 0;
    virtual int emit (std::stringstream &out) = 0;
    virtual int emit_vtx (const std::string &prefix,
                          const resource_graph_t &g,
                          const vtx_t &u,
                          unsigned int needs,
                          const std::map<std::string, std::string> &agfilter_data,
                          bool exclusive) = 0;
    virtual int emit_edg (const std::string &prefix, const resource_graph_t &g, const edg_t &e)
    {
        return 0;
    }
    virtual int emit_tm (uint64_t starttime, uint64_t expiration)
    {
        return 0;
    }
    virtual int emit_attrs (const std::string &k, const std::string &v)
    {
        return 0;
    }
    int compress_ids (std::stringstream &o, const std::vector<int64_t> &ids);
    int compress_hosts (const std::vector<std::string> &hosts,
                        const char *hostlist_init,
                        char **hostlist);

    /* Return a boolean indicating whether or not the writer should be invoked
     * on vertices that form non-root parts of exclusive subtrees.
     */
    virtual bool emit_exclusive_subtrees ()
    {
        return true;
    }
};

/*! Simple match writers class for a matched resource set
 */
class sim_match_writers_t : public match_writers_t {
   public:
    virtual ~sim_match_writers_t ()
    {
    }
    virtual bool empty ();
    virtual int emit_json (json_t **o, json_t **aux = nullptr);
    virtual int emit (std::stringstream &out);
    virtual int emit_vtx (const std::string &prefix,
                          const resource_graph_t &g,
                          const vtx_t &u,
                          unsigned int needs,
                          const std::map<std::string, std::string> &agfilter_data,
                          bool exclusive) override;

   private:
    std::stringstream m_out;
};

template<typename T>
concept associative_cstr_key = requires (T h, std::string s) {
    { h.begin () };
    { h.end () };
    { h.begin ()->first.c_str () } -> std::convertible_to<const char *>;
    { h.begin ()->second.c_str () } -> std::convertible_to<const char *>;
};

/*! JGF match writers class for a matched resource set
 */
class jgf_match_writers_t : public match_writers_t {
   public:
    jgf_match_writers_t ();
    jgf_match_writers_t (const jgf_match_writers_t &w);
    jgf_match_writers_t &operator= (const jgf_match_writers_t &w);
    virtual ~jgf_match_writers_t ();

    virtual bool empty ();
    virtual int emit_json (json_t **o, json_t **aux = nullptr);
    virtual int emit (std::stringstream &out);
    virtual int emit_vtx (const std::string &prefix,
                          const resource_graph_t &g,
                          const vtx_t &u,
                          unsigned int needs,
                          const std::map<std::string, std::string> &agfilter_data,
                          bool exclusive) override;
    virtual int emit_edg (const std::string &prefix, const resource_graph_t &g, const edg_t &e);

   private:
    json_t *emit_vtx_base (const resource_graph_t &g,
                           const vtx_t &u,
                           unsigned int needs,
                           bool exclusive);
    int emit_vtx_prop (json_t *o,
                       const resource_graph_t &g,
                       const vtx_t &u,
                       unsigned int needs,
                       bool exclusive);
    int emit_vtx_path (json_t *o,
                       const resource_graph_t &g,
                       const vtx_t &u,
                       unsigned int needs,
                       bool exclusive);

    int map2json (json_t *o, associative_cstr_key auto const &mp, const char *key)
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

    int emit_edg_meta (json_t *o, const resource_graph_t &g, const edg_t &e);
    int alloc_json_arrays ();
    int check_array_sizes ();

    json_t *m_vout = NULL;
    json_t *m_eout = NULL;
};

/*! JGF_shorthand match writers class for a matched resource set
 */
class jgf_shorthand_match_writers_t : public jgf_match_writers_t {
   public:
    jgf_shorthand_match_writers_t () = default;
    jgf_shorthand_match_writers_t (const jgf_shorthand_match_writers_t &w) = default;
    jgf_shorthand_match_writers_t &operator= (const jgf_shorthand_match_writers_t &w);

    virtual bool emit_exclusive_subtrees () override
    {
        return false;
    }
};

/*! RLITE match writers class for a matched resource set
 */
class rlite_match_writers_t : public match_writers_t {
   public:
    rlite_match_writers_t ();
    rlite_match_writers_t (const rlite_match_writers_t &w);
    rlite_match_writers_t &operator= (const rlite_match_writers_t &w);
    virtual ~rlite_match_writers_t ();

    virtual bool empty ();
    virtual int emit_json (json_t **o, json_t **aux = nullptr);
    virtual int emit (std::stringstream &out, bool newline);
    virtual int emit (std::stringstream &out);
    virtual int emit_vtx (const std::string &prefix,
                          const resource_graph_t &g,
                          const vtx_t &u,
                          unsigned int needs,
                          const std::map<std::string, std::string> &agfilter_data,
                          bool exclusive) override;

   private:
    class rank_host_t {
       public:
        int64_t rank;
        std::string host;
    };
    bool m_reducer_set ();
    int emit_gatherer (const resource_graph_t &g, const vtx_t &u);
    int get_gatherer_children (std::string &children);
    int fill (json_t *rlite_array, json_t *host_array, json_t *props);
    int fill_hosts (std::vector<std::string> &hosts, json_t *host_array);

    std::map<resource_type_t, std::vector<int64_t>> m_reducer;
    std::map<std::string, std::vector<rank_host_t>> m_gl_gatherer;
    std::map<std::string, std::vector<int64_t>> m_gl_prop_gatherer;
    std::set<resource_type_t> m_gatherer;
};

/*! R Version 1 match writers class for a matched resource set
 */
class rv1_match_writers_t : public match_writers_t {
   public:
    virtual bool empty ();
    virtual int emit_json (json_t **o, json_t **aux = nullptr);
    virtual int emit (std::stringstream &out);
    virtual int emit_vtx (const std::string &prefix,
                          const resource_graph_t &g,
                          const vtx_t &u,
                          unsigned int needs,
                          const std::map<std::string, std::string> &agfilter_data,
                          bool exclusive) override;
    virtual int emit_edg (const std::string &prefix, const resource_graph_t &g, const edg_t &e);
    virtual int emit_tm (uint64_t start_tm, uint64_t end_tm);
    virtual int emit_attrs (const std::string &k, const std::string &v);

   protected:
    virtual jgf_match_writers_t &get_jgf ();

   private:
    int attrs_json (json_t **o);

    rlite_match_writers_t rlite;
    int64_t m_starttime = 0;
    int64_t m_expiration = 0;
    std::map<std::string, std::string> m_attrs;
    jgf_match_writers_t jgf_writer;
};

/*! R Version 1 with no "scheduling" key match writers class
 */
class rv1_nosched_match_writers_t : public match_writers_t {
   public:
    virtual bool empty ();
    virtual int emit_json (json_t **o, json_t **aux = nullptr);
    virtual int emit (std::stringstream &out);
    virtual int emit_vtx (const std::string &prefix,
                          const resource_graph_t &g,
                          const vtx_t &u,
                          unsigned int needs,
                          const std::map<std::string, std::string> &agfilter_data,
                          bool exclusive) override;
    virtual int emit_tm (uint64_t start_tm, uint64_t end_tm);

   private:
    rlite_match_writers_t rlite;
    int64_t m_starttime = 0;
    int64_t m_expiration = 0;
};

/*! Human-friendly simple match writers class for a matched resource set
 */
class pretty_sim_match_writers_t : public match_writers_t {
   public:
    virtual bool empty ();
    virtual int emit_json (json_t **o, json_t **aux = nullptr);
    virtual int emit (std::stringstream &out);
    virtual int emit_vtx (const std::string &prefix,
                          const resource_graph_t &g,
                          const vtx_t &u,
                          unsigned int needs,
                          const std::map<std::string, std::string> &agfilter_data,
                          bool exclusive) override;

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

}  // namespace resource_model
}  // namespace Flux

#endif  // MATCH_WRITERS_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

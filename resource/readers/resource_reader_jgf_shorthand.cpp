extern "C" {
#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <flux/idset.h>
}

#include "resource/readers/resource_reader_jgf_shorthand.hpp"

using namespace Flux;
using namespace Flux::resource_model;

resource_reader_jgf_shorthand_t::~resource_reader_jgf_shorthand_t ()
{
}

int resource_reader_jgf_shorthand_t::fetch_additional_vertices (
    resource_graph_t &g,
    resource_graph_metadata_t &m,
    fetch_helper_t &fetcher,
    std::vector<fetch_helper_t> &additional_vertices)
{
    int rc = -1;
    std::map<std::string, vmap_val_t> empty_vmap{};  // so `find_vtx` doesn't error out
    vtx_t v = boost::graph_traits<resource_graph_t>::null_vertex ();
    if (!fetcher.exclusive)  // vertex isn't exclusive, nothing to do
        return 0;

    if ((rc = resource_reader_jgf_t::find_vtx (g, m, empty_vmap, fetcher, v)) != 0)
        return rc;

    return recursively_collect_vertices (g, v, additional_vertices);
}

int resource_reader_jgf_shorthand_t::recursively_collect_vertices (
    resource_graph_t &g,
    vtx_t v,
    std::vector<fetch_helper_t> &additional_vertices)
{
    static const subsystem_t containment_sub{"containment"};
    f_out_edg_iterator_t ei, ei_end;
    vtx_t target;

    if (v == boost::graph_traits<resource_graph_t>::null_vertex ()) {
        return -1;
    }

    for (boost::tie (ei, ei_end) = boost::out_edges (v, g); ei != ei_end; ++ei) {
        if (g[*ei].subsystem != containment_sub)
            continue;
        target = boost::target (*ei, g);

        fetch_helper_t vertex_copy;
        vertex_copy.type = g[target].type.c_str ();
        vertex_copy.basename = g[target].basename.c_str ();
        vertex_copy.size = g[target].size;
        vertex_copy.uniq_id = g[target].uniq_id;
        vertex_copy.rank = g[target].rank;
        vertex_copy.status = g[target].status;
        vertex_copy.id = g[target].id;
        vertex_copy.name = g[target].name;
        vertex_copy.properties = g[target].properties;
        vertex_copy.paths = g[target].paths;
        vertex_copy.unit = g[target].unit.c_str ();
        vertex_copy.exclusive = 1;  // must be exclusive as part of exclusive sub-tree
        if (resource_reader_jgf_t::apply_defaults (vertex_copy, g[target].name.c_str ()) < 0)
            return -1;

        additional_vertices.push_back (vertex_copy);
        if (recursively_collect_vertices (g, target, additional_vertices) < 0) {
            return -1;
        }
    }
    return 0;
}

int resource_reader_jgf_shorthand_t::fetch_additional_edges (
    resource_graph_t &g,
    resource_graph_metadata_t &m,
    std::map<std::string, vmap_val_t> &vmap,
    fetch_helper_t &root,
    std::vector<fetch_helper_t> &additional_vertices,
    uint64_t token)
{
    if (additional_vertices.size () > 0 && update_additional_edges (g, m, vmap, root, token) < 0) {
        return -1;
    }
    // iterate through again to add edges
    for (auto &fetcher : additional_vertices) {
        if (update_additional_edges (g, m, vmap, fetcher, token) < 0) {
            return -1;
        }
    }
    return 0;
}

int resource_reader_jgf_shorthand_t::update_additional_edges (
    resource_graph_t &g,
    resource_graph_metadata_t &m,
    std::map<std::string, vmap_val_t> &vmap,
    fetch_helper_t &fetcher,
    uint64_t token)
{
    vtx_t v;
    std::map<std::string, vmap_val_t> empty_vmap{};  // so `find_vtx` doesn't error out
    std::string vertex_id = std::to_string (fetcher.uniq_id);
    fetcher.vertex_id = vertex_id.c_str ();
    if (resource_reader_jgf_t::find_vtx (g, m, empty_vmap, fetcher, v) != 0
        || v == boost::graph_traits<resource_graph_t>::null_vertex ())
        return -1;
    fetcher.vertex_id = nullptr;
    f_out_edg_iterator_t ei, ei_end;
    for (boost::tie (ei, ei_end) = boost::out_edges (v, g); ei != ei_end; ++ei) {
        if (g[*ei].subsystem != containment_sub)
            continue;
        std::string target_str = std::to_string (boost::target (*ei, g));
        if (update_src_edge (g, m, vmap, vertex_id, token) < 0
            || update_tgt_edge (g, m, vmap, vertex_id, target_str, token) < 0) {
            return -1;
        }
    }
    return 0;
}

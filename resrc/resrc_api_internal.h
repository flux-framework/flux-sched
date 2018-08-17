#ifndef _FLUX_RESRC_API_INTERNAL_H
#define _FLUX_RESRC_API_INTERNAL_H

/* Should be only included in internal API implementation codes
 * An ability to query the roots of various tree graphs is now
 * part of Resource APIs.
 */
struct resrc_api_ctx {
    zhash_t *resrc_hash;      /* look up compute node resrc by name */
    resrc_t *hwloc_cluster;   /* track the cluster resrc for hwloc reader */
    char *tree_name;          /* track the name of the physical hieararchy */
    resrc_tree_t *tree_root;  /* track the root resrc of the phys hierarcy */
    zlist_t *flow_names;      /* names of the flow hierarchies read in */
    zhash_t *flow_roots;      /* roots of the flow hierarchies indexed by name */
    /* additional API-level data here */
};

struct resrc_api_map {
    zhash_t *map;
};

#endif /* !_FLUX_RESRC_API_INTERNAL_H */

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

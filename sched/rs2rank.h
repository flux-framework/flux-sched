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

#ifndef RS2RANK_H
#define RS2RANK_H 1

#include <hwloc.h>

typedef struct machs machs_t;
typedef struct rssig rssig_t;

/* rs2rank table (of machs_t type) c'tor/d'tors.
 * The table is keyed by hostname and each value entry maintains
 * the signature of each resource partition within this host as
 * well as one or more equivalent broker ranks that manage the
 * resource partition.
 */
machs_t *rs2rank_tab_new ();
void rs2rank_tab_destroy (machs_t *m);

/* A equality matching function based on hostname, socket and core count
 * This comparator should be used if a node-type resrc_t object can
 * be considered to be equal to a hwloc object when their hostnames
 * and socket and core counts all match.
 */
const char *rs2rank_tab_eq_by_count (machs_t *m, const char *hn, int s, int c);

/* A testing equality matching -- returns the signature of the first partition
 * This dumb comparator should be used if each and every node-type resrc_t object
 * must be matched with the first hwloc obj (e.g., emulation mode).
 */
const char *rs2rank_tab_eq_by_none (machs_t *m);

/* Update the rs2rank table with hostname and resource partition signature
 * If multiple ranks manage the same resource partition given by s,
 * the table maintains and treats all these ranks as "equivalent."
 */
int rs2rank_tab_update (machs_t *m, const char *hn, rssig_t *s, uint32_t rank);

/* Return a broker rank that manages the resource partition
 * given by hostname hn and digest. If multiple ranks manage the partition
 * in a shared fashion, return a rank in round robin from the equivalent
 * ranks set (wrapped to the first rank in the end).  Passing true to
 * reset will return the first rank of this ranks set by resetting
 * the iterator.
 */
int rs2rank_tab_query_by_sign (machs_t *m, const char *hn, const char *digest,
                               bool reset, uint32_t *rank);

/* A testing query -- always use the first host in m
 * (needed for emulation testing)
 */
int rs2rank_tab_query_by_none (machs_t *m, const char *digest,
                               bool reset, uint32_t *rank);


/* Allocate and set signature s using the hwloc xml string (rsb), its length,
 * optional auxiliary information and hwloc obj. s should be freed
 * by rs2rank_tab_destroy after it is made associated with the rs2rank table.
 */
int rs2rank_set_signature (char *rsb, size_t l, char *aux,
                           hwloc_topology_t t, rssig_t **s);

/* Utility function to return a stringfied SHA digest of the resource partition */
const char *rs2rank_get_digest (rssig_t *sig);

#endif /* RS2RANK_H */

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

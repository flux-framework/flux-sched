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

/*
 * rs2rank.c - resource to rank API implementation
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdio.h>
#include <stdbool.h>
#include <czmq.h>
#include <hwloc.h>

#include "src/common/libutil/log.h"
#include "src/common/libutil/shortjansson.h"
#include "src/common/libutil/xzmalloc.h"
#include "resrc.h"
#include "resrc_tree.h"
#include "resrc_reqst.h"
#include "rs2rank.h"

/* This rs2rank table is keyed by hostname. The value
 * is another hash table keyed by the signature of
 * the resource partition contained within the host
 * (e.g., a distinct set of cores).
 * Normally there is only one resource partition per host.
 * Each value of the partition hash table includes an
 * equivalent set of broker ranks that control and mananage
 * that partition.
 */
struct machs {
    zhash_t      *tab;
};

/* Each resource partition within a host holds an object of
 * this resource signature. This can be extended later if needed.
 */
struct rssig {
    char         *digest;
    int           nsockets;
    int           ncores;
};

typedef struct partition {
    struct rssig *sig;
    int           rrobin;
    zlist_t      *ranks;
} partition_t;


/******************************************************************************
 *                                                                            *
 *                             Utility functions                              *
 *                                                                            *
 ******************************************************************************/

static inline bool host_seen (zhash_t *tab, const char *hn)
{
    return (hn && zhash_lookup (tab, hn)) ? true : false;
}

static inline bool partition_seen (zhash_t *partab, const char *digest)
{
    return (digest && zhash_lookup (partab, digest)) ? true : false;
}

static void partition_tab_freefn (void *data)
{
    zhash_t *tab = (zhash_t *)data;
    zhash_destroy (&tab);
}

static void partition_freefn (void *data)
{
    partition_t *o = (partition_t *)data;
    if (o) {
        if (o->sig) {
            if (o->sig->digest) {
                free (o->sig->digest);
                o->sig->digest = NULL;
            }
            free (o->sig);
            o->sig = NULL;
        }
        if (o->ranks)
            zlist_destroy (&(o->ranks));
        free (o);
        o = NULL;
    }
}

static inline void partition_tab_new (zhash_t *tab, const char *hn)
{
    zhash_t *partab;
    if (!(partab  = zhash_new ()))
        oom ();
    zhash_insert (tab, hn, (void *)partab);
    zhash_freefn (tab, hn, partition_tab_freefn);
}

static inline void partition_new (zhash_t *partab, rssig_t *sig)
{
    partition_t *nobj = (partition_t *)xzmalloc (sizeof (*nobj));
    nobj->sig = sig;
    nobj->rrobin = 0;
    if (!(nobj->ranks = zlist_new ()))
        oom ();
    zhash_insert (partab, sig->digest, (void *)nobj);
    zhash_freefn (partab, sig->digest, partition_freefn);
}

static int get_rank_rrobin (partition_t *part, uint32_t *rank)
{
    int rc = 0;
    uint32_t *tmp;
    if (!(part->ranks))
        return -1;

    if (part->rrobin && (tmp = zlist_next (part->ranks))) {
        *rank = *tmp;
        part->rrobin = 1;
    }
    else if ((tmp = zlist_first (part->ranks))) {
        *rank = *tmp;
        part->rrobin =1;
    }
    else
        rc = -1;

    return rc;
}


/******************************************************************************
 *                                                                            *
 *                   Resource2BrokerRank Public APIs                          *
 *                                                                            *
 ******************************************************************************/

machs_t *rs2rank_tab_new ()
{
    machs_t *m;
    m = (machs_t *) xzmalloc (sizeof (*m));
    if (!(m->tab = zhash_new ()))
        oom ();
    return m;
}

void rs2rank_tab_destroy (machs_t *m)
{
    if (m) {
        if (m->tab)
            zhash_destroy (&(m->tab));
        free (m);
    }
}

int rs2rank_tab_query_by_sign (machs_t *m, const char *hn, const char *digest,
                                bool reset, uint32_t *rank)
{
    int rc = -1;
    zhash_t *partab = NULL;
    partition_t *part = NULL;
    if (!host_seen (m->tab, hn))
        goto done;
    if (!(partab = zhash_lookup (m->tab, hn)))
        goto done;
    if (!(part = zhash_lookup (partab, digest)))
        goto done;
    if (reset)
        part->rrobin = 0;
    if (get_rank_rrobin (part, rank) != 0)
        goto done;

    rc = 0;
done:
    return rc;
}

int rs2rank_tab_query_by_none (machs_t *m, const char *digest,
                               bool reset, uint32_t *rank)
{
    int rc = -1;
    zhash_t *partab = NULL;
    partition_t *part = NULL;
    if (!(partab = zhash_first (m->tab)))
        goto done;
    if (!(part = zhash_lookup (partab, digest)))
        goto done;
    if (reset)
        part->rrobin = 0;
    if (get_rank_rrobin (part, rank) != 0)
        goto done;
    rc = 0;
done:
    return rc;
}

int rs2rank_tab_update (machs_t *m, const char *hn, rssig_t *sig, uint32_t rank)
{
    int rc = -1;
    uint32_t *rcp = NULL;
    partition_t *robj = NULL;
    zhash_t *partab = NULL;

    if (!hn || !sig || !m || !(m->tab))
        goto done;
    else if (!host_seen (m->tab, hn))
        partition_tab_new (m->tab, hn);

    if (!(partab = zhash_lookup (m->tab, hn)))
        goto done;
    else if (!partition_seen (partab, sig->digest))
        partition_new (partab, sig);

    if (!(robj = zhash_lookup (partab, sig->digest)))
        goto done;

    rcp = xzmalloc (sizeof (*rcp));
    *rcp = rank;
    if (zlist_append (robj->ranks, (void *)rcp) != 0)
        goto done;
    else if (!zlist_freefn (robj->ranks, (void *)rcp, free, false))
        goto done;
    rc = 0;

done:
    return rc;
}

const char *rs2rank_tab_eq_by_count (machs_t *m, const char *hn,
                                    int nsockets, int ncores)
{
    zhash_t *partab = NULL;
    partition_t *part = NULL;
    const char *rdigest = NULL;

    if (!host_seen (m->tab, hn))
        goto done;
    if ((!(partab = zhash_lookup (m->tab, hn))))
        goto done;
    for (part = zhash_first (partab); part; part = zhash_next (partab)) {
        if (part->sig->nsockets == nsockets && part->sig->ncores == ncores) {
            rdigest = part->sig->digest;
            break;
        }
    }
done:
    return rdigest;
}

const char *rs2rank_tab_eq_by_none (machs_t *m)
{
    const char *rdigest = NULL;
    zhash_t *partab = NULL;
    partition_t *part = NULL;
    if ((partab = zhash_first (m->tab)) && (part = zhash_first (partab)))
        rdigest = part->sig->digest;
    return rdigest;
}

int rs2rank_set_signature (char *rsbuf, size_t len, char *aux,
                           hwloc_topology_t t, rssig_t **sig)
{
    int rc = -1;
    zdigest_t *digest = NULL;

    if (!rsbuf)
        goto error;

    *sig = (rssig_t *) xzmalloc (sizeof (**sig)) ;
    if (!(digest = zdigest_new ()))
        oom ();

    zdigest_update (digest, (byte *)rsbuf, len);
    if (!aux)
        (*sig)->digest = xasprintf ("%s", zdigest_string (digest));
    else
        (*sig)->digest = xasprintf ("%s.%s", zdigest_string (digest), aux);

    zdigest_destroy (&(digest));
    /* FIMXE: Why HWLOC_OBJ_NUMANODE doesn't work ? */
    (*sig)->nsockets = hwloc_get_nbobjs_by_type (t, HWLOC_OBJ_SOCKET);
    (*sig)->ncores = hwloc_get_nbobjs_by_type (t, HWLOC_OBJ_CORE);
    if (((*sig)->nsockets > 0) && ((*sig)->ncores > 0))
        rc = 0;

error:
    return rc;
}

const char *rs2rank_get_digest (rssig_t *sig)
{
    return sig ? sig->digest : NULL;
}


/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

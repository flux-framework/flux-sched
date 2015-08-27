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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <czmq.h>

#include "rdl.h"
#include "../scheduler.h"
#include "src/common/libtap/tap.h"


static struct timeval start_time;

int schedule_jobs (ctx_t *ctx, double sim_time)
{
    return 0;
}

bool allocate_bandwidth (flux_lwj_t *job, struct resource *r, zlist_t *ancestors) {
    return 0;
}

void init_time() {
    gettimeofday(&start_time, NULL);
}

u_int64_t get_time() {
    struct timeval t;
    gettimeofday(&t, NULL);
    return (u_int64_t) (t.tv_sec - start_time.tv_sec) * 1000000
        + (t.tv_usec - start_time.tv_usec);
}

int main (int argc, char *argv[])
{
    const char *filename = argv[1];
    int verbose = 0;
    int64_t free_cores = 0, free_nodes = 0;
    struct rdl *rdl = NULL;
    struct rdllib *rdllib = NULL;
    struct resource *root = NULL;
    const char *uri = "default";
    bool test_ok = false;

    plan (7);
    if (filename == NULL || *filename == '\0')
        filename = getenv ("TESTSIM_INPUT_FILE");

    ok ((filename != NULL), "valid resource file name");
    ok ((access (filename, F_OK) == 0), "resoure file exists");
    ok ((access (filename, R_OK) == 0), "resoure file readable");

    init_time();
    rdllib = rdllib_open ();
    rdl = rdl_loadfile (rdllib, filename);

    test_ok = rdllib != NULL && rdl != NULL;
    ok (test_ok, "resource generation took: %lf",
        ((double)get_time())/1000000);
    if (!test_ok)
        goto ret;

    root = rdl_resource_get (rdl, uri);
    ok ((root != NULL), "tree root found");
    if (!root)
        goto ret;

    if (verbose) {
        printf ("Printing resources:\n");
        print_resources (root);
        printf ("End of resources\n");
    }

    init_time();
    free_cores = get_free_count (rdl, uri, "core");
    free_nodes = get_free_count (rdl, uri, "node");

    test_ok = (free_cores == 2464);
    ok (test_ok, "found %d free cores in %lf", free_cores,
        ((double)get_time())/1000000);

    test_ok = (free_nodes == 154);
    ok (test_ok, "found %d free nodes in %lf", free_nodes,
        ((double)get_time())/1000000);

    init_time();
    rdl_resource_destroy (root);
    rdl_destroy (rdl);
    rdllib_close (rdllib);
    printf("destroy took: %lf\n", ((double)get_time())/1000000);
ret:
    done_testing ();
}


/*
 * vi: ts=4 sw=4 expandtab
 */

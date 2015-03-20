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

#include "../resrc.h"
#include "../resrc_tree.h"
#include "src/common/libtap/tap.h"


static struct timeval start_time;

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
    char *resrc_id = NULL;
    const char *filename = argv[1];
    int found = 0;
    int rc = 0;
    int verbose = 0;
    JSON ja = NULL;
    /* JSON jtago = NULL;  /\* json tag object *\/ */
    JSON child_core = NULL;
    JSON child_sock = NULL;
    JSON o = NULL;
    JSON req_res = NULL;
    resource_list_t *found_res = resrc_new_id_list ();
    resources_t *resrcs = NULL;
    resrc_t *resrc = NULL;
    resrc_t *sample_resrc = NULL;
    resrc_tree_t *resrc_tree = NULL;
    zlist_t *resrc_tree_list = zlist_new ();

    plan (14);
    if (filename == NULL || *filename == '\0')
        filename = getenv ("TESTRESRC_INPUT_FILE");

    ok ((filename != NULL), "valid resource file name");
    ok ((access (filename, F_OK) == 0), "resoure file exists");
    ok ((access (filename, R_OK) == 0), "resoure file readable");

    init_time();
    resrcs = resrc_generate_resources (filename, "default");

    ok ((resrcs != NULL), "resource generation took: %lf",
        ((double)get_time())/1000000);
    if (!resrcs)
        goto ret;

    if (verbose) {
        printf ("Listing flat resources:\n");
        resrc_print_resources (resrcs);
        printf ("End of flat resources\n");
    }

    resrc = zhash_lookup ((zhash_t *)resrcs, "head");
    ok ((resrc != NULL), "tree head found");
    if (!resrc)
        goto ret;

    resrc_tree = resrc_phys_tree (resrc);
    ok ((resrc_tree != NULL), "resource tree valid");
    if (!resrc_tree)
        goto ret;

    if (verbose) {
        printf ("Listing resource tree\n");
        resrc_tree_print (resrc_tree);
        printf ("End of resource tree\n");
    }
    zlist_append (resrc_tree_list, resrc_tree);

    /*
     *  Build a resource composite to search for
     */
    child_core = Jnew ();
    Jadd_str (child_core, "type", "core");
    Jadd_int (child_core, "req_qty", 6);

    ja = Jnew_ar ();
    Jadd_ar_obj(ja, child_core);

    child_sock = Jnew ();
    Jadd_str (child_sock, "type", "socket");
    Jadd_int (child_sock, "req_qty", 2);
    Jadd_obj (child_sock, "req_children", ja);

    /* jtago = Jnew (); */
    /* Jadd_bool (jtago, "maytag", true); */
    /* Jadd_bool (jtago, "yourtag", true); */

    req_res = Jnew ();
    Jadd_str (req_res, "type", "node");
    /* Jadd_obj (req_res, "tags", jtago); */
    Jadd_int (req_res, "req_qty", 2);
    Jadd_obj (req_res, "req_child", child_sock);

    sample_resrc = resrc_new_from_json (req_res, NULL);
    Jput (req_res);
    ok ((sample_resrc != NULL), "sample resource composite valid");
    if (!sample_resrc)
        goto ret;

    if (verbose) {
        printf ("Listing sample tree\n");
        resrc_tree_print (resrc_phys_tree (sample_resrc));
        printf ("End of sample tree\n");
    }

    init_time();
    found = resrc_tree_search ((resrc_tree_list_t *)resrc_tree_list, found_res,
                               resrc_phys_tree (sample_resrc), false);

    ok (found, "found %d composite resources in %lf", found,
        ((double)get_time())/1000000);
    if (!found)
        goto ret;

    if (verbose) {
        resrc_id = resrc_list_first (found_res);
        while (resrc_id) {
            printf ("resrc_id %s\n", resrc_id);
            resrc_id = resrc_list_next (found_res);
        }
    }

    init_time();
    o = resrc_serialize (resrcs, found_res);
    ok ((o != NULL), "found resource serialization took: %lf",
        ((double)get_time())/1000000);

    if (verbose) {
        printf ("The found resources serialized: %s\n", Jtostr (o));
    }
    Jput (o);

    init_time();
    rc = resrc_allocate_resources (resrcs, found_res, 1);
    ok (!rc, "successfully allocated resources for job 1");
    rc = resrc_allocate_resources (resrcs, found_res, 2);
    ok (!rc, "successfully allocated resources for job 2");
    rc = resrc_allocate_resources (resrcs, found_res, 3);
    ok (!rc, "successfully allocated resources for job 3");
    rc = resrc_reserve_resources (resrcs, found_res, 4);
    ok (!rc, "successfully reserved resources for job 4");

    printf ("allocate and reserve took: %lf\n", ((double)get_time())/1000000);

    if (verbose) {
        printf ("Allocated and reserved resources\n");
        resrc_print_resources (resrcs);
    }

    init_time();
    rc = resrc_release_resources (resrcs, found_res, 1);
    ok (!rc, "resource release of job 1 took: %lf",
        ((double)get_time())/1000000);

    if (verbose) {
        printf ("Same resources without job 1\n");
        resrc_print_resources (resrcs);
    }

    init_time();
    resrc_id_list_destroy (found_res);
    resrc_destroy_resources (&resrcs);
    printf("destroy took: %lf\n", ((double)get_time())/1000000);
ret:
    done_testing ();
}


/*
 * vi: ts=4 sw=4 expandtab
 */

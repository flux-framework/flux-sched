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

static void test_select_children (resrc_tree_t rt)
{
    if (rt) {
        resrc_t resrc = resrc_tree_resrc (rt);
        if (resrc_has_pool (resrc, "memory")) {
            resrc_select_pool_items (resrc, "memory", 100);
        }
        if (zlist_size ((zlist_t*)resrc_tree_children(rt))) {
            resrc_tree_t child = zlist_first ((zlist_t*)
                                              resrc_tree_children(rt));
            while (child) {
                test_select_children (child);
                child = zlist_next ((zlist_t*)resrc_tree_children(rt));
            }
        }
    }
}

/*
 * Select some resources from the found trees
 */
static resrc_tree_list_t test_select_resources (resrc_tree_list_t found_trees,
                                                int skip)
{
    resrc_tree_list_t selected_res = resrc_tree_new_list ();
    resrc_tree_t rt;
    int count = 1;

    rt = resrc_tree_list_first (found_trees);
    while (rt) {
        if (!(count % skip)) {
            test_select_children (rt);
            zlist_append ((zlist_t *) selected_res, rt);
        }
        count++;
        rt = resrc_tree_list_next (found_trees);
    }

    return (resrc_tree_list_t)selected_res;
}

int main (int argc, char *argv[])
{
    const char *filename = argv[1];
    int found = 0;
    int rc = 0;
    int verbose = 0;
    JSON ja = NULL;
    /* JSON jtago = NULL;  /\* json tag object *\/ */
    JSON child_core = NULL;
    JSON child_sock = NULL;
    JSON memory = NULL;
    JSON o = NULL;
    JSON req_res = NULL;
    resources_t resrcs = NULL;
    resrc_t resrc = NULL;
    resrc_reqst_t resrc_reqst = NULL;
    resrc_tree_list_t found_trees = resrc_tree_new_list ();
    resrc_tree_list_t selected_trees;
    resrc_tree_t found_tree = NULL;
    resrc_tree_t resrc_tree = NULL;

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

    /*
     *  Build a resource composite to search for
     */
    child_core = Jnew ();
    Jadd_str (child_core, "type", "core");
    Jadd_int (child_core, "req_qty", 6);

    memory = Jnew ();
    Jadd_str (memory, "type", "memory");
    Jadd_int (memory, "req_qty", 100);

    ja = Jnew_ar ();
    json_object_array_add (ja, child_core);
    json_object_array_add (ja, memory);

    child_sock = Jnew ();
    Jadd_str (child_sock, "type", "socket");
    Jadd_int (child_sock, "req_qty", 2);
    json_object_object_add (child_sock, "req_children", ja);

    /* jtago = Jnew (); */
    /* Jadd_bool (jtago, "maytag", true); */
    /* Jadd_bool (jtago, "yourtag", true); */

    req_res = Jnew ();
    Jadd_str (req_res, "type", "node");
    /* json_object_object_add (req_res, "tags", jtago); */
    Jadd_int (req_res, "req_qty", 2);
    json_object_object_add (req_res, "req_child", child_sock);

    resrc_reqst = resrc_reqst_from_json (req_res, NULL);
    Jput (req_res);
    ok ((resrc_reqst != NULL), "resource request valid");
    if (!resrc_reqst)
        goto ret;

    if (verbose) {
        printf ("Listing resource request tree\n");
        resrc_reqst_print (resrc_reqst);
        printf ("End of resource request tree\n");
    }

    init_time();
    found = resrc_tree_search (resrc_tree_children (resrc_tree), resrc_reqst,
                               found_trees, false);

    ok (found, "found %d composite resources in %lf", found,
        ((double)get_time())/1000000);
    if (!found)
        goto ret;

    if (verbose) {
        printf ("Listing found trees\n");
        found_tree = resrc_tree_list_first (found_trees);
        while (found_tree) {
            resrc_tree_print (found_tree);
            found_tree = resrc_tree_list_next (found_trees);
        }
        printf ("End of found trees\n");
    }

    o = Jnew ();
    init_time();
    rc = resrc_tree_list_serialize (o, found_trees);
    ok (!rc, "found resource serialization took: %lf",
        ((double)get_time())/1000000);

    if (verbose) {
        printf ("The found resources serialized: %s\n", Jtostr (o));
    }
    Jput (o);

    init_time();

    selected_trees = test_select_resources (found_trees, 1);
    rc = resrc_tree_list_allocate (selected_trees, 1);
    ok (!rc, "successfully allocated resources for job 1");
    zlist_destroy ((zlist_t**) &selected_trees);

    selected_trees = test_select_resources (found_trees, 2);
    rc = resrc_tree_list_allocate (selected_trees, 2);
    ok (!rc, "successfully allocated resources for job 2");
    zlist_destroy ((zlist_t**) &selected_trees);

    selected_trees = test_select_resources (found_trees, 3);
    rc = resrc_tree_list_allocate (selected_trees, 3);
    ok (!rc, "successfully allocated resources for job 3");
    zlist_destroy ((zlist_t**) &selected_trees);

    selected_trees = test_select_resources (found_trees, 2);
    rc = resrc_tree_list_reserve (selected_trees, 4);
    ok (!rc, "successfully reserved resources for job 4");
    zlist_destroy ((zlist_t**) &selected_trees);

    printf ("allocate and reserve took: %lf\n", ((double)get_time())/1000000);

    if (verbose) {
        printf ("Allocated and reserved resources\n");
        resrc_print_resources (resrcs);
    }

    init_time();
    rc = resrc_tree_list_release (found_trees, 1);
    ok (!rc, "resource release of job 1 took: %lf",
        ((double)get_time())/1000000);

    if (verbose) {
        printf ("Same resources without job 1\n");
        resrc_print_resources (resrcs);
    }

    init_time();
    resrc_reqst_destroy (resrc_reqst);
    resrc_tree_list_destroy (found_trees);
    resrc_destroy_resources (&resrcs);
    printf("destroy took: %lf\n", ((double)get_time())/1000000);
ret:
    done_testing ();
}


/*
 * vi: ts=4 sw=4 expandtab
 */

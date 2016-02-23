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

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/time.h>
#include <czmq.h>
#include <hwloc.h>

#include "../resrc.h"
#include "../resrc_tree.h"
#include "../resrc_reqst.h"
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

static void test_select_children (resrc_tree_t *rt)
{
    if (rt) {
        resrc_t *resrc = resrc_tree_resrc (rt);
        if (strcmp (resrc_type(resrc), "memory")) {
            resrc_stage_resrc (resrc, 1);
        } else {
            resrc_stage_resrc (resrc, 100);
        }
        if (resrc_tree_num_children (rt)) {
            resrc_tree_t *child = resrc_tree_list_first (
                resrc_tree_children (rt));
            while (child) {
                test_select_children (child);
                child = resrc_tree_list_next (resrc_tree_children (rt));
            }
        }
    }
}

/*
 * Select some resources from the found trees
 */
static resrc_tree_list_t *test_select_resources (resrc_tree_list_t *found_trees,
                                                 int select)
{
    resrc_tree_list_t *selected_res = resrc_tree_list_new ();
    resrc_tree_t *rt;
    int count = 1;

    rt = resrc_tree_list_first (found_trees);
    while (rt) {
        if (count == select) {
            test_select_children (rt);
            resrc_tree_list_append (selected_res, rt);
            break;
        }
        count++;
        rt = resrc_tree_list_next (found_trees);
    }

    return selected_res;
}

// Contains 10 tests
static int num_temporal_allocation_tests = 10;
static void test_temporal_allocation ()
{
    int rc = 0;
    size_t available;
    resrc_t *resource = resrc_new_resource ("custom", "/test", "test", NULL, 1,
                                            0, 10);

    available = resrc_available_at_time (resource, 0);
    rc = (rc || !(available == 10));
    available = resrc_available_during_range (resource, 0, 1000, false);
    rc = (rc || !(available == 10));
    ok (!rc, "resrc_available...(time/range) on unallocated resource work");

    // Setup the resource allocations for the rest of the tests
    resrc_stage_resrc (resource, 5);
    rc = (rc || resrc_allocate_resource (resource, 1, 1, 1000));
    resrc_stage_resrc (resource, 10);
    rc = (rc || resrc_allocate_resource (resource, 2, 2000, 3000));
    ok (!rc, "Temporal allocation setup works");
    if (rc) {
        return;
    }

    // Start the actual testing
    resrc_stage_resrc (resource, 1);
    // This should fail
    rc = (rc || !resrc_allocate_resource (resource, 3, 10, 3000));
    // This should work
    rc = (rc || resrc_allocate_resource (resource, 3, 10, 1999));
    ok (!rc, "Overlapping temporal allocations work");
    if (rc) {
        return;
    }

    // Test "available at time"
    // Job 1
    available = resrc_available_at_time (resource, 1);
    rc = (rc || !(available == 5));
    // Jobs 1 & 3
    available = resrc_available_at_time (resource, 10);
    rc = (rc || !(available == 4));
    available = resrc_available_at_time (resource, 500);
    rc = (rc || !(available == 4));
    available = resrc_available_at_time (resource, 1000);
    rc = (rc || !(available == 4));
    // Job 3
    available = resrc_available_at_time (resource, 1500);
    rc = (rc || !(available == 9));
    available = resrc_available_at_time (resource, 1999);
    rc = (rc || !(available == 9));
    // Job 2
    available = resrc_available_at_time (resource, 2000);
    rc = (rc || !(available == 0));
    available = resrc_available_at_time (resource, 2500);
    rc = (rc || !(available == 0));
    available = resrc_available_at_time (resource, 3000);
    rc = (rc || !(available == 0));
    // No Jobs
    available = resrc_available_at_time (resource, 3001);
    rc = (rc || !(available == 10));
    ok (!rc, "resrc_available_at_time works");
    if (rc) {
        return;
    }

    // Test "available during range"

    // Range == job window (both edges are the same)
    available = resrc_available_during_range (resource, 2000, 3000, false);
    rc = (rc || !(available == 0));
    available = resrc_available_during_range (resource, 0, 1000, false);
    rc = (rc || !(available == 4));
    available = resrc_available_during_range (resource, 10, 1999, false);
    rc = (rc || !(available == 4));
    ok (!rc, "resrc_available_during_range: range == job window works");
    rc = 0;

    // Range is a subset of job window (no edges are the same)
    available = resrc_available_during_range (resource, 4, 6, false);
    rc = (rc || !(available == 5));
    available = resrc_available_during_range (resource, 20, 999, false);
    rc = (rc || !(available == 4));
    available = resrc_available_during_range (resource, 1001, 1998, false);
    rc = (rc || !(available == 9));
    available = resrc_available_during_range (resource, 2500, 2600, false);
    rc = (rc || !(available == 0));
    ok (!rc, "resrc_available_during_range: range is a subset (no edges) works");
    rc = 0;

    // Range is a subset of a job window (one edge is the same)
    available = resrc_available_during_range (resource, 0, 999, false);
    rc = (rc || !(available == 4));
    available = resrc_available_during_range (resource, 10, 999, false);
    rc = (rc || !(available == 4));
    available = resrc_available_during_range (resource, 20, 1000, false);
    rc = (rc || !(available == 4));
    available = resrc_available_during_range (resource, 1001, 1999, false);
    rc = (rc || !(available == 9));
    available = resrc_available_during_range (resource, 1001, 1999, false);
    rc = (rc || !(available == 9));
    ok (!rc, "resrc_available_during_range: range is a subset (1 edge) works");
    rc = 0;

    // Range overlaps 1 job window
    //     (no edges are exactly equal)
    available = resrc_available_during_range (resource, 2500, 4000, false);
    rc = (rc || !(available == 0));
    //     (1 edge is exactly equal)
    available = resrc_available_during_range (resource, 3000, 5000, false);
    rc = (rc || !(available == 0));
    ok (!rc, "resrc_available_during_range: range overlaps 1 job works");
    rc = 0;

    // Range overlaps multiple job windows
    //     (no edges are exactly equal)
    available = resrc_available_during_range (resource, 100, 1500, false);
    rc = (rc || !(available == 4));
    available = resrc_available_during_range (resource, 1500, 2500, false);
    rc = (rc || !(available == 0));
    //     (some edges are exactly equal)
    available = resrc_available_during_range (resource, 1000, 2000, false);
    rc = (rc || !(available == 0));
    ok (!rc, "resrc_available_during_range: range overlaps multiple job works");
    rc = 0;

    // Range overlaps all job windows (edges exactly equal)
    available = resrc_available_during_range (resource, 0, 3000, false);
    rc = (rc || !(available == 0));
    available = resrc_available_during_range (resource, 0, 2000, false);
    rc = (rc || !(available == 0));
    // Range overlaps no job windows
    available = resrc_available_during_range (resource, 3001, 5000, false);
    rc = (rc || !(available == 10));
    ok (!rc, "resrc_available_during_range: range overlaps all job works");

    resrc_resource_destroy (resource);
}

static int test_a_resrc (resrc_t *resrc, bool rdl)
{
    int found = 0;
    int rc = 0;
    int verbose = 0;
    int64_t nowtime = epochtime ();
    JSON child_core = NULL;
    JSON o = NULL;
    JSON req_res = NULL;
    resrc_reqst_t *resrc_reqst = NULL;
    resrc_tree_list_t *deserialized_trees = NULL;
    resrc_tree_list_t *found_trees = resrc_tree_list_new ();
    resrc_tree_list_t *selected_trees;
    resrc_tree_t *deserialized_tree = NULL;
    resrc_tree_t *found_tree = NULL;
    resrc_tree_t *resrc_tree = NULL;

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
     *  Build a resource composite to search for.  Two variants are
     *  constructed depending on whether the loaded resources came
     *  from the sample RDL file or from the hwloc.  The hwloc request
     *  does not span multiple nodes or contain the localid property.
     */
    child_core = Jnew ();
    Jadd_str (child_core, "type", "core");
    req_res = Jnew ();
    Jadd_str (req_res, "type", "node");

    if (rdl) {
        JSON child_sock = Jnew ();
        JSON ja = Jnew_ar ();
        JSON jpropo = Jnew (); /* json property object */
        JSON memory = Jnew ();

        /* JSON jtago = Jnew ();  /\* json tag object *\/ */
        /* Jadd_bool (jtago, "maytag", true); */
        /* Jadd_bool (jtago, "yourtag", true); */

        Jadd_str (memory, "type", "memory");
        Jadd_int (memory, "req_qty", 1);
        Jadd_int (memory, "size", 100);
        json_object_array_add (ja, memory);

        Jadd_int (child_core, "req_qty", 6);
        Jadd_int (jpropo, "localid", 1);
        json_object_object_add (child_core, "properties", jpropo);
        json_object_array_add (ja, child_core);

        Jadd_str (child_sock, "type", "socket");
        Jadd_int (child_sock, "req_qty", 2);
        json_object_object_add (child_sock, "req_children", ja);

        Jadd_int (req_res, "req_qty", 2);
        /* json_object_object_add (req_res, "tags", jtago); */
        json_object_object_add (req_res, "req_child", child_sock);
    } else {
        Jadd_int (child_core, "req_qty", 2);
        Jadd_int (req_res, "req_qty", 1);
        json_object_object_add (req_res, "req_child", child_core);
    }

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

    init_time ();
    found = resrc_tree_search (resrc_tree_children (resrc_tree), resrc_reqst,
                               found_trees, false);

    ok (found, "found %d composite resources in %lf", found,
        ((double)get_time ())/1000000);
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

    o = Jnew_ar ();
    init_time ();
    rc = resrc_tree_list_serialize (o, found_trees);
    ok (!rc, "found resource serialization took: %lf",
        ((double)get_time ())/1000000);

    if (verbose) {
        printf ("The found resources serialized: %s\n", Jtostr (o));
    }

    deserialized_trees = resrc_tree_list_deserialize (o);
    if (verbose) {
        printf ("Listing deserialized trees\n");
        deserialized_tree = resrc_tree_list_first (deserialized_trees);
        while (deserialized_tree) {
            resrc_tree_print (deserialized_tree);
            deserialized_tree = resrc_tree_list_next (deserialized_trees);
       }
        printf ("End of deserialized trees\n");
    }
    Jput (o);

    init_time ();

    selected_trees = test_select_resources (found_trees, 1);
    rc = resrc_tree_list_allocate (selected_trees, 1, nowtime, nowtime + 3600);
    ok (!rc, "successfully allocated resources for job 1");
    resrc_tree_list_free (selected_trees);

    selected_trees = test_select_resources (found_trees, 2);
    rc = resrc_tree_list_allocate (selected_trees, 2, nowtime, nowtime + 3600);
    ok (!rc, "successfully allocated resources for job 2");
    resrc_tree_list_free (selected_trees);

    selected_trees = test_select_resources (found_trees, 3);
    rc = resrc_tree_list_allocate (selected_trees, 3, nowtime, nowtime + 3600);
    ok (!rc, "successfully allocated resources for job 3");
    resrc_tree_list_free (selected_trees);

    selected_trees = test_select_resources (found_trees, 4);
    rc = resrc_tree_list_reserve (selected_trees, 4, nowtime, nowtime + 3600);
    ok (!rc, "successfully reserved resources for job 4");
    resrc_tree_list_free (selected_trees);

    printf ("        allocate and reserve took: %lf\n",
            ((double)get_time ())/1000000);

    if (verbose) {
        printf ("Allocated and reserved resources\n");
        resrc_tree_print (resrc_tree);
    }

    init_time ();
    rc = resrc_tree_list_release (found_trees, 1);
    ok (!rc, "resource release of job 1 took: %lf",
        ((double)get_time ())/1000000);

    if (verbose) {
        printf ("Same resources without job 1\n");
        resrc_tree_print (resrc_tree);
    }

    init_time ();
    resrc_reqst_destroy (resrc_reqst);
    resrc_tree_list_destroy (deserialized_trees, true);
    resrc_tree_list_destroy (found_trees, false);
    resrc_tree_destroy (resrc_tree, true);
    printf ("        destroy took: %lf\n", ((double)get_time ())/1000000);
ret:
    return rc;
}

/*
 * These is a test of the resrc library.  The test uses two methods
 * for generating a resrc object.  The first loads an RDL-formatted
 * resource identified by the TESTRESRC_INPUT_FILE environment
 * variable.  The second option generates a resrc object from the node
 * it is running on using the hwloc library.  The test then conducts a
 * number of resrc library operations on each of these resrc objects.
 */
int main (int argc, char *argv[])
{
    char *buffer = NULL;
    char *filename = NULL;
    hwloc_topology_t topology;
    int buflen = 0;
    int rc1 = 1, rc2 = 1;
    resrc_t *resrc = NULL;

    plan (27 + num_temporal_allocation_tests);
    test_temporal_allocation ();

    if ((filename = getenv ("TESTRESRC_INPUT_FILE"))) {
        ok (!(filename == NULL || *filename == '\0'), "resource file provided");
        ok ((access (filename, F_OK) == 0), "resource file exists");
        ok ((access (filename, R_OK) == 0), "resource file readable");

        init_time();
        resrc = resrc_generate_rdl_resources (filename, "default");
        ok ((resrc != NULL), "resource generation from config file took: %lf",
            ((double)get_time())/1000000);
        if (resrc)
            rc1 = test_a_resrc (resrc, true);
    }

    init_time();
    ok ((hwloc_topology_init (&topology) == 0),
        "hwloc topology init succeeded");
    ok ((hwloc_topology_load (topology) == 0),
        "hwloc topology load succeeded");
    ok ((hwloc_topology_export_xmlbuffer (topology, &buffer, &buflen) == 0),
        "hwloc topology export succeeded");
    ok (((resrc = resrc_create_cluster ("cluster")) != 0),
        "cluster resource creation succeeded");
    ok ((resrc_generate_xml_resources (resrc, buffer, buflen, NULL) != 0),
        "resource generation from hwloc took: %lf",
        ((double)get_time())/1000000);
    hwloc_free_xmlbuffer (topology, buffer);
    hwloc_topology_destroy (topology);
    if (resrc)
        rc2 = test_a_resrc (resrc, false);

    done_testing ();
    return (rc1 | rc2);
}


/*
 * vi: ts=4 sw=4 expandtab
 */

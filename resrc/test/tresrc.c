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

#include "src/common/libutil/shortjansson.h"
#include "../resrc.h"
#include "../resrc_tree.h"
#include "../resrc_flow.h"
#include "../resrc_reqst.h"
#include "src/common/libtap/tap.h"


static struct timeval start_time;
int verbose = 0;

void init_time() {
    gettimeofday(&start_time, NULL);
}

u_int64_t get_time() {
    struct timeval t;
    gettimeofday(&t, NULL);
    return (u_int64_t) (t.tv_sec - start_time.tv_sec) * 1000000
        + (t.tv_usec - start_time.tv_usec);
}

/*
 * Select some resources from the found tree
 * frt is found resource tree
 * prt is selected resource tree parent
 */
static resrc_tree_t *test_select_resources (resrc_tree_t *frt, resrc_tree_t *prt,
                                            int select)
{
    resrc_tree_t *selected_res = NULL;

    if (frt) {
        resrc_t *resrc = resrc_tree_resrc (frt);

        if (!strcmp (resrc_type(resrc), "core")) {
            if (resrc_id (resrc) == select)
                resrc_stage_resrc (resrc, 1, NULL);
            else
                goto ret;
        } else if (!strcmp (resrc_type(resrc), "memory"))
            resrc_stage_resrc (resrc, 100, NULL);

        selected_res = resrc_tree_new (prt, resrc);
        if (resrc_tree_num_children (frt)) {
            resrc_tree_t *child = resrc_tree_list_first (
                resrc_tree_children (frt));
            while (child) {
                (void) test_select_resources (child, selected_res, select);
                child = resrc_tree_list_next (resrc_tree_children (frt));
            }
        }
    }
ret:
    return selected_res;
}

// Contains 10 tests
static int num_temporal_allocation_tests = 10;
static void test_temporal_allocation ()
{
    int rc = 0;
    size_t available;

    resrc_api_ctx_t *rsapi = resrc_api_init ();
    resrc_t *resource = resrc_new_resource (rsapi, "custom", "/test", "test",
                                            "test1", NULL, 1, NULL, 10);

    available = resrc_available_at_time (resource, 0);
    rc = (rc || !(available == 10));
    available = resrc_available_during_range (resource, 0, 1000, false);
    rc = (rc || !(available == 10));
    ok (!rc, "resrc_available...(time/range) on unallocated resource work");

    // Setup the resource allocations for the rest of the tests
    resrc_stage_resrc (resource, 5, NULL);
    rc = (rc || resrc_allocate_resource (resource, 1, 1, 1000));
    resrc_stage_resrc (resource, 10, NULL);
    rc = (rc || resrc_allocate_resource (resource, 2, 2000, 3000));
    ok (!rc, "Temporal allocation setup works");
    if (rc) {
        goto ret;
    }

    // Start the actual testing
    resrc_stage_resrc (resource, 1, NULL);
    // This should fail
    rc = (rc || !resrc_allocate_resource (resource, 3, 10, 3000));
    // This should work
    rc = (rc || resrc_allocate_resource (resource, 3, 10, 1999));
    ok (!rc, "Overlapping temporal allocations work");
    if (rc) {
        goto ret;
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
        goto ret;
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

ret:
    if (resource)
        resrc_resource_destroy (rsapi, resource);
    if (rsapi)
        resrc_api_fini (rsapi);
    return;
}

static int test_a_resrc (resrc_api_ctx_t *rsapi, resrc_t *resrc, bool rdl)
{
    int found = 0;
    int rc = 0;
    int64_t nowtime = epochtime ();
    json_t *o = NULL;
    json_t *req_res = NULL;
    resrc_reqst_t *resrc_reqst = NULL;
    resrc_tree_t *deserialized_tree = NULL;
    resrc_tree_t *found_tree = NULL;
    resrc_tree_t *resrc_tree = NULL;
    resrc_tree_t *selected_tree = NULL;

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
    req_res = Jnew ();

    if (rdl) {
        json_t *bandwidth = Jnew ();
        json_t *child_core = Jnew ();
        json_t *child_sock = Jnew ();
        json_t *graph_array = Jnew_ar ();
        json_t *ja = Jnew_ar ();
        json_t *jpropo = Jnew (); /* json property object */
        json_t *memory = Jnew ();
        json_t *power = Jnew ();

        /* json_t *jtago = Jnew ();  /\* json tag object *\/ */
        /* Jadd_bool (jtago, "maytag", true); */
        /* Jadd_bool (jtago, "yourtag", true); */

        Jadd_str (memory, "type", "memory");
        Jadd_int (memory, "req_qty", 1);
        Jadd_int (memory, "size", 100);
        json_array_append_new (ja, memory);

        Jadd_str (child_core, "type", "core");
        Jadd_int (child_core, "req_qty", 6);
        Jadd_bool (child_core, "exclusive", true);
        Jadd_int (jpropo, "localid", 1);
        json_object_set_new (child_core, "properties", jpropo);
        json_array_append_new (ja, child_core);

        Jadd_str (child_sock, "type", "socket");
        Jadd_int (child_sock, "req_qty", 2);
        json_object_set_new (child_sock, "req_children", ja);

        Jadd_str (bandwidth, "type", "bandwidth");
        Jadd_int (bandwidth, "size", 100);
        json_array_append_new (graph_array, bandwidth);

        Jadd_str (power, "type", "power");
        Jadd_int (power, "size", 10);
        json_array_append_new (graph_array, power);

        Jadd_str (req_res, "type", "node");
        Jadd_int (req_res, "req_qty", 2);
        Jadd_int64 (req_res, "starttime", nowtime);
        /* json_object_object_add (req_res, "tags", jtago); */
        json_object_set_new (req_res, "req_child", child_sock);
        json_object_set_new (req_res, "graphs", graph_array);
    } else {
        Jadd_str (req_res, "type", "core");
        Jadd_int (req_res, "req_qty", 2);
        Jadd_bool (req_res, "exclusive", true);
    }

    resrc_reqst = resrc_reqst_from_json (rsapi, req_res, NULL);
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
    found = resrc_tree_search (rsapi, resrc, resrc_reqst, &found_tree, true);

    ok (found, "found %d requested resources in %lf", found,
        ((double)get_time ())/1000000);
    if (!found)
        goto ret;

    if (verbose) {
        printf ("Listing found tree\n");
        resrc_tree_print (found_tree);
        printf ("End of found tree\n");
    }

    o = Jnew ();
    init_time ();
    rc = resrc_tree_serialize (o, found_tree);
    ok (!rc, "found resource serialization took: %lf",
        ((double)get_time ())/1000000);

    if (verbose) {
        printf ("The found resources serialized: %s\n", Jtostr (o));
    }

    json_t *gather = Jnew_ar ();
    json_t *reduce = Jnew ();
    resrc_api_map_t *gather_map = resrc_api_map_new ();
    resrc_api_map_put (gather_map, "node", (void *)(intptr_t)REDUCE_UNDER_ME);
    resrc_api_map_t *reduce_map = resrc_api_map_new ();
    resrc_api_map_put (reduce_map, "core", (void *)(intptr_t)NONE_UNDER_ME);
    resrc_api_map_put (reduce_map, "gpu", (void *)(intptr_t)NONE_UNDER_ME);

    init_time ();
    rc = resrc_tree_serialize_lite (gather, reduce, found_tree,
                                    gather_map, reduce_map);
    ok (!rc, "found resource lightweight serialization took: %lf",
        ((double)get_time ())/1000000);

    if (verbose) {
        printf ("The lightweight serialization: %s\n",
                json_dumps (gather, JSON_INDENT (3)));
    }
    Jput (gather);
    Jput (reduce);
    resrc_api_map_destroy (&gather_map);
    resrc_api_map_destroy (&reduce_map);

    /* serialized resrcs should be deserialized into a new API instance */
    resrc_api_ctx_t *new_rsapi = resrc_api_init ();
    deserialized_tree = resrc_tree_deserialize (new_rsapi, o, NULL);
    if (verbose) {
        printf ("Listing deserialized tree\n");
        resrc_tree_print (deserialized_tree);
        printf ("End of deserialized tree\n");
    }
    Jput (o);

    init_time ();

    /*
     * Exercise time-based allocations for the rdl case and
     * now-based allocations for the hwloc case
     */
    selected_tree = test_select_resources (found_tree, NULL, 1);
    if (rdl)
        rc = resrc_tree_allocate (selected_tree, 1, nowtime, nowtime + 3600);
    else
        rc = resrc_tree_allocate (selected_tree, 1, 0, 0);
    ok (!rc, "successfully allocated resources for job 1");
    resrc_tree_destroy (rsapi, selected_tree, false, false);
    resrc_tree_unstage_resources (found_tree);

    selected_tree = test_select_resources (found_tree, NULL, 2);
    if (rdl)
        rc = resrc_tree_allocate (selected_tree, 2, nowtime, nowtime + 3600);
    else
        rc = resrc_tree_allocate (selected_tree, 2, 0, 0);
    ok (!rc, "successfully allocated resources for job 2");
    resrc_tree_destroy (rsapi, selected_tree, false, false);
    resrc_tree_unstage_resources (found_tree);

    selected_tree = test_select_resources (found_tree, NULL, 3);
    if (rdl)
        rc = resrc_tree_allocate (selected_tree, 3, nowtime, nowtime + 3600);
    else
        rc = resrc_tree_allocate (selected_tree, 3, 0, 0);
    ok (!rc, "successfully allocated resources for job 3");
    resrc_tree_destroy (rsapi, selected_tree, false, false);
    resrc_tree_unstage_resources (found_tree);

    selected_tree = test_select_resources (found_tree, NULL, 4);
    if (rdl)
        rc = resrc_tree_reserve (selected_tree, 4, nowtime, nowtime + 3600);
    else
        rc = resrc_tree_reserve (selected_tree, 4, 0, 0);
    ok (!rc, "successfully reserved resources for job 4");
    resrc_tree_destroy (rsapi, selected_tree, false, false);
    resrc_tree_unstage_resources (found_tree);

    printf ("        allocate and reserve took: %lf\n",
            ((double)get_time ())/1000000);

    if (verbose) {
        printf ("Allocated and reserved resources\n");
        resrc_tree_print (resrc_tree);
    }

    init_time ();
    rc = resrc_tree_release (found_tree, 1);
    ok (!rc, "resource release of job 1 took: %lf",
        ((double)get_time ())/1000000);

    if (verbose) {
        printf ("Same resources without job 1\n");
        resrc_tree_print (resrc_tree);
    }

    init_time ();
    resrc_reqst_destroy (rsapi, resrc_reqst);
    resrc_tree_destroy (new_rsapi, deserialized_tree, true, true);
    resrc_tree_destroy (rsapi, found_tree, false, false);
    printf ("        destroy took: %lf\n", ((double)get_time ())/1000000);
ret:
    return rc;
}

static int test_using_reader_rdl ()
{
    int rc = 1;
    char *filename = NULL;
    resrc_api_ctx_t *rsapi = NULL;
    resrc_t *resrc = NULL;
    resrc_tree_t *phys_tree = NULL;
    resrc_flow_t *power_flow = NULL;
    resrc_flow_t *bw_flow = NULL;

    if (!(filename = getenv ("TESTRESRC_INPUT_FILE")))
        goto ret;

    ok (!(filename == NULL || *filename == '\0'), "resource file provided");
    ok ((access (filename, F_OK) == 0), "resource file exists");
    ok ((access (filename, R_OK) == 0), "resource file readable");
    init_time();

    if (!(rsapi = resrc_api_init ()))
        goto ret;

    /* Generate a hardware hierarchy of resources using RDL reader */
    resrc = resrc_generate_rdl_resources (rsapi, filename, "default");
    double e = (double)get_time()/1000000;
    ok ((resrc != NULL), "resource generation from config file took: %lf", e);
    if (!resrc)
        goto ret;
    phys_tree = resrc_tree_root (rsapi);

    /* Generate a power hierarchy of resources using RDL reader */
    if (!(power_flow = resrc_flow_generate_rdl (rsapi, filename, "power")))
        goto ret;
    else if (verbose) {
        printf ("Listing power tree\n");
        resrc_flow_print (power_flow);
        printf ("End of power tree\n");
    }

    /* Generate a bandwidth hierarchy of resources using RDL reader */
    if (!(bw_flow = resrc_flow_generate_rdl (rsapi, filename, "bandwidth")))
        goto ret;
    else if (verbose) {
        printf ("Listing bandwidth tree\n");
        resrc_flow_print (bw_flow);
        printf ("End of bandwidth tree\n");
    }

    rc = test_a_resrc (rsapi, resrc, true);

ret:
    if (rsapi) {
        if (bw_flow)
            resrc_flow_destroy (rsapi, bw_flow, true);
        if (power_flow)
            resrc_flow_destroy (rsapi, power_flow, true);
        if (phys_tree)
            resrc_tree_destroy (rsapi, phys_tree, true, true);
        resrc_api_fini (rsapi);
    }
    return rc;
}

static int test_using_reader_hwloc ()
{
    int rc = 1;
    hwloc_topology_t topology;
    resrc_api_ctx_t *rsapi = NULL;
    resrc_tree_t *phys_tree = NULL;

    init_time();

    if (!(rsapi = resrc_api_init ()))
        goto ret;

    ok ((hwloc_topology_init (&topology) == 0),
        "hwloc topology init succeeded");
    ok ((hwloc_topology_load (topology) == 0),
        "hwloc topology load succeeded");
    /* Generate the hardware hierarchy of resources using hwloc reader */
    ok ((resrc_generate_hwloc_resources (rsapi, topology, NULL, NULL) != 0),
        "resource generation from hwloc took: %lf",
        ((double)get_time())/1000000);
    hwloc_topology_destroy (topology);

    phys_tree = resrc_tree_root (rsapi);
    if (!phys_tree)
        goto ret;

    rc = test_a_resrc (rsapi, resrc_tree_resrc (phys_tree), false);

ret:
    if (rsapi) {
        if (phys_tree)
            resrc_tree_destroy (rsapi, phys_tree, true, true);
        resrc_api_fini (rsapi);
    }
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
    int rc1 = 1, rc2 = 1;

    plan (27 + num_temporal_allocation_tests);
    test_temporal_allocation ();
    rc1 = test_using_reader_rdl ();
    rc2 = test_using_reader_hwloc ();
    /* plance holder for testing with other reader types */
    done_testing ();

    return (rc1 | rc2);
}


/*
 * vi: ts=4 sw=4 expandtab
 */

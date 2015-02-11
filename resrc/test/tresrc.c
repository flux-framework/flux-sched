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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <uuid/uuid.h>
#include <czmq.h>

#include "../resrc.h"
#include "../resrc_tree.h"
#include "src/common/libutil/jsonutil.h"
#include "src/common/libutil/xzmalloc.h"

#include <sys/time.h>
#include <sys/types.h>

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

int main (int argc, char** argv)
{
    char *resrc_id = NULL;
    const char *filename = argv[1];
    int found = 0;
    JSON child_sock = NULL;
    JSON child_core = NULL;
    JSON o = NULL;
    JSON req_res = NULL;
    resrc_t *resrc = NULL;
    resrc_tree_t *resrc_tree = NULL;
    resources_t *resrcs;
    resource_list_t *found_res = resrc_new_id_list ();

    if (filename == NULL || *filename == '\0')
        filename = getenv ("TESTRESRC_INPUT_FILE");

    init_time();
    resrcs = resrc_generate_resources (filename, "default");
    printf("resource generation took: %lf\n", ((double)get_time())/1000000);
    printf ("starting\n");
    resrc_print_resources (resrcs);
    printf ("end of resources\n");

    printf ("printing resource tree\n");
    resrc = zhash_lookup ((zhash_t *)resrcs, "cluster.0");
    if (resrc)
        resrc_tree_print (resrc_phys_tree (resrc));
    else
        printf ("Failed to find cluster.0 resource\n");
    printf ("end of resource tree\n");

    child_core = Jnew ();
    Jadd_str (child_core, "type", "core");
    Jadd_int (child_core, "req_qty", 6);

    child_sock = Jnew ();
    Jadd_str (child_sock, "type", "socket");
    Jadd_int (child_sock, "req_qty", 4);
    json_object_object_add (child_sock, "req_child", child_core);

    req_res = Jnew ();
    Jadd_str (req_res, "type", "node");
    Jadd_int (req_res, "req_qty", 2);
    json_object_object_add (req_res, "req_child", child_sock);

    resrc_tree = resrc_phys_tree (resrc);
    init_time();
    found = resrc_tree_search (resrc_tree_children (resrc_tree), found_res,
                               req_res, false);
    if (found) {
        printf ("Found %d composite resources in %lf\n", found,
                ((double)get_time())/1000000);
        resrc_id = resrc_list_first (found_res);
        while (resrc_id) {
            printf ("resrc_id %s\n", resrc_id);
            resrc_id = resrc_list_next (found_res);
        }
    }
    Jput (req_res);

    init_time();
    o = resrc_serialize (resrcs, found_res);
    printf ("Found resource serialization took: %lf\n",
            ((double)get_time())/1000000);
    printf ("The found resources serialized: %s\n", Jtostr (o));
    Jput (o);

    init_time();
    resrc_allocate_resources (resrcs, found_res, 1);
    resrc_allocate_resources (resrcs, found_res, 2);
    resrc_allocate_resources (resrcs, found_res, 3);
    resrc_reserve_resources (resrcs, found_res, 4);
    printf ("allocated\n");
    printf("allocate and reserve took: %lf\n", ((double)get_time())/1000000);
    /* resrc_print_resources (resrcs); */
    init_time();
    resrc_release_resources (resrcs, found_res, 1);
    printf ("released\n");
    printf("release took: %lf\n", ((double)get_time())/1000000);
    init_time();
    resrc_id_list_destroy (found_res);
    /* resrc_print_resources (resrcs); */

    resrc_destroy_resources (&resrcs);
    printf("destroy took: %lf\n", ((double)get_time())/1000000);

    return 0;
}


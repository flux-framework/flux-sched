#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <uuid/uuid.h>
#include <czmq.h>

#include "resrc.h"
#include "src/common/libutil/jsonutil.h"
#include "src/common/libutil/xzmalloc.h"

int main (int argc, char** argv)
{
    char *resrc_id;
    const char *filename = argv[1];
    int found = 0;
    zhash_t *resrcs;
    zlist_t *found_res = zlist_new ();

    if (filename == NULL || *filename == '\0')
        filename = getenv ("TESTRESRC_INPUT_FILE");

    resrcs = resrc_generate_resources (filename, "default");
    printf ("starting\n");
    resrc_print_resources (resrcs);
    found = resrc_find_resources (resrcs, found_res, "node", false);
    if (found) {
        printf ("found\n");
        resrc_id = zlist_first (found_res);
        while (resrc_id) {
            printf ("resrc_id %s\n", resrc_id);
            resrc_id = zlist_next (found_res);
        }
    }

    resrc_allocate_resources (resrcs, found_res, 1);
    resrc_allocate_resources (resrcs, found_res, 2);
    resrc_allocate_resources (resrcs, found_res, 3);
    resrc_reserve_resources (resrcs, found_res, 4);
    printf ("allocated\n");
    resrc_print_resources (resrcs);
    resrc_release_resources (resrcs, found_res, 1);
    printf ("released\n");
    resrc_id_list_destroy (found_res);
    resrc_print_resources (resrcs);

    zhash_destroy (&resrcs);

    return 0;
}


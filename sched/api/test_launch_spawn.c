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

#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <limits.h>

#include "flux_api.h"

#define NNODES          1
#define NPROCS_PER_NODE 4
#define TARGET_TESTER   "test_sleeper"

static char *
tester_sleeptime = "180";

int 
main (int argc, char *argv[])
{
    flux_rc_e rc;
    flux_lwj_id_t lwj;
    flux_lwj_status_e status;
    flux_lwj_info_t lwjInfo; 
    int sync = 0;
    size_t size = 0;
    size_t rsize = 0;
    char * lwjargv[3]; 
    char cwd[PATH_MAX] = {'\0'};
    char tmpbuf[PATH_MAX] = {'\0'};
    MPIR_PROCDESC_EXT *proctable = NULL;

    set_verbose_level (1);
    if ( getenv ("DEBUG_FLUXAPI") != NULL ) {
        set_verbose_level (3);
    }

    if (argc > 1) {
        tester_sleeptime = strdup(argv[1]);
    }
    
    if ( ( rc = FLUX_init () != FLUX_OK )) {
	error_log ("Test Failed: ", 0,
	    "FLUX_init failed.");
	return EXIT_FAILURE;
    }

    if (argc > 2) {
	error_log ("Usage: test_launch_spawn [sync]", 0);

	return EXIT_FAILURE;
    }

    if ( (argc == 2 )
	 && (strcmp (argv[1], "sync") == 0) ) {
	sync = 1;
    }
    else {
	error_log ("Test Warning: "
	    "sync flag is not understood. Ignore %s", 1,
	    argv[1]);	
    }

    if ( ( rc = FLUX_update_createLWJCxt(&lwj)) 
	 != FLUX_OK ) {
	error_log ("Test Failed: "
	    "FLUX_update_createLWJCx returned an error.", 0);

	return EXIT_FAILURE;
    }

    error_log ("jobid: %ld ", 1, lwj);

    if ( ( rc = FLUX_query_LWJId2JobInfo (
                    &lwj, &lwjInfo) ) != FLUX_OK ) {
	error_log (
            "Test Failed: FLUX_query_LWJId2JobInfo "
            "returned an error.",
            0);
	
	return EXIT_FAILURE;
    }

    if ( getcwd (cwd, sizeof(cwd)) == NULL) {
	error_log (
            "Test Failed: Can't get cwd", 0);

        return EXIT_FAILURE;
    }

    snprintf (tmpbuf, PATH_MAX,
        "%s/%s", 
        cwd, 
        TARGET_TESTER);
    lwjargv[0] = strdup (tmpbuf);
    lwjargv[1] = strdup (tester_sleeptime);
    lwjargv[2] = NULL;

    rc = FLUX_launch_spawn (
             &lwj,
             sync,
             NULL, 
	     lwjargv[0], 
             lwjargv,
	     0,
             NNODES,
             NPROCS_PER_NODE);

    sleep(5);

    if ( rc != FLUX_OK ) {
	error_log ("Test Failed: "
	    "FLUX_launch_spawn returned an error.", 0);
	
	return EXIT_FAILURE;
    }

    if ( ( rc = FLUX_query_LWJStatus (
                    &lwj, &status)) != FLUX_OK ) {
	error_log ("Test Failed: "
	    "FLUX_query_LWJStatus returned an error.", 0);
	
	return EXIT_FAILURE;
    }
    
    if ( ( (sync == 0) 
	   && ((status != status_spawned_running)
               && (status != status_running)) )
	|| ( (sync == 1)
	     && (status != status_spawned_stopped)) ) {
	error_log ("Test Failed: "
	    "FLUX_query_LWJStatus returned "
            "an incorrect status.", 0);
	
	return EXIT_FAILURE;
    }

    if ( ( rc = FLUX_query_globalProcTableSize (
                    &lwj, &size)) != FLUX_OK ) {
	error_log ("Test Failed: "
	    "FLUX_query_globalProcTableSize "
            "returned an error.",0);
	
	return EXIT_FAILURE;
    }

    proctable 
	= (MPIR_PROCDESC_EXT *) 
	  malloc (size * sizeof (MPIR_PROCDESC_EXT));

    if ( ( rc = FLUX_query_globalProcTable (
                    &lwj, proctable, 
                    size, &rsize)) != FLUX_OK ) {
	error_log ("Test Failed: "
	    "FLUX_query_globalProcTable "
            "returned an error.", 0);
	
	return EXIT_FAILURE;
    }

    int i;
    for (i=0; i < size; ++i) {
	error_log ("=====================================", 1);
	error_log ("executable: %s", 1, proctable[i].pd.executable_name);
	error_log ("hostname: %s", 1, proctable[i].pd.host_name);
	error_log ("pid: %d", 1, proctable[i].pd.pid);
	error_log ("mpirank: %d", 1, proctable[i].mpirank);
	error_log ("cnodeid: %d", 1, proctable[i].cnodeid);
    }
    error_log ("=====================================", 1);

    if ( (rc = FLUX_fini()) != FLUX_OK ) {
	error_log ("Test failed:"
            "FLUX finale failedTest Passed", 0);
        return EXIT_FAILURE;
    }

    error_log ("Test Passed", 1);

    return EXIT_SUCCESS;
}

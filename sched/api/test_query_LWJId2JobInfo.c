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
#include "flux_api_mockup.h"

int main (int argc, char *argv[])
{
    flux_rc_e rc;
    flux_lwj_id_t lwj;
    flux_lwj_info_t lwjInfo;

    if ( ( rc = FLUX_init () != FLUX_OK )) {
	error_log ("Test Failed: "
	    "FLUX_init failed.");
	return EXIT_FAILURE;
    }

    if ( ( rc = FLUX_query_pid2LWJId (FLUX_MOCKUP_HOSTNAME,
				      FLUX_MOCKUP_PID,
				      &lwj) ) != FLUX_OK ) {
	error_log ("Test Failed: "
	    "FLUX_query_pid2LWJId returned an error.");

	return EXIT_FAILURE;
    }

    if ( ( rc = FLUX_query_LWJId2JobInfo (&lwj, 
					  &lwjInfo) ) != FLUX_OK ) {
	error_log ("Test Failed: "
	    "FLUX_query_LWJId2JobInfo returned an error.");
	
	return EXIT_FAILURE;
    }

    if ( (lwjInfo.status != FLUX_MOCKUP_STATUS)
	 || (strcmp (lwjInfo.hn, FLUX_MOCKUP_HOSTNAME) != 0)
	 || (lwjInfo.pid != FLUX_MOCKUP_PID)
	 || (lwjInfo.procTableSize != 1)
	 || (lwjInfo.procTable == NULL) ) {
	
	error_log ("Test Failed: "
	    "FLUX_query_LWJId2JobInfo returned incorrect info.");
	
	return EXIT_FAILURE;
    }
    else {
        error_log ("Test Passed");
    }

    return EXIT_SUCCESS;
}

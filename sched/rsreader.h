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

#ifndef RSREADER_H
#define RSREADER_H 1

#include <stdint.h>
#include <hwloc.h>
#include "resrc.h"
#include "rs2rank.h"

typedef enum {
     RSREADER_RESRC_EMUL,
     RSREADER_RESRC,
     RSREADER_HWLOC,
     RSREADER_FOR_RENT
} rsreader_t;

/* Bulk-load the resources specified in an rdl file (rdl_path).
 * After successful return from this call, the root of the loaded
 * resrc objects can be accessed through resrc_tree_root (rapi).
 * uri is optional; if NULL, "default" uri is used.
 */
int rsreader_resrc_bulkload (resrc_api_ctx_t *rapi,
        const char *rdl_path, char *uri);

/* Placehodler for hwloc resource data bulk loading
 * Not Yet Implemented.
 */
int rsreader_hwloc_bulkload (resrc_api_ctx_t *rapi, const char *hw_xml,
        size_t len, rsreader_t r_mode, machs_t *machs);

/* Placehodler for individual resrc data loading
 * Not Yet Implemented.
 */
int rsreader_resrc_load (resrc_api_ctx_t *rapi, const char *path,
        char *uri, uint32_t rank);

 /* The main purpose of this function is to process the hwloc
  * resource xml buffer of one broker rank (rank) to fill in
  * the rs2rank table (machs: please see rs2rank.h).
  * Optinally, it loads the hwloc resource xml buffer and makes
  * it available through rapi handle if r_mode is RSREADER_RESRC.
  */
int rsreader_hwloc_load (resrc_api_ctx_t *rapi, const char *hw_xml, size_t len,
        uint32_t rank, rsreader_t r_mode, machs_t *machs, char **err_str);

/* Traverse the entire resrc hierarchy and link each node-type
 * resrc_t object with a broker rank by filling in its digest field.
 * Its hostname and digest of the resrc object are later used
 * to determine the managing broker. If err_str is not null,
 * this routine returns an error message along with a non-zero rc.
 */
int rsreader_link2rank (resrc_api_ctx_t *rapi, machs_t *machs, char **err_str);

/* For testing such as emulation whereby no real mapping may
 * exist between the physcal broker rank and testing resource
 * hierarchy, this function should be used to force us to associate
 * each and all of the node-type resrc_t object with a single
 * broker rank.
 */
int rsreader_force_link2rank (resrc_api_ctx_t *rapi, machs_t *machs);

#endif /* RSREADER_H */

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

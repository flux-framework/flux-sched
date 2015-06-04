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

/*
 * schedsrv.h - common data structure for scheduler framework and plugins
 *
 * Update Log:
 *       Apr 08 2015 DHA: Code refactoring w/ JSC API
 * 	     May 24 2014 DHA: File created.
 */

#ifndef SCHEDSRV_H
#define SCHEDSRV_H 1

#include <stdint.h>
#include <czmq.h>

#include "resrc.h"


/**
 *  Defines resource request info block.
 *  This needs to be expanded as Flux resources evolve.
 */
typedef struct flux_resources {
    uint64_t nnodes; /*!< num of nodes requested by a job */
    uint64_t ncores; /*!< num of cores requested by a job */
    uint64_t corespernode; /*!< num of cores per node requested by a job */
} flux_res_t;


/**
 *  Defines LWJ info block (this needs to be expanded of course)
 */
typedef struct {
    int64_t     lwj_id;  /*!< LWJ id */
    job_state_t state;   /*!< current job state */
    bool        reserve; /*!< reserve resources for job if true */
    flux_res_t *req;     /*!< resources requested by this LWJ */
    resrc_tree_list_t resrc_trees; /*!< resources allocated to this LWJ */
} flux_lwj_t;

#endif /* SCHEDSRV_H */

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

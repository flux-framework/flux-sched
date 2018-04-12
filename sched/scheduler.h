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
 * scheduler.h - common data structure and macroes
 *    for scheduler framework and plugins
 */

#ifndef SCHEDULER_H
#define SCHEDULER_H 1

#include <stdint.h>
#include <flux/core.h>

#include "resrc.h"


/**
 *  Defines resource request info block.
 *  This needs to be expanded as Flux resources evolve.
 */
typedef struct flux_resources {
    int64_t nnodes; /*!< num of nodes requested by a job */
    int64_t ncores; /*!< num of cores requested by a job */
    int64_t ngpus; /*!< num of cores requested by a job */
    int64_t corespernode; /*!< num of cores per node requested by a job */
    int64_t walltime; /*!< walltime requested by a job */
    bool     node_exclusive; /*!< job requires exclusive use of node if true */
} flux_res_t;


/**
 *  Defines LWJ info block (this needs to be expanded of course)
 */
typedef struct {
    int64_t     lwj_id;  /*!< LWJ id */
    job_state_t state;   /*!< current job state */
    char    *user;       /*!< user name of person who submitted job */
    char    *account;    /*!< account to charge for resource usage */
    flux_res_t *req;     /*!< resources requested by this LWJ */
    resrc_tree_t *resrc_tree; /*!< resources allocated to this LWJ */
    int64_t submittime;
    int64_t starttime;
    int64_t endtime;
    int64_t enqueue_pos; /*!< the initial enqueue position */
    double user_prio;    /*!< user-requested priority */
    double priority;     /*!< scheduling priority */
} flux_lwj_t;

/**
 *  Defines the properties of the scheduler plugin
 */
struct sched_prop {
    bool out_of_order_capable; ;   /*!< true if out of order scheduling*/
};

/**
 *  Defines parameters that control scheduling optimization
 */
typedef struct {
    long queue_depth;        /* max njobs to consider per sched event */
    bool delay_sched;        /* delay scheduling on individual job event */
} sched_params_t;


/*
 * The following defines the default values for all of the scheduling
 * parameters and should be used by both scheduler framework service
 * comms module and the scheduling plug-ins. If the default values
 * are overriden via other mechanisms (e.g., sched load options), the
 * new values also need to be passed down to the scheduling
 * plug-ins.
 */
#define SCHED_PARAM_Q_DEPTH_DEFAULT 1024
#define SCHED_PARAM_DELAY_DEFAULT true

const sched_params_t *sched_params_get (flux_t *h);

#endif /* SCHEDULER_H */

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

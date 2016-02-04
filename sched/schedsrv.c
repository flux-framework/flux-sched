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
 * schedsrv.c - scheduler framework service comms module
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <errno.h>
#include <libgen.h>
#include <czmq.h>
#include <json.h>
#include <dlfcn.h>
#include <stdbool.h>
#include <flux/core.h>

#include "src/common/libutil/jsonutil.h"
#include "src/common/libutil/log.h"
#include "src/common/libutil/xzmalloc.h"
#include "resrc.h"
#include "resrc_tree.h"
#include "resrc_reqst.h"
#include "rs2rank.h"
#include "rsreader.h"
#include "schedsrv.h"
#include "../simulator/simulator.h"

#define DYNAMIC_SCHEDULING 0
#define ENABLE_TIMER_EVENT 0
#define SCHED_UNIMPL -1

#if ENABLE_TIMER_EVENT
static int timer_event_cb (flux_t h, void *arg);
#endif
static void res_event_cb (flux_t h, flux_msg_handler_t *w,
                          const flux_msg_t *msg, void *arg);
static int job_status_cb (const char *jcbstr, void *arg, int errnum);


/******************************************************************************
 *                                                                            *
 *              Scheduler Framework Service Module Context                    *
 *                                                                            *
 ******************************************************************************/

typedef int (*setup_f) (flux_t h);

typedef resrc_tree_list_t *(*find_f) (flux_t h, resrc_t *resrc,
                                      resrc_reqst_t *resrc_reqst);

typedef resrc_tree_list_t *(*sel_f) (flux_t h, resrc_tree_list_t *resrc_trees,
                                     resrc_reqst_t *resrc_reqst);

typedef int (*alloc_f) (flux_t h, resrc_tree_list_t *rtl, int64_t job_id,
                        int64_t starttime, int64_t endtime);

typedef int (*reserv_f) (flux_t h, resrc_tree_list_t *rtl, int64_t job_id,
                         int64_t starttime, int64_t walltime, resrc_t *resrc,
                         resrc_reqst_t *resrc_reqst);

typedef struct sched_ops {
    void         *dso;                /* Scheduler plug-in DSO handle */
    setup_f       sched_loop_setup;   /* prepare to run through a sched cycle */
    find_f        find_resources;     /* find resources that match request */
    sel_f         select_resources;   /* select the best resources */
    alloc_f       allocate_resources; /* allocate those resources */
    reserv_f      reserve_resources;  /* or reserve them */
} sched_ops_t;

typedef struct {
    JSON          jcb;
    void         *arg;
    int           errnum;
} jsc_event_t;

typedef struct {
    flux_t        h;
    void         *arg;
} res_event_t;

typedef struct {
    bool          in_sim;
    sim_state_t  *sim_state;
    zlist_t      *res_queue;
    zlist_t      *jsc_queue;
    zlist_t      *timer_queue;
} simctx_t;

typedef struct {
    resrc_t      *root_resrc;         /* resrc object pointing to the root */
    char         *root_uri;           /* Name of the root of the RDL hierachy */
} rdlctx_t;

typedef struct {
    char         *path;
    char         *uri;
    char         *userplugin;
    bool          sim;
    bool          schedonce;          /* Use resources only once */
    int           verbosity;
    rsreader_t    r_mode;
} ssrvarg_t;

/* TODO: Implement prioritization function for p_queue */
typedef struct {
    flux_t        h;
    zlist_t      *p_queue;            /* Pending job priority queue */
    zlist_t      *r_queue;            /* Running job queue */
    zlist_t      *c_queue;            /* Complete/cancelled job queue */
    machs_t      *machs;              /* Helps resolve resources to ranks */
    ssrvarg_t     arg;                /* args passed to this module */
    rdlctx_t      rctx;               /* RDL context */
    simctx_t      sctx;               /* simulator context */
    sched_ops_t   sops;               /* scheduler plugin operations */
} ssrvctx_t;

/******************************************************************************
 *                                                                            *
 *                                 Utilities                                  *
 *                                                                            *
 ******************************************************************************/

static inline void ssrvarg_init (ssrvarg_t *arg)
{
    arg->path = NULL;
    arg->uri = NULL;
    arg->userplugin = NULL;
    arg->sim = false;
    arg->schedonce = false;
    arg->verbosity = 0;
}

static inline void ssrvarg_free (ssrvarg_t *arg)
{
    if (arg->path)
        free (arg->path);
    if (arg->uri)
        free (arg->uri);
    if (arg->userplugin)
        free (arg->userplugin);
}

static inline int ssrvarg_process_args (int argc, char **argv, ssrvarg_t *a)
{
    int i = 0, rc = 0;
    char *schedonce = NULL;
    char *vlevel= NULL;
    char *sim = NULL;
    for (i = 0; i < argc; i++) {
        if (!strncmp ("rdl-conf=", argv[i], sizeof ("rdl-conf"))) {
            a->path = xstrdup (strstr (argv[i], "=") + 1);
        } else if (!strncmp ("sched-once=", argv[i], sizeof ("sched-once"))) {
            schedonce = xstrdup (strstr (argv[i], "=") + 1);
        } else if (!strncmp ("verbosity=", argv[i], sizeof ("verbosity"))) {
            vlevel = xstrdup (strstr (argv[i], "=") + 1);
        } else if (!strncmp ("rdl-resource=", argv[i], sizeof ("rdl-resource"))) {
            a->uri = xstrdup (strstr (argv[i], "=") + 1);
        } else if (!strncmp ("in-sim=", argv[i], sizeof ("in-sim"))) {
            sim = xstrdup (strstr (argv[i], "=") + 1);
        } else if (!strncmp ("plugin=", argv[i], sizeof ("plugin"))) {
            a->userplugin = xstrdup (strstr (argv[i], "=") + 1);
        } else {
            rc = -1;
            errno = EINVAL;
            goto done;
        }
    }

    if (!(a->userplugin))
        a->userplugin = xstrdup ("sched.plugin1");

    if (sim && !strncmp (sim, "true", sizeof ("true"))) {
        a->sim = true;
        free (sim);
    }
    if (schedonce && !strncmp (schedonce, "true", sizeof ("true"))) {
        a->schedonce = true;
        free (schedonce);
    }
    if (vlevel) {
         a->verbosity = strtol(vlevel, (char **)NULL, 10);
         free (vlevel);
    }
    if (a->path)
        a->r_mode = (a->sim)? RSREADER_RESRC_EMUL : RSREADER_RESRC;
    else
        a->r_mode = RSREADER_HWLOC;
done:
    return rc;
}

static void freectx (void *arg)
{
    ssrvctx_t *ctx = arg;
    zlist_destroy (&(ctx->p_queue));
    zlist_destroy (&(ctx->r_queue));
    zlist_destroy (&(ctx->c_queue));
    rs2rank_tab_destroy (ctx->machs);
    ssrvarg_free (&(ctx->arg));
    resrc_tree_destroy (resrc_phys_tree (ctx->rctx.root_resrc), true);
    free (ctx->rctx.root_uri);
    free (ctx->sctx.sim_state);
    if (ctx->sctx.res_queue)
        zlist_destroy (&(ctx->sctx.res_queue));
    if (ctx->sctx.jsc_queue)
        zlist_destroy (&(ctx->sctx.jsc_queue));
    if (ctx->sctx.timer_queue)
        zlist_destroy (&(ctx->sctx.timer_queue));
    if (ctx->sops.dso)
        dlclose (ctx->sops.dso);
}

static ssrvctx_t *getctx (flux_t h)
{
    ssrvctx_t *ctx = (ssrvctx_t *)flux_aux_get (h, "schedsrv");
    if (!ctx) {
        ctx = xzmalloc (sizeof (*ctx));
        ctx->h = h;
        if (!(ctx->p_queue = zlist_new ()))
            oom ();
        if (!(ctx->r_queue = zlist_new ()))
            oom ();
        if (!(ctx->c_queue = zlist_new ()))
            oom ();
        if (!(ctx->machs = rs2rank_tab_new ()))
            oom ();
        ssrvarg_init (&(ctx->arg));
        ctx->rctx.root_resrc = NULL;
        ctx->rctx.root_uri = NULL;
        ctx->sctx.in_sim = false;
        ctx->sctx.sim_state = NULL;
        ctx->sctx.res_queue = NULL;
        ctx->sctx.jsc_queue = NULL;
        ctx->sctx.timer_queue = NULL;
        ctx->sops.dso = NULL;
        ctx->sops.sched_loop_setup = NULL;
        ctx->sops.find_resources = NULL;
        ctx->sops.select_resources = NULL;
        ctx->sops.allocate_resources = NULL;
        ctx->sops.reserve_resources = NULL;
        flux_aux_set (h, "schedsrv", ctx, freectx);
    }
    return ctx;
}

static inline void get_jobid (JSON jcb, int64_t *jid)
{
    Jget_int64 (jcb, JSC_JOBID, jid);
}

static inline void get_states (JSON jcb, int64_t *os, int64_t *ns)
{
    JSON o = NULL;
    Jget_obj (jcb, JSC_STATE_PAIR, &o);
    Jget_int64 (o, JSC_STATE_PAIR_OSTATE, os);
    Jget_int64 (o, JSC_STATE_PAIR_NSTATE, ns);
}

static inline int fill_resource_req (flux_t h, flux_lwj_t *j)
{
    char *jcbstr = NULL;
    int rc = -1;
    int64_t nn = 0;
    int64_t nc = 0;
    int64_t walltime = 0;
    JSON jcb = NULL;
    JSON o = NULL;

    if (!j) goto done;

    j->req = (flux_res_t *) xzmalloc (sizeof (flux_res_t));
    if ((rc = jsc_query_jcb (h, j->lwj_id, JSC_RDESC, &jcbstr)) != 0) {
        flux_log (h, LOG_ERR, "error in jsc_query_jcb.");
        goto done;
    }
    if (!(jcb = Jfromstr (jcbstr))) {
        flux_log (h, LOG_ERR, "fill_resource_req: error parsing JSON string");
        goto done;
    }
    if (!Jget_obj (jcb, JSC_RDESC, &o)) goto done;
    if (!Jget_int64 (o, JSC_RDESC_NNODES, &nn)) goto done;
    if (!Jget_int64 (o, JSC_RDESC_NTASKS, &nc)) goto done;
    j->req->nnodes = (uint64_t) nn;
    j->req->ncores = (uint64_t) nc;
    if (!Jget_int64 (o, JSC_RDESC_WALLTIME, &walltime)) {
        j->req->walltime = (uint64_t) 3600;
    } else {
        j->req->walltime = (uint64_t) walltime;
    }
    j->req->node_exclusive = false;
    rc = 0;
done:
    if (jcb)
        Jput (jcb);
    return rc;
}

static int update_state (flux_t h, uint64_t jid, job_state_t os, job_state_t ns)
{
    const char *jcbstr = NULL;
    int rc = -1;
    JSON jcb = Jnew ();
    JSON o = Jnew ();
    Jadd_int64 (o, JSC_STATE_PAIR_OSTATE, (int64_t) os);
    Jadd_int64 (o, JSC_STATE_PAIR_NSTATE , (int64_t) ns);
    /* don't want to use Jadd_obj because I want to transfer the ownership */
    json_object_object_add (jcb, JSC_STATE_PAIR, o);
    jcbstr = Jtostr (jcb);
    rc = jsc_update_jcb (h, jid, JSC_STATE_PAIR, jcbstr);
    Jput (jcb);
    return rc;
}

static inline bool is_newjob (JSON jcb)
{
    int64_t os = J_NULL, ns = J_NULL;
    get_states (jcb, &os, &ns);
    return ((os == J_NULL) && (ns == J_NULL))? true : false;
}


/********************************************************************************
 *                                                                              *
 *                          Simple Job Queue Methods                            *
 *                                                                              *
 *******************************************************************************/

static int append_to_pqueue (ssrvctx_t *ctx, JSON jcb)
{
    int rc = -1;
    int64_t jid = -1;
    flux_lwj_t *job = NULL;

    get_jobid (jcb, &jid);
    if ( !(job = (flux_lwj_t *) xzmalloc (sizeof (*job))))
        oom ();

    job->lwj_id = jid;
    job->state = J_NULL;
    if (zlist_append (ctx->p_queue, job) != 0) {
        flux_log (ctx->h, LOG_ERR, "failed to append to pending job queue.");
        goto done;
    }
    rc = 0;
done:
    return rc;
}

static flux_lwj_t *q_find_job (ssrvctx_t *ctx, int64_t id)
{
    flux_lwj_t *j = NULL;
    /* NOTE: performance issue when we have
     * large numbers of jobs in the system?
     */
    for (j = zlist_first (ctx->p_queue); j; j = zlist_next (ctx->p_queue))
        if (j->lwj_id == id)
            return j;
    for (j = zlist_first (ctx->r_queue); j; j = zlist_next (ctx->r_queue))
        if (j->lwj_id == id)
            return j;
    for (j = zlist_first (ctx->c_queue); j; j = zlist_next (ctx->c_queue))
        if (j->lwj_id == id)
            return j;
    return NULL;
}

static int q_move_to_rqueue (ssrvctx_t *ctx, flux_lwj_t *j)
{
    zlist_remove (ctx->p_queue, j);
    return zlist_append (ctx->r_queue, j);
}

static int q_move_to_cqueue (ssrvctx_t *ctx, flux_lwj_t *j)
{
    /* NOTE: performance issue? */
    // FIXME: no transition from pending queue to cqueue yet
    //zlist_remove (ctx->p_queue, j);
    zlist_remove (ctx->r_queue, j);
    return zlist_append (ctx->c_queue, j);
}

static flux_lwj_t *fetch_job_and_event (ssrvctx_t *ctx, JSON jcb,
                                        job_state_t *ns)
{
    int64_t jid = -1, os64 = 0, ns64 = 0;
    get_jobid (jcb, &jid);
    get_states (jcb, &os64, &ns64);
    *ns = (job_state_t) ns64;
    return q_find_job (ctx, jid);
}


/******************************************************************************
 *                                                                            *
 *                            Scheduler Plugin Loader                         *
 *                                                                            *
 ******************************************************************************/

static int resolve_functions (ssrvctx_t *ctx)
{
    int rc = -1;

    ctx->sops.sched_loop_setup = dlsym (ctx->sops.dso, "sched_loop_setup");
    if (!(ctx->sops.sched_loop_setup) || !(*(ctx->sops.sched_loop_setup))) {
        flux_log (ctx->h, LOG_ERR, "can't load sched_loop_setup: %s",
                  dlerror ());
        goto done;
    }
    ctx->sops.find_resources = dlsym (ctx->sops.dso, "find_resources");
    if (!(ctx->sops.find_resources) || !(*(ctx->sops.find_resources))) {
        flux_log (ctx->h, LOG_ERR, "can't load find_resources: %s", dlerror ());
        goto done;
    }
    ctx->sops.select_resources = dlsym (ctx->sops.dso, "select_resources");
    if (!(ctx->sops.select_resources) || !(*(ctx->sops.select_resources))) {
        flux_log (ctx->h, LOG_ERR, "can't load select_resources: %s",
                  dlerror ());
        goto done;
    }
    ctx->sops.allocate_resources = dlsym (ctx->sops.dso, "allocate_resources");
    if (!(ctx->sops.allocate_resources) || !(*(ctx->sops.allocate_resources))) {
        flux_log (ctx->h, LOG_ERR, "can't load allocate_resources: %s",
                  dlerror ());
        goto done;
    }
    ctx->sops.reserve_resources = dlsym (ctx->sops.dso, "reserve_resources");
    if (!(ctx->sops.reserve_resources) || !(*(ctx->sops.reserve_resources))) {
        flux_log (ctx->h, LOG_ERR, "can't load reserve_resources: %s",
                  dlerror ());
        goto done;
    }
    rc = 0;

done:
    return rc;
}

static int load_sched_plugin (ssrvctx_t *ctx)
{
    int rc = -1;
    flux_t h = ctx->h;
    char *path = NULL;
    char *searchpath = getenv ("FLUX_MODULE_PATH");

    if (!searchpath) {
        flux_log (h, LOG_ERR, "FLUX_MODULE_PATH not set");
        goto done;
    }
    if (!(path = flux_modfind (searchpath, ctx->arg.userplugin))) {
        flux_log (h, LOG_ERR, "%s: not found in module search path %s",
                  ctx->arg.userplugin, searchpath);
        goto done;
    }
    if (!(ctx->sops.dso = dlopen (path, RTLD_NOW | RTLD_LOCAL))) {
        flux_log (h, LOG_ERR, "failed to open sched plugin: %s",
                  dlerror ());
        goto done;
    }
    flux_log (h, LOG_DEBUG, "loaded: %s", ctx->arg.userplugin);
    rc = resolve_functions (ctx);

done:
    return rc;
}


/******************************************************************************
 *                                                                            *
 *                   Setting Up RDL (RFC 4)                                   *
 *                                                                            *
 ******************************************************************************/

static void setup_rdl_lua (flux_t h)
{
    flux_log (h, LOG_DEBUG, "LUA_PATH %s", getenv ("LUA_PATH"));
    flux_log (h, LOG_DEBUG, "LUA_CPATH %s", getenv ("LUA_CPATH"));
}

static int build_hwloc_rs2rank (ssrvctx_t *ctx, rsreader_t r_mode)
{
    int rc = -1;
    size_t len = 0;
    uint32_t rank = 0, size = 0;
    char *key = NULL, *rs_buf = NULL;

    if (flux_get_size (ctx->h, &size) == -1) {
        flux_log (ctx->h, LOG_ERR, "can't decide the instance size");
        goto done;
    }
    for (rank=0; rank < size; rank++) {
        key = xasprintf ("resource.hwloc.xml.%"PRIu32"", rank);
        if (kvs_get_string (ctx->h, key, &rs_buf) == -1) {
            flux_log (ctx->h, LOG_ERR, "can't get hwloc data in kvs (%s)", key);
            break;
        }
        len = strlen (rs_buf);
        if (rsreader_hwloc_load (rs_buf, len, rank, r_mode,
             &(ctx->rctx.root_resrc), ctx->machs) != 0) {
            flux_log (ctx->h, LOG_ERR, "can't load hwloc data");
            goto done;
        } else if (key) {
            free (key);
            key = NULL;
        }
    }
    rc = 0;

done:
    if (key)
        free (key);
    if (rs_buf)
        free (rs_buf);
    return rc;
}

static void dump_resrc_state (flux_t h, resrc_tree_t *rt)
{
    char *str;
    if (!rt)
        return;
    str = resrc_to_string (resrc_tree_resrc (rt));
    flux_log (h, LOG_INFO, "%s", str);
    free (str);
    if (resrc_tree_num_children (rt)) {
        resrc_tree_t *child = resrc_tree_list_first (resrc_tree_children (rt));
        while (child) {
            dump_resrc_state (h, child);
            child = resrc_tree_list_next (resrc_tree_children (rt));
        }
    }
    return;
}

static int load_resources (ssrvctx_t *ctx)
{
    int rc = -1;
    char *turi = NULL;
    resrc_t *tres = NULL;
    char *path = ctx->arg.path;
    char *uri = ctx->arg.uri;
    rsreader_t r_mode = ctx->arg.r_mode;

    setup_rdl_lua (ctx->h);

    switch (r_mode) {
    case RSREADER_RESRC_EMUL:
        if (rsreader_resrc_bulkload (path, uri, &turi, &tres) != 0) {
            flux_log (ctx->h, LOG_ERR, "failed to load resrc");
            goto done;
        } else if (build_hwloc_rs2rank (ctx, r_mode) != 0) {
            flux_log (ctx->h, LOG_ERR, "failed to build rs2rank");
            goto done;
        } else if (rsreader_force_link2rank (ctx->machs, tres) != 0) {
            flux_log (ctx->h, LOG_ERR, "failed to force a link to a rank");
            goto done;
        }
        ctx->rctx.root_uri = turi;
        ctx->rctx.root_resrc = tres;
        flux_log (ctx->h, LOG_INFO, "loaded resrc");
        rc = 0;
        break;

    case RSREADER_RESRC:
        if (rsreader_resrc_bulkload (path, uri, &turi, &tres) != 0) {
            flux_log (ctx->h, LOG_ERR, "failed to load resrc");
            goto done;
        } else if (build_hwloc_rs2rank (ctx, r_mode) != 0) {
            flux_log (ctx->h, LOG_ERR, "failed to build rs2rank");
            goto done;
        }
        if (ctx->arg.verbosity > 0) {
            flux_log (ctx->h, LOG_INFO, "resrc state after resrc read");
            dump_resrc_state (ctx->h, resrc_phys_tree (tres));
        }
        if (rsreader_link2rank (ctx->machs, tres) != 0) {
            flux_log (ctx->h, LOG_ERR, "RDL(%s) inconsistent w/ hwloc!", path);
            flux_log (ctx->h, LOG_INFO, "rebuild resrc using hwloc");
            if (turi)
                free (turi);
            if (tres)
                resrc_tree_destroy (resrc_phys_tree (tres), true);
            r_mode = RSREADER_HWLOC;
            /* deliberate fall-through to RSREADER_HWLOC! */
        } else {
            ctx->rctx.root_uri = turi;
            ctx->rctx.root_resrc = tres;
            flux_log (ctx->h, LOG_INFO, "loaded resrc");
            rc = 0;
            break;
        }

    case RSREADER_HWLOC:
        if (!(ctx->rctx.root_resrc = resrc_create_cluster ("cluster"))) {
            flux_log (ctx->h, LOG_ERR, "failed to create cluster resrc");
            goto done;
        } else if (build_hwloc_rs2rank (ctx, r_mode) != 0) {
            flux_log (ctx->h, LOG_ERR, "failed to load resrc using hwloc");
            goto done;
        }
        /* linking has already been done by build_hwloc_rs2rank above */
        if (ctx->arg.verbosity > 0) {
            flux_log (ctx->h, LOG_INFO, "resrc state after hwloc read");
            dump_resrc_state (ctx->h, resrc_phys_tree (ctx->rctx.root_resrc));
        }
        rc = 0;
        break;

    default:
        flux_log (ctx->h, LOG_ERR, "unkwown resource reader type");
        break;
    }

done:
    return rc;
}


/******************************************************************************
 *                                                                            *
 *                         Emulator Specific Code                             *
 *                                                                            *
 ******************************************************************************/

/*
 * Simulator Helper Functions
 */
static void queue_timer_change (ssrvctx_t *ctx, const char *module)
{
    zlist_append (ctx->sctx.timer_queue, (void *)module);
}

// Set the timer for "module" to happen relatively soon
// If the mod is sim_exec, it shouldn't happen immediately
// because the scheduler still needs to transition through
// 3->4 states before the sim_exec module can actually "exec" a job
static void set_next_event (const char *module, sim_state_t *sim_state)
{
    double next_event;
    double *timer = zhash_lookup (sim_state->timers, module);
    next_event =
        sim_state->sim_time + ((!strcmp (module, "sim_exec")) ? .0001 : .00001);
    if (*timer > next_event || *timer < 0) {
        *timer = next_event;
    }
}

static void handle_timer_queue (ssrvctx_t *ctx, sim_state_t *sim_state)
{
    while (zlist_size (ctx->sctx.timer_queue) > 0)
        set_next_event (zlist_pop (ctx->sctx.timer_queue), sim_state);

#if ENABLE_TIMER_EVENT
    // Set scheduler loop to run in next occuring scheduler block
    double *this_timer = zhash_lookup (sim_state->timers, "sched");
    double next_schedule_block =
        sim_state->sim_time
        + (SCHED_INTERVAL - ((int)sim_state->sim_time % SCHED_INTERVAL));
    if (ctx->run_schedule_loop &&
        ((next_schedule_block < *this_timer || *this_timer < 0))) {
        *this_timer = next_schedule_block;
    }
    flux_log (ctx->h,
              LOG_DEBUG,
              "run_sched_loop: %d, next_schedule_block: %f, this_timer: %f",
              ctx->run_schedule_loop,
              next_schedule_block,
              *this_timer);
#endif
}

static void handle_jsc_queue (ssrvctx_t *ctx)
{
    jsc_event_t *jsc_event = NULL;

    while (zlist_size (ctx->sctx.jsc_queue) > 0) {
        jsc_event = (jsc_event_t *)zlist_pop (ctx->sctx.jsc_queue);
        flux_log (ctx->h,
                  LOG_DEBUG,
                  "JscEvent being handled - JSON: %s, errnum: %d",
                  Jtostr (jsc_event->jcb),
                  jsc_event->errnum);
        job_status_cb (Jtostr (jsc_event->jcb), jsc_event->arg,
                       jsc_event->errnum);
        Jput (jsc_event->jcb);
        free (jsc_event);
    }
}

static void handle_res_queue (ssrvctx_t *ctx)
{
    res_event_t *res_event = NULL;

    while (zlist_size (ctx->sctx.res_queue) > 0) {
        res_event = (res_event_t *)zlist_pop (ctx->sctx.res_queue);
        flux_log (ctx->h,
                  LOG_DEBUG,
                  "ResEvent being handled");
        res_event_cb (res_event->h, NULL, NULL, res_event->arg);
        free (res_event);
    }
}

/*
 * Simulator Callbacks
 */
static void start_cb (flux_t h,
                      flux_msg_handler_t *w,
                      const flux_msg_t *msg,
                      void *arg)
{
    flux_log (h, LOG_DEBUG, "received a start event");
    if (send_join_request (h, "sched", -1) < 0) {
        flux_log (h,
                  LOG_ERR,
                  "submit module failed to register with sim module");
        return;
    }
    flux_log (h, LOG_DEBUG, "sent a join request");

    if (flux_event_unsubscribe (h, "sim.start") < 0) {
        flux_log (h, LOG_ERR, "failed to unsubscribe from \"sim.start\"");
        return;
    } else {
        flux_log (h, LOG_DEBUG, "unsubscribed from \"sim.start\"");
    }

    return;
}

static int sim_job_status_cb (const char *jcbstr, void *arg, int errnum)
{
    JSON jcb = NULL;
    ssrvctx_t *ctx = getctx ((flux_t)arg);
    jsc_event_t *event = (jsc_event_t*) xzmalloc (sizeof (jsc_event_t));

    if (!(jcb = Jfromstr (jcbstr))) {
        flux_log (ctx->h, LOG_ERR, "sim_job_status_cb: error parsing JSON string");
        return -1;
    }

    event->jcb = Jget (jcb);
    event->arg = arg;
    event->errnum = errnum;

    flux_log (ctx->h,
              LOG_DEBUG,
              "JscEvent being queued - JSON: %s, errnum: %d",
              Jtostr (event->jcb),
              event->errnum);
    zlist_append (ctx->sctx.jsc_queue, event);

    return 0;
}

static void sim_res_event_cb (flux_t h, flux_msg_handler_t *w,
                              const flux_msg_t *msg, void *arg) {
    ssrvctx_t *ctx = getctx ((flux_t)arg);
    res_event_t *event = (res_event_t*) xzmalloc (sizeof (res_event_t));
    const char *topic = NULL;

    event->h = h;
    event->arg = arg;

    flux_msg_get_topic (msg, &topic);
    flux_log (ctx->h,
              LOG_DEBUG,
              "ResEvent being queued - topic: %s",
              topic);
    zlist_append (ctx->sctx.res_queue, event);
}

static void trigger_cb (flux_t h,
                        flux_msg_handler_t *w,
                        const flux_msg_t *msg,
                        void *arg)
{
    clock_t start, diff;
    double seconds;
    bool sched_loop;
    const char *json_str = NULL;
    JSON o = NULL;
    ssrvctx_t *ctx = getctx (h);

    if (flux_request_decode (msg, NULL, &json_str) < 0 || json_str == NULL
        || !(o = Jfromstr (json_str))) {
        flux_log (h, LOG_ERR, "%s: bad message", __FUNCTION__);
        Jput (o);
        return;
    }

    flux_log (h, LOG_DEBUG, "Setting sim_state to new values");
    ctx->sctx.sim_state = json_to_sim_state (o);

    start = clock ();

    handle_jsc_queue (ctx);
    handle_res_queue (ctx);

    sched_loop = true;
    diff = clock () - start;
    seconds = ((double)diff) / CLOCKS_PER_SEC;
    ctx->sctx.sim_state->sim_time += seconds;
    if (sched_loop) {
        flux_log (h,
                  LOG_DEBUG,
                  "scheduler timer: events + loop took %f seconds",
                  seconds);
    } else {
        flux_log (h,
                  LOG_DEBUG,
                  "scheduler timer: events took %f seconds",
                  seconds);
    }

    handle_timer_queue (ctx, ctx->sctx.sim_state);

    send_reply_request (h, "sched", ctx->sctx.sim_state);

    free_simstate (ctx->sctx.sim_state);
    Jput (o);
}


/******************************************************************************
 *                                                                            *
 *                     Scheduler Eventing For Emulation Mode                  *
 *                                                                            *
 ******************************************************************************/

/*
 * Simulator Initialization Functions
 */
static struct flux_msg_handler_spec sim_htab[] = {
    {FLUX_MSGTYPE_EVENT, "sim.start", start_cb},
    {FLUX_MSGTYPE_REQUEST, "sched.trigger", trigger_cb},
    {FLUX_MSGTYPE_EVENT, "sched.res.*", sim_res_event_cb},
    FLUX_MSGHANDLER_TABLE_END,
};

static int reg_sim_events (ssrvctx_t *ctx)
{
    int rc = -1;
    flux_t h = ctx->h;

    if (flux_event_subscribe (ctx->h, "sim.start") < 0) {
        flux_log (ctx->h, LOG_ERR, "subscribing to event: %s", strerror (errno));
        goto done;
    }
    if (flux_event_subscribe (ctx->h, "sched.res.") < 0) {
        flux_log (ctx->h, LOG_ERR, "subscribing to event: %s", strerror (errno));
        goto done;
    }
    if (flux_msg_handler_addvec (ctx->h, sim_htab, (void *)h) < 0) {
        flux_log (ctx->h, LOG_ERR, "flux_msg_handler_addvec: %s", strerror (errno));
        goto done;
    }
    if (jsc_notify_status (h, sim_job_status_cb, (void *)h) != 0) {
        flux_log (h, LOG_ERR, "error registering a job status change CB");
        goto done;
    }

    send_alive_request (ctx->h, "sched");

    rc = 0;
done:
    return rc;
}

static int setup_sim (ssrvctx_t *ctx, bool sim)
{
    int rc = 0;

    if (sim) {
        flux_log (ctx->h, LOG_INFO, "setting up sim in scheduler");
        ctx->sctx.in_sim = true;
        ctx->sctx.sim_state = NULL;
        ctx->sctx.res_queue = zlist_new ();
        ctx->sctx.jsc_queue = zlist_new ();
        ctx->sctx.timer_queue = zlist_new ();
    }
    else
        rc = -1;

    return rc;
}


/******************************************************************************
 *                                                                            *
 *                     Scheduler Eventing For Normal Mode                     *
 *                                                                            *
 ******************************************************************************/

static struct flux_msg_handler_spec htab[] = {
    { FLUX_MSGTYPE_EVENT,     "sched.res.*", res_event_cb},
    FLUX_MSGHANDLER_TABLE_END
};

/*
 * Register events, some of which CAN triger a scheduling loop iteration.
 * Currently,
 *    -  Resource event: invoke the schedule loop;
 *    -  Timer event: invoke the schedule loop;
 *    -  Job event (JSC notification): triggers actions based on FSM
 *          and some state changes trigger the schedule loop.
 */
static int inline reg_events (ssrvctx_t *ctx)
{
    int rc = 0;
    flux_t h = ctx->h;

    if (flux_event_subscribe (h, "sched.res.") < 0) {
        flux_log (h, LOG_ERR, "subscribing to event: %s", strerror (errno));
        rc = -1;
        goto done;
    }
    if (flux_msg_handler_addvec (h, htab, (void *)h) < 0) {
        flux_log (h, LOG_ERR,
                  "error registering resource event handler: %s",
                  strerror (errno));
        rc = -1;
        goto done;
    }
    /* TODO: we need a way to manage environment variables or
       configrations
    */
#if ENABLE_TIMER_EVENT
    if (flux_tmouthandler_add (h, 30000, false, timer_event_cb, (void *)h) < 0) {
        flux_log (h, LOG_ERR,
                  "error registering timer event CB: %s",
                  strerror (errno));
        rc = -1;
        goto done;
    }
#endif
    if (jsc_notify_status (h, job_status_cb, (void *)h) != 0) {
        flux_log (h, LOG_ERR, "error registering a job status change CB");
        rc = -1;
        goto done;
    }

done:
    return rc;
}


/******************************************************************************
 *                                                                            *
 *            Mode Bridging Layer to Hide Emulation vs. Normal Mode           *
 *                                                                            *
 ******************************************************************************/

static inline int bridge_set_execmode (ssrvctx_t *ctx)
{
    int rc = 0;
    if (ctx->arg.sim && setup_sim (ctx, ctx->arg.sim) != 0) {
        flux_log (ctx->h, LOG_ERR, "failed to setup sim mode");
        rc = -1;
        goto done;
    }
done:
    return rc;
}

static inline int bridge_set_events (ssrvctx_t *ctx)
{
    int rc = -1;
    if (ctx->sctx.in_sim) {
        if (reg_sim_events (ctx) != 0) {
            flux_log (ctx->h, LOG_ERR, "failed to reg sim events");
            goto done;
        }
        flux_log (ctx->h, LOG_INFO, "sim events registered");
    } else {
        if (reg_events (ctx) != 0) {
            flux_log (ctx->h, LOG_ERR, "failed to reg events");
            goto done;
        }
        flux_log (ctx->h, LOG_INFO, "events registered");
    }
    rc = 0;

done:
    return rc;
}

static inline int bridge_send_runrequest (ssrvctx_t *ctx, flux_lwj_t *job)
{
    int rc = -1;
    flux_t h = ctx->h;
    char *topic = NULL;
    flux_msg_t *msg = NULL;

    if (ctx->sctx.in_sim) {
        /* Emulation mode */
        if (asprintf (&topic, "sim_exec.run.%"PRId64"", job->lwj_id) < 0) {
            flux_log (h, LOG_ERR, "%s: topic create failed: %s",
                      __FUNCTION__, strerror (errno));
        } else if (!(msg = flux_msg_create (FLUX_MSGTYPE_REQUEST))
                   || flux_msg_set_topic (msg, topic) < 0
                   || flux_send (h, msg, 0) < 0) {
            flux_log (h, LOG_ERR, "%s: request create failed: %s",
                      __FUNCTION__, strerror (errno));
        } else {
            queue_timer_change (ctx, "sim_exec");
            flux_log (h, LOG_DEBUG, "job %"PRId64" runrequest", job->lwj_id);
            rc = 0;
        }
    } else {
        /* Normal mode */
        if (asprintf (&topic, "wrexec.run.%"PRId64"", job->lwj_id) < 0) {
            flux_log (h, LOG_ERR, "%s: topic create failed: %s",
                      __FUNCTION__, strerror (errno));
        } else if (!(msg = flux_event_encode (topic, NULL))
                   || flux_send (h, msg, 0) < 0) {
            flux_log (h, LOG_ERR, "%s: event create failed: %s",
                      __FUNCTION__, strerror (errno));
        } else {
            flux_log (h, LOG_DEBUG, "job %"PRId64" runrequest", job->lwj_id);
            rc = 0;
        }
    }
    if (msg)
        flux_msg_destroy (msg);
    if (topic)
        free (topic);
    return rc;
}

static inline void bridge_update_timer (ssrvctx_t *ctx)
{
    if (ctx->sctx.in_sim)
        queue_timer_change (ctx, "sched");
}

static inline int bridge_rs2rank_tab_query (ssrvctx_t *ctx, resrc_t *r,
                                            uint32_t *rank)
{
    int rc = -1;
    if (ctx->sctx.in_sim) {
        rc = rs2rank_tab_query_by_none (ctx->machs, resrc_digest (r),
                                        false, rank);
    } else {
        flux_log (ctx->h, LOG_INFO, "hostname: %s, digest: %s\n", resrc_name (r),
                                     resrc_digest (r));
        rc = rs2rank_tab_query_by_sign (ctx->machs, resrc_name (r), resrc_digest (r),
                                        false, rank);
    }
    if (rc == 0)
        flux_log (ctx->h, LOG_INFO, "broker found, rank: %"PRIu32, *rank);
    else
        flux_log (ctx->h, LOG_ERR, "controlling broker not found!");

    return rc;
}

/********************************************************************************
 *                                                                              *
 *            Task Program Execution Service Request (RFC 8)                    *
 *                                                                              *
 *******************************************************************************/

static void inline build_contain_1node_req (int64_t nc, int64_t rank, JSON rarr)
{
    JSON e = Jnew ();
    JSON o = Jnew ();
    Jadd_int64 (o, JSC_RDL_ALLOC_CONTAINING_RANK, rank);
    Jadd_int64 (o, JSC_RDL_ALLOC_CONTAINED_NCORES, nc);
    json_object_object_add (e, JSC_RDL_ALLOC_CONTAINED, o);
    json_object_array_add (rarr, e);
}

/*
 * Because the job's rdl should only contain what's allocated to the job,
 * we traverse the entire tree in the post-order walk fashion
 */
static int build_contain_req (ssrvctx_t *ctx, flux_lwj_t *job, JSON arr)
{
    int rc = -1;
    uint32_t rank = 0;
    resrc_tree_t *nd = NULL;
    resrc_t *r = NULL;

    for (nd = resrc_tree_list_first (job->resrc_trees); nd;
            nd = resrc_tree_list_next (job->resrc_trees)) {
        r = resrc_tree_resrc (nd);
        if (strcmp (resrc_type (r), "node") != 0
            || bridge_rs2rank_tab_query (ctx, r, &rank) != 0)
            goto done;

        build_contain_1node_req (job->req->corespernode, rank, arr);
    }
    rc = 0;
done:
    return rc;
}

/*
 * Once the job gets allocated to its own copy of rdl, this
 *    1) serializes the rdl and sends it to TP exec service
 *    2) builds JSC_RDL_ALLOC JCB and sends it to TP exec service
 *    3) sends JCB state update with J_ALLOCATE
 */
static int req_tpexec_allocate (ssrvctx_t *ctx, flux_lwj_t *job)
{
    const char *jcbstr = NULL;
    int rc = -1;
    flux_t h = ctx->h;
    JSON jcb = Jnew ();
    JSON ro = Jnew_ar ();
    JSON arr = Jnew_ar ();

    if (resrc_tree_list_serialize (ro, job->resrc_trees)) {
        flux_log (h, LOG_ERR, "%"PRId64" resource serialization failed: %s",
                  job->lwj_id, strerror (errno));
        goto done;
    }
    json_object_object_add (jcb, JSC_RDL, ro);
    jcbstr = Jtostr (jcb);
    if (jsc_update_jcb (h, job->lwj_id, JSC_RDL, jcbstr) != 0) {
        flux_log (h, LOG_ERR, "error jsc udpate: %"PRId64" (%s)", job->lwj_id,
                  strerror (errno));
        goto done;
    }
    Jput (jcb);
    jcb = Jnew ();
    if (build_contain_req (ctx, job, arr) != 0) {
        flux_log (h, LOG_ERR, "error requesting containment for job");
        goto done;
    }
    json_object_object_add (jcb, JSC_RDL_ALLOC, arr);
    jcbstr = Jtostr (jcb);
    if (jsc_update_jcb (h, job->lwj_id, JSC_RDL_ALLOC, jcbstr) != 0) {
        flux_log (h, LOG_ERR, "error updating jcb");
        goto done;
    }
    Jput (arr);
    if ((update_state (h, job->lwj_id, job->state, J_ALLOCATED)) != 0) {
        flux_log (h, LOG_ERR, "failed to update the state of job %"PRId64"",
                  job->lwj_id);
        goto done;
    }
    bridge_update_timer (ctx);
    rc = 0;
done:
    if (jcb)
        Jput (jcb);
    return rc;
}

#if DYNAMIC_SCHEDULING
static int req_tpexec_grow (flux_t h, flux_lwj_t *job)
{
    /* TODO: NOT IMPLEMENTED */
    /* This runtime grow service will grow the resource set of the job.
       The special non-elastic case will be to grow the resource from
       zero to the selected RDL
    */
    return SCHED_UNIMPL;
}

static int req_tpexec_shrink (flux_t h, flux_lwj_t *job)
{
    /* TODO: NOT IMPLEMENTED */
    return SCHED_UNIMPL;
}

static int req_tpexec_map (flux_t h, flux_lwj_t *job)
{
    /* TODO: NOT IMPLEMENTED */
    /* This runtime grow service will grow the resource set of the job.
       The special non-elastic case will be to grow the resource from
       zero to RDL
    */
    return SCHED_UNIMPL;
}
#endif

static int req_tpexec_exec (flux_t h, flux_lwj_t *job)
{
    ssrvctx_t *ctx = getctx (h);
    int rc = -1;

    if ((update_state (h, job->lwj_id, job->state, J_RUNREQUEST)) != 0) {
        flux_log (h, LOG_ERR, "failed to update the state of job %"PRId64"",
                  job->lwj_id);
        goto done;
    } else if (bridge_send_runrequest (ctx, job) != 0) {
        flux_log (h, LOG_ERR, "failed to send runrequest for job %"PRId64"",
                  job->lwj_id);
        goto done;
    }
    rc = 0;
done:
    return rc;
}

static int req_tpexec_run (flux_t h, flux_lwj_t *job)
{
    /* TODO: wreckrun does not provide grow and map yet
     *   we will switch to the following sequence under the TP exec service
     *   that provides RFC 8.
     *
     *   req_tpexec_grow
     *   req_tpexec_map
     *   req_tpexec_exec
     */
    return req_tpexec_exec (h, job);
}


/********************************************************************************
 *                                                                              *
 *           Actions on Job/Res/Timer event including Scheduling Loop           *
 *                                                                              *
 *******************************************************************************/

/*
 * schedule_job() searches through all of the idle resources to
 * satisfy a job's requirements.  If enough resources are found, it
 * proceeds to allocate those resources and update the kvs's lwj entry
 * in preparation for job execution.  If less resources
 * are found than the job requires, and if the job asks to reserve
 * resources, then those resources will be reserved.
 */
int schedule_job (ssrvctx_t *ctx, flux_lwj_t *job, int64_t starttime)
{
    JSON child_core = NULL;
    JSON req_res = NULL;
    flux_t h = ctx->h;
    int rc = -1;
    int64_t nnodes = 0;
    resrc_reqst_t *resrc_reqst = NULL;
    resrc_tree_list_t *found_trees = NULL;
    resrc_tree_list_t *selected_trees = NULL;

    /*
     * Require at least one task per node, and
     * Assume (for now) one task per core.
     */
    job->req->nnodes = (job->req->nnodes ? job->req->nnodes : 1);
    if (job->req->ncores < job->req->nnodes)
        job->req->ncores = job->req->nnodes;
    job->req->corespernode = (job->req->ncores + job->req->nnodes - 1) /
        job->req->nnodes;

    child_core = Jnew ();
    Jadd_str (child_core, "type", "core");
    Jadd_int64 (child_core, "req_qty", job->req->corespernode);
    /* setting size == 1 devotes (all of) the core to the job */
    Jadd_int64 (child_core, "req_size", 1);
    /* setting exclusive to true prevents multiple jobs per core */
    Jadd_bool (child_core, "exclusive", true);
    Jadd_int64 (child_core, "starttime", starttime);
    Jadd_int64 (child_core, "endtime", starttime + job->req->walltime);

    req_res = Jnew ();
    Jadd_str (req_res, "type", "node");
    Jadd_int64 (req_res, "req_qty", job->req->nnodes);
    if (job->req->node_exclusive) {
        Jadd_int64 (req_res, "req_size", 1);
        Jadd_bool (req_res, "exclusive", true);
    } else {
        Jadd_int64 (req_res, "req_size", 0);
        Jadd_bool (req_res, "exclusive", false);
    }
    Jadd_int64 (req_res, "starttime", starttime);
    Jadd_int64 (req_res, "endtime", starttime + job->req->walltime);

    json_object_object_add (req_res, "req_child", child_core);

    resrc_reqst = resrc_reqst_from_json (req_res, NULL);
    Jput (req_res);
    if (!resrc_reqst)
        goto done;

    if ((found_trees = ctx->sops.find_resources (h, ctx->rctx.root_resrc,
                                                 resrc_reqst))) {
        nnodes = resrc_tree_list_size (found_trees);
        flux_log (h, LOG_DEBUG, "%"PRId64" nodes found for job %"PRId64", "
                  "reqrd_qty: %"PRId64"", nnodes, job->lwj_id, job->req->nnodes);

        resrc_tree_list_unstage_resources (found_trees);
        if ((selected_trees = ctx->sops.select_resources (h, found_trees,
                                                          resrc_reqst))) {
            nnodes = resrc_tree_list_size (selected_trees);
            if (nnodes == job->req->nnodes) {
                ctx->sops.allocate_resources (h, selected_trees, job->lwj_id,
                                              starttime, starttime +
                                              job->req->walltime);
                /* Scheduler specific job transition */
                // TODO: handle this some other way (JSC?)
                job->starttime = starttime;
                job->state = J_SELECTED;
                job->resrc_trees = selected_trees;
                if (req_tpexec_allocate (ctx, job) != 0) {
                    flux_log (h, LOG_ERR,
                              "failed to request allocate for job %"PRId64"",
                              job->lwj_id);
                    goto done;
                }
                flux_log (h, LOG_DEBUG, "Allocated %"PRId64" nodes for job "
                          "%"PRId64", reqrd_qty: %"PRId64"", nnodes, job->lwj_id,
                          job->req->nnodes);
            } else {
                ctx->sops.reserve_resources (h, selected_trees, job->lwj_id,
                                             starttime, job->req->walltime,
                                             ctx->rctx.root_resrc, resrc_reqst);
            }
        }
    }
    rc = 0;
done:
    if (resrc_reqst)
        resrc_reqst_destroy (resrc_reqst);
    if (found_trees)
        resrc_tree_list_destroy (found_trees, false);

    return rc;
}

static int schedule_jobs (ssrvctx_t *ctx)
{
    int rc = 0;
    flux_lwj_t *job = NULL;
    /* Prioritizing the job queue is left to an external agent.  In
     * this way, the scheduler's activities are pared down to just
     * scheduling activies.
     * TODO: when dynamic scheduling is supported, the loop should
     * traverse through running job queue as well.
     */
    zlist_t *jobs = ctx->p_queue;
    int64_t starttime = (ctx->sctx.in_sim) ?
        (int64_t) ctx->sctx.sim_state->sim_time : epochtime();

    resrc_tree_release_all_reservations (resrc_phys_tree (ctx->rctx.root_resrc));
    rc = ctx->sops.sched_loop_setup (ctx->h);
    job = zlist_first (jobs);
    while (!rc && job) {
        if (job->state == J_SCHEDREQ) {
            rc = schedule_job (ctx, job, starttime);
        }
        job = (flux_lwj_t*)zlist_next (jobs);
    }

    return rc;
}


/********************************************************************************
 *                                                                              *
 *                        Scheduler Event Handling                              *
 *                                                                              *
 ********************************************************************************/

#define VERIFY(rc) if (!(rc)) {goto bad_transition;}
static inline bool trans (job_state_t ex, job_state_t n, job_state_t *o)
{
    if (ex == n) {
        *o = n;
        return true;
    }
    return false;
}

/*
 * Following is a state machine. action is invoked when an external job
 * state event is delivered. But in action, certain events are also generated,
 * some events are realized by falling through some of the case statements.
 */
static int action (ssrvctx_t *ctx, flux_lwj_t *job, job_state_t newstate)
{
    flux_t h = ctx->h;
    job_state_t oldstate = job->state;

    flux_log (h, LOG_DEBUG, "attempting job %"PRId64" state change from "
              "%s to %s", job->lwj_id, jsc_job_num2state (oldstate),
              jsc_job_num2state (newstate));

    switch (oldstate) {
    case J_NULL:
        VERIFY (trans (J_NULL, newstate, &(job->state))
                || trans (J_RESERVED, newstate, &(job->state)));
        break;
    case J_RESERVED:
        VERIFY (trans (J_SUBMITTED, newstate, &(job->state)));
        fill_resource_req (h, job);
        /* fall through for implicit event generation */
    case J_SUBMITTED:
        VERIFY (trans (J_PENDING, J_PENDING, &(job->state)));
        /* fall through for implicit event generation */
    case J_PENDING:
        VERIFY (trans (J_SCHEDREQ, J_SCHEDREQ, &(job->state)));
        schedule_jobs (ctx); /* includes request allocate if successful */
        break;
    case J_SCHEDREQ:
        /* A schedule reqeusted should not get an event. */
        /* SCHEDREQ -> SELECTED happens implicitly within schedule jobs */
        VERIFY (false);
        break;
    case J_SELECTED:
        VERIFY (trans (J_ALLOCATED, newstate, &(job->state)));
        req_tpexec_run (h, job);
        break;
    case J_ALLOCATED:
        VERIFY (trans (J_RUNREQUEST, newstate, &(job->state)));
        break;
    case J_RUNREQUEST:
        VERIFY (trans (J_STARTING, newstate, &(job->state)));
        break;
    case J_STARTING:
        VERIFY (trans (J_RUNNING, newstate, &(job->state)));
        q_move_to_rqueue (ctx, job);
        break;
    case J_RUNNING:
        VERIFY (trans (J_COMPLETE, newstate, &(job->state))
                || trans (J_CANCELLED, newstate, &(job->state)));
        q_move_to_cqueue (ctx, job);
        if (!ctx->arg.schedonce) {
            /* support testing by actually not releasing the resrc */
            if (resrc_tree_list_release (job->resrc_trees, job->lwj_id)) {
            flux_log (h, LOG_ERR, "%s: failed to release resources for job "
                      "%"PRId64"", __FUNCTION__, job->lwj_id);
            }
        }
        flux_msg_t *msg = flux_event_encode ("sched.res.freed", NULL);
        if (!msg || flux_send (h, msg, 0) < 0) {
            flux_log (h, LOG_ERR, "%s: error sending event: %s",
                      __FUNCTION__, strerror (errno));
        } else {
            flux_msg_destroy (msg);
            flux_log (h, LOG_DEBUG, "Released resources for job %"PRId64"",
                      job->lwj_id);
        }
        break;
    case J_CANCELLED:
        VERIFY (trans (J_REAPED, newstate, &(job->state)));
        zlist_remove (ctx->c_queue, job);
        if (job->req)
            free (job->req);
        free (job);
        break;
    case J_COMPLETE:
        VERIFY (trans (J_REAPED, newstate, &(job->state)));
        zlist_remove (ctx->c_queue, job);
        if (job->req)
            free (job->req);
        free (job);
        break;
    case J_REAPED:
    default:
        VERIFY (false);
        break;
    }
    return 0;

bad_transition:
    flux_log (h, LOG_ERR, "job %"PRId64" bad state transition from %s to %s",
              job->lwj_id, jsc_job_num2state (oldstate),
              jsc_job_num2state (newstate));
    return -1;
}

/* TODO: we probably need to abstract out resource status & control  API
 * For now, the only resource event is raised when a job releases its
 * RDL allocation.
 */
static void res_event_cb (flux_t h, flux_msg_handler_t *w,
                          const flux_msg_t *msg, void *arg)
{
    schedule_jobs (getctx ((flux_t)arg));
    return;
}

#if ENABLE_TIMER_EVENT
static int timer_event_cb (flux_t h, void *arg)
{
    //flux_log (h, LOG_ERR, "TIMER CALLED");
    schedule_jobs (getctx ((flux_t)arg));
    return 0;
}
#endif

static int job_status_cb (const char *jcbstr, void *arg, int errnum)
{
    JSON jcb = NULL;
    ssrvctx_t *ctx = getctx ((flux_t)arg);
    flux_lwj_t *j = NULL;
    job_state_t ns = J_FOR_RENT;

    if (errnum > 0) {
        flux_log (ctx->h, LOG_ERR, "job_status_cb: errnum passed in");
        return -1;
    }
    if (!(jcb = Jfromstr (jcbstr))) {
        flux_log (ctx->h, LOG_ERR, "job_status_cb: error parsing JSON string");
        return -1;
    }
    if (is_newjob (jcb))
        append_to_pqueue (ctx, jcb);
    if ((j = fetch_job_and_event (ctx, jcb, &ns)) == NULL) {
        flux_log (ctx->h, LOG_ERR, "error fetching job and event");
        return -1;
    }
    Jput (jcb);
    return action (ctx, j, ns);
}

/******************************************************************************
 *                                                                            *
 *                     Scheduler Service Module Main                          *
 *                                                                            *
 ******************************************************************************/


int mod_main (flux_t h, int argc, char **argv)
{
    int rc = -1;
    ssrvctx_t *ctx = NULL;
    uint32_t rank = 1;

    if (!(ctx = getctx (h))) {
        flux_log (h, LOG_ERR, "can't find or allocate the context");
        goto done;
    }
    if (flux_get_rank (h, &rank)) {
        flux_log (h, LOG_ERR, "failed to determine rank");
        goto done;
    } else if (rank) {
        flux_log (h, LOG_ERR, "sched module must only run on rank 0");
        goto done;
    } else if (ssrvarg_process_args (argc, argv, &(ctx->arg)) != 0) {
        flux_log (h, LOG_ERR, "can't process module args");
        goto done;
    }
    flux_log (h, LOG_INFO, "sched comms module starting");

    if (load_sched_plugin (ctx) != 0) {
        flux_log (h, LOG_ERR, "failed to load scheduler plugin");
        goto done;
    }
    flux_log (h, LOG_INFO, "%s plugin loaded", ctx->arg.userplugin);
    if (bridge_set_execmode (ctx) != 0) {
        flux_log (h, LOG_ERR, "failed to setup execution mode");
        goto done;
    }
    if (load_resources (ctx) != 0) {
        flux_log (h, LOG_ERR, "failed to load resources");
        goto done;
    }
    flux_log (h, LOG_INFO, "resources loaded");
    if (bridge_set_events (ctx) != 0) {
        flux_log (h, LOG_ERR, "failed to set events");
        goto done;
    }
    if (flux_reactor_run (flux_get_reactor (h), 0) < 0) {
        flux_log (h, LOG_ERR, "flux_reactor_run: %s", strerror (errno));
        goto done;
    }
    rc = 0;

done:
    return rc;
}

MOD_NAME ("sched");

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

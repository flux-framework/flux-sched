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
 * sched.c - scheduler framework service comms module
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <argz.h>
#include <libgen.h>
#include <errno.h>
#include <libgen.h>
#include <czmq.h>
#include <dlfcn.h>
#include <stdbool.h>
#include <flux/core.h>

#include "src/common/libutil/log.h"
#include "src/common/libutil/shortjansson.h"
#include "src/common/libutil/xzmalloc.h"
#include "resrc.h"
#include "resrc_tree.h"
#include "resrc_reqst.h"
#include "rs2rank.h"
#include "rsreader.h"
#include "scheduler.h"
#include "plugin.h"

#include "../simulator/simulator.h"

#define DYNAMIC_SCHEDULING 0
#define ENABLE_TIMER_EVENT 0
#define SCHED_UNIMPL -1

#if ENABLE_TIMER_EVENT
static int timer_event_cb (flux_t *h, void *arg);
#endif
static void ev_prep_cb (flux_reactor_t *r, flux_watcher_t *w,
                        int revents, void *arg);
static void ev_check_cb (flux_reactor_t *r, flux_watcher_t *w,
                         int revents, void *arg);
static void res_event_cb (flux_t *h, flux_msg_handler_t *w,
                          const flux_msg_t *msg, void *arg);
static int job_status_cb (const char *jcbstr, void *arg, int errnum);


/******************************************************************************
 *                                                                            *
 *              Scheduler Framework Service Module Context                    *
 *                                                                            *
 ******************************************************************************/

typedef struct {
    json_t  *jcb;
    void         *arg;
    int           errnum;
} jsc_event_t;

typedef struct {
    flux_t       *h;
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
    char         *userplugin_opts;
    bool          sim;
    bool          schedonce;          /* Use resources only once */
    bool          fail_on_error;      /* Fail immediately on error */
    int           verbosity;
    rsreader_t    r_mode;
    sched_params_t s_params;
} ssrvarg_t;

/* TODO: Implement prioritization function for p_queue */
typedef struct {
    flux_t       *h;
    zhash_t      *job_index;          /* For fast job lookup for all queues*/
    zlist_t      *p_queue;            /* Pending job priority queue */
    bool          pq_state;           /* schedulable state change in p_queue */
    zlist_t      *r_queue;            /* Running job queue */
    zlist_t      *c_queue;            /* Complete/cancelled job queue */
    machs_t      *machs;              /* Helps resolve resources to ranks */
    ssrvarg_t     arg;                /* args passed to this module */
    rdlctx_t      rctx;               /* RDL context */
    simctx_t      sctx;               /* simulator context */
    struct sched_plugin_loader *loader; /* plugin loader */
    flux_watcher_t *before;
    flux_watcher_t *after;
    flux_watcher_t *idle;
} ssrvctx_t;


/******************************************************************************
 *                                                                            *
 *                                 Utilities                                  *
 *                                                                            *
 ******************************************************************************/

static inline void sched_params_default (sched_params_t *params)
{
    params->queue_depth = SCHED_PARAM_Q_DEPTH_DEFAULT;
    params->delay_sched = SCHED_PARAM_DELAY_DEFAULT;
    /* set other scheduling parameters to their default values here */
}

static inline int sched_params_args (char *arg, sched_params_t *params)
{
    int rc = 0;
    char *argz = NULL;
    size_t argz_len = 0;
    const char *e = NULL;
    char *o_arg = NULL;
    long val = 0;

    argz_create_sep (arg, ',', &argz, &argz_len);
    while ((e = argz_next (argz, argz_len, e))) {
        if (!strncmp ("queue-depth=", e, sizeof ("queue-depth"))) {
            o_arg = strstr(e, "=") + 1;
            val = strtol(o_arg, (char **) NULL, 10);
            if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN))
                   || (errno != 0 && val == 0))
                rc = -1;
            else
                params->queue_depth = val;
        } else if (!strncmp ("delay-sched=", e, sizeof ("delay-sched"))) {
            if (!strncmp ((strstr(e, "=") + 1), "true", sizeof ("true")))
                params->delay_sched = true;
            else if (!strncmp ((strstr(e, "=") + 1), "false", sizeof ("false")))
                params->delay_sched = false;
            else {
                errno = EINVAL;
                rc = -1;
            }
        } else {
           errno = EINVAL;
           rc = -1;
        }

        if (rc != 0)
            break;
    }

    if (argz)
        free (argz);
    return rc;
}

static inline void ssrvarg_init (ssrvarg_t *arg)
{
    arg->path = NULL;
    arg->uri = NULL;
    arg->userplugin = NULL;
    arg->userplugin_opts = NULL;
    arg->sim = false;
    arg->schedonce = false;
    arg->fail_on_error = false;
    arg->verbosity = 0;
    sched_params_default (&(arg->s_params));
}

static inline void ssrvarg_free (ssrvarg_t *arg)
{
    if (arg->path)
        free (arg->path);
    if (arg->uri)
        free (arg->uri);
    if (arg->userplugin)
        free (arg->userplugin);
    if (arg->userplugin_opts)
        free (arg->userplugin_opts);
}

static inline int ssrvarg_process_args (int argc, char **argv, ssrvarg_t *a)
{
    int i = 0, rc = 0;
    char *schedonce = NULL;
    char *immediate = NULL;
    char *vlevel= NULL;
    char *sim = NULL;
    char *sprms = NULL;
    for (i = 0; i < argc; i++) {
        if (!strncmp ("rdl-conf=", argv[i], sizeof ("rdl-conf"))) {
            a->path = xstrdup (strstr (argv[i], "=") + 1);
        } else if (!strncmp ("sched-once=", argv[i], sizeof ("sched-once"))) {
            schedonce = xstrdup (strstr (argv[i], "=") + 1);
        } else if (!strncmp ("fail-on-error=", argv[i],
                    sizeof ("fail-on-error"))) {
            immediate = xstrdup (strstr (argv[i], "=") + 1);
        } else if (!strncmp ("verbosity=", argv[i], sizeof ("verbosity"))) {
            vlevel = xstrdup (strstr (argv[i], "=") + 1);
        } else if (!strncmp ("rdl-resource=", argv[i], sizeof ("rdl-resource"))) {
            a->uri = xstrdup (strstr (argv[i], "=") + 1);
        } else if (!strncmp ("in-sim=", argv[i], sizeof ("in-sim"))) {
            sim = xstrdup (strstr (argv[i], "=") + 1);
        } else if (!strncmp ("plugin=", argv[i], sizeof ("plugin"))) {
            a->userplugin = xstrdup (strstr (argv[i], "=") + 1);
        } else if (!strncmp ("plugin-opts=", argv[i], sizeof ("plugin-opts"))) {
            a->userplugin_opts = xstrdup (strstr (argv[i], "=") + 1);
        } else if (!strncmp ("sched-params=", argv[i], sizeof ("sched-params"))) {
            sprms = xstrdup (strstr (argv[i], "=") + 1);
        } else {
            rc = -1;
            errno = EINVAL;
            goto done;
        }
    }

    if (!(a->userplugin))
        a->userplugin = xstrdup ("sched.fcfs");

    if (sim && !strncmp (sim, "true", sizeof ("true"))) {
        a->sim = true;
        free (sim);
    }
    if (schedonce && !strncmp (schedonce, "true", sizeof ("true"))) {
        a->schedonce = true;
        free (schedonce);
    }
    if (immediate && !strncmp (immediate, "true", sizeof ("true"))) {
        a->fail_on_error = true;
        free (immediate);
    }
    if (vlevel) {
         a->verbosity = strtol(vlevel, (char **)NULL, 10);
         free (vlevel);
    }
    if (a->path)
        a->r_mode = (a->sim)? RSREADER_RESRC_EMUL : RSREADER_RESRC;
    else
        a->r_mode = RSREADER_HWLOC;
    if (sprms)
        rc = sched_params_args (sprms, &(a->s_params));
done:
    return rc;
}

static int adjust_for_sched_params (ssrvctx_t *ctx)
{
    flux_reactor_t *r = NULL;
    int rc = 0;
    if (ctx->arg.s_params.delay_sched && !ctx->sctx.in_sim) {
        if (!(r = flux_get_reactor (ctx->h))) {
            rc = -1;
            goto done;
        }
        if (!(ctx->before = flux_prepare_watcher_create (r, ev_prep_cb, ctx))) {
            rc = -1;
            goto done;
        }
        if (!(ctx->after = flux_check_watcher_create (r, ev_check_cb, ctx))) {
            rc = -1;
            goto done;
        }
        /* idle watcher makes sure the check watcher (after) is called
           even with no external events delivered */
        if (!(ctx->idle = flux_idle_watcher_create (r, NULL, NULL))) {
            rc = -1;
            goto done;
        }
        flux_watcher_start (ctx->before);
        flux_watcher_start (ctx->after);
    }
done:
    return rc;
}


static void freectx (void *arg)
{
    ssrvctx_t *ctx = arg;
    zhash_destroy (&(ctx->job_index));
    zlist_destroy (&(ctx->p_queue));
    zlist_destroy (&(ctx->r_queue));
    zlist_destroy (&(ctx->c_queue));
    rs2rank_tab_destroy (ctx->machs);
    ssrvarg_free (&(ctx->arg));
    resrc_tree_destroy (resrc_phys_tree (ctx->rctx.root_resrc), true);
    resrc_fini ();
    free (ctx->rctx.root_uri);
    free_simstate (ctx->sctx.sim_state);
    if (ctx->sctx.res_queue)
        zlist_destroy (&(ctx->sctx.res_queue));
    if (ctx->sctx.jsc_queue)
        zlist_destroy (&(ctx->sctx.jsc_queue));
    if (ctx->sctx.timer_queue)
        zlist_destroy (&(ctx->sctx.timer_queue));
    if (ctx->loader)
        sched_plugin_loader_destroy (ctx->loader);
    if (ctx->before)
        flux_watcher_destroy (ctx->before);
    if (ctx->after)
        flux_watcher_destroy (ctx->after);
    if (ctx->idle)
        flux_watcher_destroy (ctx->idle);
    free (ctx);
}

static ssrvctx_t *getctx (flux_t *h)
{
    ssrvctx_t *ctx = (ssrvctx_t *)flux_aux_get (h, "sched");
    if (!ctx) {
        ctx = xzmalloc (sizeof (*ctx));
        ctx->h = h;
        if (!(ctx->job_index = zhash_new ()))
            oom ();
        if (!(ctx->p_queue = zlist_new ()))
            oom ();
        ctx->pq_state = false;
        if (!(ctx->r_queue = zlist_new ()))
            oom ();
        if (!(ctx->c_queue = zlist_new ()))
            oom ();
        if (!(ctx->machs = rs2rank_tab_new ()))
            oom ();
        ssrvarg_init (&(ctx->arg));
        resrc_init ();
        ctx->rctx.root_resrc = NULL;
        ctx->rctx.root_uri = NULL;
        ctx->sctx.in_sim = false;
        ctx->sctx.sim_state = NULL;
        ctx->sctx.res_queue = NULL;
        ctx->sctx.jsc_queue = NULL;
        ctx->sctx.timer_queue = NULL;
        ctx->loader = NULL;
        ctx->before = NULL;
        ctx->after = NULL;
        ctx->idle = NULL;
        flux_aux_set (h, "sched", ctx, freectx);
    }
    return ctx;
}

static inline void get_jobid (json_t *jcb, int64_t *jid)
{
    Jget_int64 (jcb, JSC_JOBID, jid);
}

static inline void get_states (json_t *jcb, int64_t *os, int64_t *ns)
{
    json_t *o = NULL;
    Jget_obj (jcb, JSC_STATE_PAIR, &o);
    Jget_int64 (o, JSC_STATE_PAIR_OSTATE, os);
    Jget_int64 (o, JSC_STATE_PAIR_NSTATE, ns);
}

static inline int fill_resource_req (flux_t *h, flux_lwj_t *j)
{
    char *jcbstr = NULL;
    int rc = -1;
    int64_t nn = 0;
    int64_t nc = 0;
    int64_t walltime = 0;
    json_t *jcb = NULL;
    json_t *o = NULL;

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
    if (!Jget_int64 (o, JSC_RDESC_WALLTIME, &walltime) || !walltime) {
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

static int update_state (flux_t *h, uint64_t jid, job_state_t os, job_state_t ns)
{
    const char *jcbstr = NULL;
    int rc = -1;
    json_t *jcb = Jnew ();
    json_t *o = Jnew ();
    Jadd_int64 (o, JSC_STATE_PAIR_OSTATE, (int64_t) os);
    Jadd_int64 (o, JSC_STATE_PAIR_NSTATE , (int64_t) ns);
    /* don't want to use Jadd_obj because I want to transfer the ownership */
    json_object_set_new (jcb, JSC_STATE_PAIR, o);
    jcbstr = Jtostr (jcb);
    rc = jsc_update_jcb (h, jid, JSC_STATE_PAIR, jcbstr);
    Jput (jcb);
    return rc;
}

static inline bool is_newjob (json_t *jcb)
{
    int64_t os = J_NULL, ns = J_NULL;
    get_states (jcb, &os, &ns);
    return ((os == J_NULL) && (ns == J_NULL))? true : false;
}

static int plugin_process_args (ssrvctx_t *ctx, char *userplugin_opts)
{
    int rc = -1;
    char *argz = NULL;
    size_t argz_len = 0;
    struct sched_plugin *plugin = sched_plugin_get (ctx->loader);
    const sched_params_t *sp = &(ctx->arg.s_params);

    if (userplugin_opts)
        argz_create_sep (userplugin_opts, ',', &argz, &argz_len);
    if (plugin->process_args (ctx->h, argz, argz_len, sp) < 0)
        goto done;

    rc = 0;

 done:
    if (argz)
        free (argz);

    return rc;
}


/********************************************************************************
 *                                                                              *
 *                          Simple Job Queue Methods                            *
 *                                                                              *
 *******************************************************************************/

static int q_enqueue_into_pqueue (ssrvctx_t *ctx, json_t *jcb)
{
    int rc = -1;
    int64_t jid = -1;
    char *key = NULL;
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
    job->enqueue_pos = (int64_t)zlist_size (ctx->p_queue);
    key = xasprintf ("%"PRId64"", jid);
    if (zhash_insert(ctx->job_index, key, job) != 0) {
        flux_log (ctx->h, LOG_ERR, "failed to index a job.");
        goto done;
    }
    /* please don't free the job using job_index; this is just a lookup table */
    rc = 0;
done:
    if (key)
        free (key);
    return rc;
}

static flux_lwj_t *q_find_job (ssrvctx_t *ctx, int64_t id)
{
    flux_lwj_t *j = NULL;
    char *key = NULL;
    key = xasprintf ("%"PRId64"", id);
    j = zhash_lookup (ctx->job_index, key);
    free (key);
    return j;
}

static int q_mark_schedulability (ssrvctx_t *ctx, flux_lwj_t *job)
{
    if (ctx->pq_state == false
        && job->enqueue_pos <= ctx->arg.s_params.queue_depth) {
        ctx->pq_state = true;
        return 0;
    }
    return -1;
}

static int q_move_to_rqueue (ssrvctx_t *ctx, flux_lwj_t *j)
{
    zlist_remove (ctx->p_queue, j);
    /* dequeue operation should always be a schedulable queue operation */
    if (ctx->pq_state == false)
        ctx->pq_state = true;
    return zlist_append (ctx->r_queue, j);
}

static int q_move_to_cqueue (ssrvctx_t *ctx, flux_lwj_t *j)
{
    /* NOTE: performance issue? */
    // FIXME: no transition from pending queue to cqueue yet
    //zlist_remove (ctx->p_queue, j);
    zlist_remove (ctx->r_queue, j);
    if (ctx->pq_state == false)
        ctx->pq_state = true;
    return zlist_append (ctx->c_queue, j);
}

static flux_lwj_t *fetch_job_and_event (ssrvctx_t *ctx, json_t *jcb,
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
 *                   Setting Up RDL (RFC 4)                                   *
 *                                                                            *
 ******************************************************************************/

static void setup_rdl_lua (flux_t *h)
{
    flux_log (h, LOG_DEBUG, "LUA_PATH %s", getenv ("LUA_PATH"));
    flux_log (h, LOG_DEBUG, "LUA_CPATH %s", getenv ("LUA_CPATH"));
}

/* Block until value of 'key' becomes non-NULL.
 * It is an EPROTO error if value is type other than json_type_string.
 * On success returns value, otherwise NULL with errno set.
 */
static json_t *get_string_blocking (flux_t *h, const char *key)
{
    char *json_str = NULL; /* initial value for watch */
    json_t *o = NULL;
    int saved_errno;

    if (kvs_watch_once (h, key, &json_str) < 0) {
        saved_errno = errno;
        goto error;
    }

    if (!json_str || !(o = Jfromstr (json_str))
                  || !json_is_string (o)) {
        saved_errno = EPROTO;
        goto error;
    }
    free (json_str);
    return o;
error:
    if (json_str)
        free (json_str);
    if (o)
        Jput (o);
    errno = saved_errno;
    return NULL;
}

static int build_hwloc_rs2rank (ssrvctx_t *ctx, rsreader_t r_mode)
{
    int rc = -1;
    uint32_t rank = 0, size = 0;

    if (flux_get_size (ctx->h, &size) == -1) {
        flux_log_error (ctx->h, "flux_get_size");
        goto done;
    }
    for (rank=0; rank < size; rank++) {
        json_t *o;
        char k[64];
        int n = snprintf (k, sizeof (k), "resource.hwloc.xml.%"PRIu32"", rank);
        assert (n < sizeof (k));
        if (!(o = get_string_blocking (ctx->h, k))) {
            flux_log_error (ctx->h, "kvs_get %s", k);
            goto done;
        }
        const char *s = json_string_value (o);
        char *err_str = NULL;
        size_t len = strlen (s);
        if (rsreader_hwloc_load (s, len, rank, r_mode, &(ctx->rctx.root_resrc),
                                 ctx->machs, &err_str)) {
            Jput (o);
            flux_log_error (ctx->h, "can't load hwloc data: %s", err_str);
            free (err_str);
            goto done;
        }
        Jput (o);
    }
    rc = 0;

done:
    return rc;
}

static void dump_resrc_state (flux_t *h, resrc_tree_t *rt)
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
    char *e_str = NULL;
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
        if (rsreader_link2rank (ctx->machs, tres, &e_str) != 0) {
            flux_log (ctx->h, LOG_INFO, "RDL(%s) inconsistent w/ hwloc!", path);
            if (e_str) {
                flux_log (ctx->h, LOG_INFO, "%s", e_str);
                free (e_str);
            }
            if (ctx->arg.fail_on_error)
                goto done;
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
static void start_cb (flux_t *h,
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
    json_t *jcb = NULL;
    ssrvctx_t *ctx = getctx ((flux_t *)arg);
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

static void sim_res_event_cb (flux_t *h, flux_msg_handler_t *w,
                              const flux_msg_t *msg, void *arg) {
    ssrvctx_t *ctx = getctx ((flux_t *)arg);
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

static void trigger_cb (flux_t *h,
                        flux_msg_handler_t *w,
                        const flux_msg_t *msg,
                        void *arg)
{
    clock_t start, diff;
    double seconds;
    bool sched_loop;
    const char *json_str = NULL;
    json_t *o = NULL;
    ssrvctx_t *ctx = getctx (h);

    if (flux_request_decode (msg, NULL, &json_str) < 0 || json_str == NULL
        || !(o = Jfromstr (json_str))) {
        flux_log (h, LOG_ERR, "%s: bad message", __FUNCTION__);
        Jput (o);
        return;
    }

    flux_log (h, LOG_DEBUG, "Setting sim_state to new values");
    ctx->sctx.sim_state = json_to_sim_state (o);
    ev_prep_cb (NULL, NULL, 0, ctx);

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

    ev_check_cb (NULL, NULL, 0, ctx);
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
    flux_t *h = ctx->h;

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
    flux_t *h = ctx->h;

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
    flux_t *h = ctx->h;
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
        flux_log (ctx->h, LOG_INFO, "hostname: %s, digest: %s", resrc_name (r),
                                     resrc_digest (r));
        rc = rs2rank_tab_query_by_sign (ctx->machs, resrc_name (r),
                                        resrc_digest (r), false, rank);
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

static void inline build_contain_1node_req (int64_t nc, int64_t rank,
					    json_t *rarr)
{
    json_t *e = Jnew ();
    json_t *o = Jnew ();
    Jadd_int64 (o, JSC_RDL_ALLOC_CONTAINING_RANK, rank);
    Jadd_int64 (o, JSC_RDL_ALLOC_CONTAINED_NCORES, nc);
    json_object_set_new (e, JSC_RDL_ALLOC_CONTAINED, o);
    json_array_append_new (rarr, e);
}

static int n_resources_of_type (resrc_tree_t *rt, const char *type)
{
    int n = 0;
    resrc_t *r = NULL;

    if (rt) {
        r = resrc_tree_resrc (rt);
        if (! strcmp (resrc_type (r), type)) {
            return 1;
        } else {
            if (resrc_tree_num_children (rt)) {
                resrc_tree_list_t *children = resrc_tree_children (rt);
                if (children) {
                    resrc_tree_t *child = resrc_tree_list_first (children);
                    while (child) {
                        n += n_resources_of_type(child, type);
                        child = resrc_tree_list_next (children);
                    }
                }
            }
        }
    }
    return n;
}


/*
 * Because the job's rdl should only contain what's allocated to the job,
 * we traverse the entire tree in the post-order walk fashion
 */
static int build_contain_req (ssrvctx_t *ctx, flux_lwj_t *job, resrc_tree_t *rt,
                              json_t *arr)
{
    int rc = -1;
    uint32_t rank = 0;
    resrc_t *r = NULL;

    if (rt) {
        r = resrc_tree_resrc (rt);
        if (strcmp (resrc_type (r), "node")) {
            if (resrc_tree_num_children (rt)) {
                resrc_tree_list_t *children = resrc_tree_children (rt);
                if (children) {
                    resrc_tree_t *child = resrc_tree_list_first (children);
                    while (child) {
                        build_contain_req (ctx, job, child, arr);
                        child = resrc_tree_list_next (children);
                    }
                }
            }
        } else {
            if (bridge_rs2rank_tab_query (ctx, r, &rank))
                goto done;
            else {
                int cores = job->req->corespernode ? job->req->corespernode :
                    n_resources_of_type(rt, "core");
                build_contain_1node_req (cores, rank, arr);
            }
        }
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
    flux_t *h = ctx->h;
    json_t *jcb = Jnew ();
    json_t *ro = Jnew ();
    json_t *arr = Jnew_ar ();

    if (resrc_tree_serialize (ro, job->resrc_tree)) {
        flux_log (h, LOG_ERR, "%"PRId64" resource serialization failed: %s",
                  job->lwj_id, strerror (errno));
        goto done;
    }
    json_object_set_new (jcb, JSC_RDL, ro);
    jcbstr = Jtostr (jcb);
    if (jsc_update_jcb (h, job->lwj_id, JSC_RDL, jcbstr) != 0) {
        flux_log (h, LOG_ERR, "error jsc udpate: %"PRId64" (%s)", job->lwj_id,
                  strerror (errno));
        goto done;
    }
    Jput (jcb);
    jcb = Jnew ();
    if (build_contain_req (ctx, job, job->resrc_tree, arr) != 0) {
        flux_log (h, LOG_ERR, "error requesting containment for job");
        goto done;
    }
    json_object_set_new (jcb, JSC_RDL_ALLOC, arr);
    jcbstr = Jtostr (jcb);
    if (jsc_update_jcb (h, job->lwj_id, JSC_RDL_ALLOC, jcbstr) != 0) {
        flux_log (h, LOG_ERR, "error updating jcb");
        goto done;
    }
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
static int req_tpexec_grow (flux_t *h, flux_lwj_t *job)
{
    /* TODO: NOT IMPLEMENTED */
    /* This runtime grow service will grow the resource set of the job.
       The special non-elastic case will be to grow the resource from
       zero to the selected RDL
    */
    return SCHED_UNIMPL;
}

static int req_tpexec_shrink (flux_t *h, flux_lwj_t *job)
{
    /* TODO: NOT IMPLEMENTED */
    return SCHED_UNIMPL;
}

static int req_tpexec_map (flux_t *h, flux_lwj_t *job)
{
    /* TODO: NOT IMPLEMENTED */
    /* This runtime grow service will grow the resource set of the job.
       The special non-elastic case will be to grow the resource from
       zero to RDL
    */
    return SCHED_UNIMPL;
}
#endif

static int req_tpexec_exec (flux_t *h, flux_lwj_t *job)
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

static int req_tpexec_run (flux_t *h, flux_lwj_t *job)
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
    json_t *req_res = NULL;
    flux_t *h = ctx->h;
    int rc = -1;
    int64_t nfound = 0;
    int64_t nreqrd = 0;
    resrc_reqst_t *resrc_reqst = NULL;
    resrc_tree_t *found_tree = NULL;
    resrc_tree_t *selected_tree = NULL;
    struct sched_plugin *plugin = sched_plugin_get (ctx->loader);

    if (!plugin)
        return rc;

    /*
     * Require at least one task per node, and
     * Assume (for now) one task per core.
     *
     * At this point, our flux_lwj_t structure supplies a simple count
     * of nodes and cores.  This is a short term solution that
     * supports the typical request.  Until a more complex model is
     * available, we will have to interpret the request along these
     * most likely scenarios:
     *
     * - If only cores are requested, the number of nodes we find to
     *   supply the requested cores does not matter to the user.
     *
     * - If only nodes are requested, we will return only nodes whose
     *   cores are all idle.
     *
     * - If nodes and cores are requested, we will return the
     *   requested number of nodes with at least the requested number
     *   of cores on each node.  We will not attempt to provide a
     *   balanced number of cores per node.
     */
    req_res = Jnew ();
    if (job->req->nnodes > 0) {
        json_t *child_core = Jnew ();

        Jadd_str (req_res, "type", "node");
        Jadd_int64 (req_res, "req_qty", job->req->nnodes);
        nreqrd = job->req->nnodes;

        /* Since nodes are requested, make sure we look for at
         * least one core on each node */
        if (job->req->ncores < job->req->nnodes)
            job->req->ncores = job->req->nnodes;
        job->req->corespernode = (job->req->ncores + job->req->nnodes - 1) /
            job->req->nnodes;
        if (job->req->node_exclusive) {
            Jadd_int64 (req_res, "req_size", 1);
            Jadd_bool (req_res, "exclusive", true);
        } else {
            Jadd_int64 (req_res, "req_size", 0);
            Jadd_bool (req_res, "exclusive", false);
        }

        Jadd_str (child_core, "type", "core");
        Jadd_int64 (child_core, "req_qty", job->req->corespernode);
        /* setting size == 1 devotes (all of) the core to the job */
        Jadd_int64 (child_core, "req_size", 1);
        /* setting exclusive to true prevents multiple jobs per core */
        Jadd_bool (child_core, "exclusive", true);
        Jadd_int64 (child_core, "starttime", starttime);
        Jadd_int64 (child_core, "endtime", starttime + job->req->walltime);
        json_object_set_new (req_res, "req_child", child_core);
    } else if (job->req->ncores > 0) {
        Jadd_str (req_res, "type", "core");
        Jadd_int (req_res, "req_qty", job->req->ncores);
        nreqrd = job->req->ncores;

        Jadd_int64 (req_res, "req_size", 1);
        /* setting exclusive to true prevents multiple jobs per core */
        Jadd_bool (req_res, "exclusive", true);
    } else
        goto done;

    Jadd_int64 (req_res, "starttime", starttime);
    Jadd_int64 (req_res, "endtime", starttime + job->req->walltime);
    resrc_reqst = resrc_reqst_from_json (req_res, NULL);
    Jput (req_res);
    if (!resrc_reqst)
        goto done;

    if ((nfound = plugin->find_resources (h, ctx->rctx.root_resrc,
                                          resrc_reqst, &found_tree))) {
        flux_log (h, LOG_DEBUG, "Found %"PRId64" %s(s) for job %"PRId64", "
                  "required: %"PRId64"", nfound,
                  resrc_type (resrc_reqst_resrc (resrc_reqst)), job->lwj_id,
                  nreqrd);

        resrc_tree_unstage_resources (found_tree);
        resrc_reqst_clear_found (resrc_reqst);
        if ((selected_tree = plugin->select_resources (h, found_tree,
                                                       resrc_reqst, NULL))) {
            if (resrc_reqst_all_found (resrc_reqst)) {
                plugin->allocate_resources (h, selected_tree, job->lwj_id,
                                            starttime, starttime +
                                            job->req->walltime);
                /* Scheduler specific job transition */
                // TODO: handle this some other way (JSC?)
                job->starttime = starttime;
                job->state = J_SELECTED;
                job->resrc_tree = selected_tree;
                if (req_tpexec_allocate (ctx, job) != 0) {
                    flux_log (h, LOG_ERR,
                              "failed to request allocate for job %"PRId64"",
                              job->lwj_id);
                    resrc_tree_destroy (job->resrc_tree, false);
                    job->resrc_tree = NULL;
                    goto done;
                }
                flux_log (h, LOG_DEBUG, "Allocated %"PRId64" %s(s) for job "
                          "%"PRId64"", nreqrd,
                          resrc_type (resrc_reqst_resrc (resrc_reqst)),
                          job->lwj_id);
            } else {
                rc = plugin->reserve_resources (h, &selected_tree, job->lwj_id,
                                                starttime, job->req->walltime,
                                                ctx->rctx.root_resrc,
                                                resrc_reqst);
                if (rc) {
                    resrc_tree_destroy (selected_tree, false);
                    job->resrc_tree = NULL;
                } else
                    job->resrc_tree = selected_tree;
            }
        }
    }
    rc = 0;
done:
    if (resrc_reqst)
        resrc_reqst_destroy (resrc_reqst);
    if (found_tree)
        resrc_tree_destroy (found_tree, false);

    return rc;
}

static int schedule_jobs (ssrvctx_t *ctx)
{
    int rc = 0;
    int qdepth = 0;
    flux_lwj_t *job = NULL;
    struct sched_plugin *plugin = sched_plugin_get (ctx->loader);
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
    if (!plugin)
        return -1;
    rc = plugin->sched_loop_setup (ctx->h);
    job = zlist_first (jobs);
    while (!rc && job && (qdepth < ctx->arg.s_params.queue_depth)) {
        if (job->state == J_SCHEDREQ) {
            rc = schedule_job (ctx, job, starttime);
        }
        job = (flux_lwj_t*)zlist_next (jobs);
        qdepth++;
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
    flux_t *h = ctx->h;
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
        if (!ctx->arg.s_params.delay_sched)
            schedule_jobs (ctx);
        else
            q_mark_schedulability (ctx, job);
        break;
    case J_SCHEDREQ:
        /* A schedule reqeusted should not get an event. */
        /* SCHEDREQ -> SELECTED happens implicitly within schedule_jobs */
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
            if (resrc_tree_release (job->resrc_tree, job->lwj_id)) {
                flux_log (h, LOG_ERR, "%s: failed to release resources for job "
                      "%"PRId64"", __FUNCTION__, job->lwj_id);
            }
        }
        if (!ctx->arg.s_params.delay_sched) {
            flux_msg_t *msg = flux_event_encode ("sched.res.freed", NULL);
            if (!msg || flux_send (h, msg, 0) < 0) {
                flux_log (h, LOG_ERR, "%s: error sending event: %s",
                          __FUNCTION__, strerror (errno));
            } else {
                flux_msg_destroy (msg);
                flux_log (h, LOG_DEBUG, "Released resources for job %"PRId64"",
                          job->lwj_id);
            }
        }
        break;
    case J_CANCELLED:
        VERIFY (trans (J_REAPED, newstate, &(job->state)));
        zlist_remove (ctx->c_queue, job);
        if (job->req)
            free (job->req);
        resrc_tree_destroy (job->resrc_tree, false);
        free (job);
        break;
    case J_COMPLETE:
        VERIFY (trans (J_REAPED, newstate, &(job->state)));
        zlist_remove (ctx->c_queue, job);
        if (job->req)
            free (job->req);
        resrc_tree_destroy (job->resrc_tree, false);
        free (job);
        break;
    case J_REAPED:
    default:
        /* TODO: when reap functionality is implemented
           not only remove the job from ctx->c_queue but also
           remove it from ctx->job_index.
         */
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
static void res_event_cb (flux_t *h, flux_msg_handler_t *w,
                          const flux_msg_t *msg, void *arg)
{
    schedule_jobs (getctx ((flux_t *)arg));
    return;
}

#if ENABLE_TIMER_EVENT
static int timer_event_cb (flux_t *h, void *arg)
{
    //flux_log (h, LOG_ERR, "TIMER CALLED");
    schedule_jobs (getctx ((flux_t *)arg));
    return 0;
}
#endif

static int job_status_cb (const char *jcbstr, void *arg, int errnum)
{
    json_t *jcb = NULL;
    ssrvctx_t *ctx = getctx ((flux_t *)arg);
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
        q_enqueue_into_pqueue (ctx, jcb);
    if ((j = fetch_job_and_event (ctx, jcb, &ns)) == NULL) {
        flux_log (ctx->h, LOG_ERR, "error fetching job and event");
        return -1;
    }
    Jput (jcb);
    return action (ctx, j, ns);
}

static void ev_prep_cb (flux_reactor_t *r, flux_watcher_t *w, int ev, void *a)
{
    ssrvctx_t *ctx = (ssrvctx_t *)a;
    if (ctx->pq_state && ctx->idle)
        flux_watcher_start (ctx->idle);
}

static void ev_check_cb (flux_reactor_t *r, flux_watcher_t *w, int ev, void *a)
{
    ssrvctx_t *ctx = (ssrvctx_t *)a;
    if (ctx->idle)
        flux_watcher_stop (ctx->idle);
    if (ctx->pq_state) {
        flux_log (ctx->h, LOG_DEBUG, "check callback about to schedule jobs.");
        ctx->pq_state = false;
        schedule_jobs (ctx);
    }
}


/******************************************************************************
 *                                                                            *
 *                     Scheduler Service Module Main                          *
 *                                                                            *
 ******************************************************************************/

const sched_params_t *sched_params_get (flux_t *h)
{
    const sched_params_t *rp = NULL;
    ssrvctx_t *ctx = NULL;
    if (!(ctx = getctx (h))) {
        flux_log (h, LOG_ERR, "can't find or allocate the context");
        goto done;
    }
    rp = (const sched_params_t *) &(ctx->arg.s_params);
done:
    return rp;
}

int mod_main (flux_t *h, int argc, char **argv)
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
    if (bridge_set_execmode (ctx) != 0) {
        flux_log (h, LOG_ERR, "failed to setup execution mode");
        goto done;
    }
    if (adjust_for_sched_params (ctx) != 0) {
        flux_log (h, LOG_ERR, "can't adjust for schedule parameters");
        goto done;
    }
    flux_log (h, LOG_INFO, "sched comms module starting");
    if (!(ctx->loader = sched_plugin_loader_create (h))) {
        flux_log_error (h, "failed to initialize plugin loader");
        goto done;
    }
    if (ctx->arg.userplugin) {
        if (sched_plugin_load (ctx->loader, ctx->arg.userplugin) < 0) {
            flux_log_error (h, "failed to load %s", ctx->arg.userplugin);
            goto done;
        }
        if (plugin_process_args (ctx, ctx->arg.userplugin_opts) < 0) {
            flux_log_error (h, "failed to process args for %s", ctx->arg.userplugin);
            goto done;
        }
        flux_log (h, LOG_INFO, "%s plugin loaded", ctx->arg.userplugin);
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

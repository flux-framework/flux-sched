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
 * schedsrv.c - scheduler frameowrk service comms module 
 *
 * Update Log:
 *       Apr 12 2015 DHA: Code refactoring including JSC API integration
 *       May 24 2014 DHA: File created.
 */

#include <czmq.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <libgen.h>
#include <dlfcn.h>
#include <stdbool.h>
#include <json/json.h>
#include <flux/core.h>

/* We assume the abstraction in rdl.h is generic for now */
#include "rdl.h"
#include "scheduler.h"
#include "src/common/libutil/log.h"
#include "src/common/libutil/jsonutil.h"
#include "src/common/libutil/xzmalloc.h"
#include "src/common/libutil/shortjson.h"

#define DYNAMIC_SCHEDULING 0
#define ENABLE_TIMER_EVENT 0
#define SCHED_UNIMPL -1

#if ENABLE_TIMER_EVENT
static int timer_event_cb (flux_t h, void *arg);
#endif
static int res_event_cb (flux_t h, int t, zmsg_t **zmsg, void *arg);
static int job_status_cb (JSON jcb, void *arg, int errnum);

/******************************************************************************
 *                                                                            *
 *              Scheduler Framework Service Module Context                    *
 *                                                                            *
 ******************************************************************************/

typedef struct rdl *(FIND_PTR)( flux_t h, struct rdl *r, const char *u, 
                                flux_lwj_t *j, bool *p);
typedef int (SEL_PTR)(flux_t h, struct rdl *r, const char *u, 
                                struct resource *f, flux_lwj_t *j, bool s);
typedef int (REL_PTR)(flux_t h, struct rdl *r, const char *u, flux_lwj_t *j);

typedef struct sched_ops {
    void          *dso;               /* Scheduler plug-in DSO handle */
    FIND_PTR      *find_resources;    /* func pointer to find resources */
    SEL_PTR       *select_resources;  /* func pointer to select resources */
    REL_PTR       *release_resources; /* func pointer to free resources */
} sched_ops_t; 

typedef struct {
    struct rdllib *rdl_lib;           /* rdllib */ 
    struct rdl    *root_rdl;          /* rdl object pointing to the root */
    char          *root_uri;          /* Name of the root of the RDL hierachy */
} rdlctx_t;

/* TODO: Implement prioritization function for p_queue */
typedef struct {
    flux_t        h;
    zlist_t      *p_queue;            /* Pending job priority queue */
    zlist_t      *r_queue;            /* Running job queue */
    zlist_t      *c_queue;            /* Complete/cancelled job queue */
    rdlctx_t      rctx;               /* RDL context */
    sched_ops_t   sops;               /* scheduler plugin operations */
} ssrvctx_t;

/******************************************************************************
 *                                                                            *
 *                                 Utilities                                  *
 *                                                                            *
 ******************************************************************************/
static void freectx (ssrvctx_t *ctx)
{
    /* FIXME: we probably need some item free hooked into the lists 
     * ignore this for a while. 
     */
    zlist_destroy (&(ctx->p_queue));
    zlist_destroy (&(ctx->r_queue));
    zlist_destroy (&(ctx->c_queue));
    rdllib_close (ctx->rctx.rdl_lib);
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
        ctx->rctx.rdl_lib = NULL;
        ctx->rctx.root_uri = NULL;
        ctx->rctx.root_rdl = NULL;
        ctx->sops.dso = NULL;
        ctx->sops.find_resources = NULL;
        ctx->sops.select_resources = NULL;
        ctx->sops.release_resources = NULL;
        flux_aux_set (h, "schedsrv", ctx, (FluxFreeFn)freectx);
    }
    return ctx;
}

static void f_err (flux_t h, const char *msg, ...)
{
    va_list ap;
    va_start (ap, msg);
    flux_vlog (h, LOG_ERR, msg, ap);
    va_end (ap);
}

static inline void get_jobid (JSON jcb, int64_t *jid)
{
    Jget_int64 (jcb, JSC_JOBID, jid);
}

static inline void get_states (JSON jcb, int64_t *os, int64_t *ns)
{
    JSON o;
    Jget_obj (jcb, JSC_STATE_PAIR, &o);
    Jget_int64 (o, JSC_STATE_PAIR_OSTATE, os);
    Jget_int64 (o, JSC_STATE_PAIR_NSTATE, ns);
}

static inline int fill_resource_req (flux_t h, flux_lwj_t *j)
{
    int rc = -1;
    int64_t nn = 0; 
    int64_t nc = 0; 
    JSON jcb = NULL;
    JSON o = NULL;

    if (!j) goto done;

    j->req = (flux_res_t *) xzmalloc (sizeof (flux_res_t));
    if ((rc = jsc_query_jcb (h, j->lwj_id, JSC_RDESC, &jcb)) != 0) {
        flux_log (h, LOG_ERR, "error in jsc_query_job.");
        goto done;
    }
    if (!Jget_obj (jcb, JSC_RDESC, &o)) goto done; 
    if (!Jget_int64 (o, JSC_RDESC_NNODES, &nn)) goto done;
    if (!Jget_int64 (o, JSC_RDESC_NTASKS, &nc)) goto done;
    j->req->nnodes = (uint64_t) nn;
    j->req->ncores = (uint32_t) nc;
    rc = 0;
done:
    if (jcb) 
        Jput (jcb);
    return rc;
}

static int update_state (flux_t h, uint64_t jid, job_state_t os, job_state_t ns)
{
    int rc = -1;
    JSON jcb = Jnew ();
    JSON o = Jnew ();
    Jadd_int64 (o, JSC_STATE_PAIR_OSTATE, (int64_t) os);
    Jadd_int64 (o, JSC_STATE_PAIR_NSTATE , (int64_t) ns);
    /* don't want to use Jadd_obj because I want to transfer the ownership */
    json_object_object_add (jcb, JSC_STATE_PAIR, o);
    rc = jsc_update_jcb (h, jid, JSC_STATE_PAIR, jcb);
    Jput (jcb);
    return rc;
}

static inline bool is_newjob (JSON jcb)
{
    int64_t os, ns;
    get_states (jcb, &os, &ns);
    return ((os == J_NULL) && (ns == J_NULL))? true : false;
}

static bool inline is_node (const char *t)
{
    return (strcmp (t, "node") == 0)? true: false; 
}

static bool inline is_core (const char *t)
{
    return (strcmp (t, "core") == 0)? true: false; 
}

static int append_to_pqueue (ssrvctx_t *ctx, JSON jcb)
{
    int rc = -1;
    int64_t jid = -1;;
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

static flux_lwj_t *fetch_job_and_event (ssrvctx_t *ctx, JSON jcb, job_state_t *ns)
{
    int64_t jid = -1, os64 = 0, ns64 = 0;
    get_jobid (jcb, &jid);
    get_states (jcb, &os64, &ns64);
    *ns = (job_state_t) ns64;
    return q_find_job (ctx, jid);
}

#if 0
static char *ctime_iso8601_now (char *buf, size_t sz)
{
    struct tm tm;
    time_t now = time (NULL);
    memset (buf, 0, sz);
    if (!localtime_r (&now, &tm))
        err_exit ("localtime");
    strftime (buf, sz, "%FT%T", &tm);
    return buf;
}
#endif


/******************************************************************************
 *                                                                            *
 *                            Scheduler Plugin Loader                         *
 *                                                                            *
 ******************************************************************************/

static int resolve_functions (ssrvctx_t *ctx)
{
    int rc = -1; 

    ctx->sops.find_resources = dlsym (ctx->sops.dso, "find_resources");
    if (!(ctx->sops.find_resources) || !(*(ctx->sops.find_resources))) {
        flux_log (ctx->h, LOG_ERR, "can't load find_resources: %s", dlerror ());
        goto done;
    }
    ctx->sops.select_resources = dlsym (ctx->sops.dso, "select_resources");
    if (!(ctx->sops.select_resources) || !(*(ctx->sops.select_resources))) {
        flux_log (ctx->h, LOG_ERR, "can't load select_resources: %s", dlerror ());
        goto done;
    }
    ctx->sops.release_resources = dlsym (ctx->sops.dso, "release_resources");
    if (!(ctx->sops.release_resources) || !(*(ctx->sops.release_resources))) {
        flux_log (ctx->h, LOG_ERR, "can't load release_resources: %s", dlerror ());
        goto done;
    }
    rc = 0;

done:
    return rc;
}

static int load_sched_plugin (ssrvctx_t *ctx, const char *pin)
{
    int rc = -1;
    flux_t h = ctx->h;
    char *path = NULL;;
    char *searchpath = getenv ("FLUX_MODULE_PATH");

    if (!searchpath) {
        flux_log (h, LOG_ERR, "FLUX_MODULE_PATH not set");
        goto done;
    }
    if (!(path = flux_modfind (searchpath, pin))) {
        flux_log (h, LOG_ERR, "%s: not found in module search path %s",
                  pin, searchpath);
        goto done;
    }
    if (!(ctx->sops.dso = dlopen (path, RTLD_NOW | RTLD_LOCAL))) {
        flux_log (h, LOG_ERR, "failed to open sched plugin: %s",
                  dlerror ());
        goto done;
    }
    flux_log (h, LOG_DEBUG, "loaded: %s", pin);
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
    rdllib_set_default_errf (h, (rdl_err_f)(&f_err));
}

static int load_rdl (ssrvctx_t *ctx, zhash_t *args)
{
    int rc = -1;
    const char *path = NULL;

    setup_rdl_lua (ctx->h);
    if (!(path = zhash_lookup (args, "rdl-conf"))) {
        flux_log (ctx->h, LOG_ERR, "rdl-conf argument is not set");
        goto done;
    }
    if (!(ctx->rctx.rdl_lib = rdllib_open ()) 
        || !(ctx->rctx.root_rdl = rdl_loadfile (ctx->rctx.rdl_lib, path))) {
        flux_log (ctx->h, LOG_ERR, "can't load from %s: %s", path, strerror (errno));
        goto done;
    }
    if (!(ctx->rctx.root_uri = zhash_lookup (args, "rdl-resource"))) {
        flux_log (ctx->h, LOG_INFO, "using default rdl resource");
        ctx->rctx.root_uri = "default";
    }
    if (!rdl_resource_get (ctx->rctx.root_rdl, ctx->rctx.root_uri)) {
        flux_log (ctx->h, LOG_ERR, "can't get %s: %s", 
            ctx->rctx.root_uri, strerror (errno));
        goto done;
    }
    flux_log (ctx->h, LOG_DEBUG, "rdl successfully loaded");
    rc = 0;
done:
    return rc;
}


/******************************************************************************
 *                                                                            *
 *                     Scheduler Event Registeration                          *
 *                                                                            *
 ******************************************************************************/

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

    if (flux_msghandler_add (h, FLUX_MSGTYPE_EVENT, "sched.res.event",
                             res_event_cb, (void *)h) < 0) {
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


/********************************************************************************
 *                                                                              *
 *            Task Program Execution Service Request (RFC 8)                    *
 *                                                                              *
 ********************************************************************************/

static void inline build_contain_1node_req (int64_t nc, JSON rarr)
{
    JSON e = Jnew ();
    JSON o = Jnew ();
    Jadd_int64 (o, JSC_RDL_ALLOC_CONTAINED_NCORES, nc); 
    json_object_object_add (e, JSC_RDL_ALLOC_CONTAINED, o);
    json_object_array_add (rarr, e);
}

/*
 * Because the job's rdl should only contain what's allocated to the job,
 * this traverse the entire tree post-order walk 
 */
static int build_contain_req (flux_t h, int64_t j, struct resource *jr, JSON rarr)
{
    int rc = 0;
    const char *type = NULL;
    json_object *o = NULL;
    struct resource *c = NULL;

    if (!jr) return -1;
    if (!(o = rdl_resource_json (jr))) return -1;
    if (!Jget_str (o, "type", &type)) return -1;

    if (is_node (type)) {
        /* base case -- node */
        int64_t ncores;
        if (!(o = rdl_resource_aggregate_json (jr))) {
            rc = -1;
            goto done;
        }
        if (!Jget_int64 (o, "core", &ncores)) return -1;
        build_contain_1node_req (ncores, rarr); 
    } else {
        /* recurse -- post-order walk until reaching to the node level */
        while ((rc == 0) && (c = rdl_resource_next_child (jr))) {
            rc = build_contain_req (h, j, c, rarr); 
            /* FIXME: does c really need to be destroyed? */
            rdl_resource_destroy (c); 
        }
    }
done:
    if (o) 
        Jput (o);
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
    int rc = -1;
    flux_t h = ctx->h;
    char *rdlstr = NULL; 
    struct resource *jr = NULL;
    JSON jcb = Jnew (); 
    JSON arr = Jnew_ar ();

    /* NOTE: move the logic from allocate_resources in plugin to here */
    /* struct resource *jr = rdl_resource_get (job->rdl, uri) */
    if (!(rdlstr = rdl_serialize (job->rdl))) {
        flux_log (h, LOG_ERR, "%ld rdl_serialize failed: %s",
                  job->lwj_id, strerror (errno));
        goto done;
    }
    /* NOTE: this will make a copy of the string */
    Jadd_str (jcb, JSC_RDL, (const char *)rdlstr);
    if (jsc_update_jcb (h, job->lwj_id, JSC_RDL, jcb) != 0) {
        flux_log (h, LOG_ERR, "error jsc udpate: %ld (%s)", job->lwj_id, 
                  strerror (errno));
        goto done;
    }
    jr = rdl_resource_get (job->rdl, ctx->rctx.root_uri);
    if (build_contain_req (h, job->lwj_id, jr, arr) != 0) {
        flux_log (h, LOG_ERR, "error requesting containment for job"); 
        goto done;
    }
    json_object_object_add (jcb, JSC_RDL_ALLOC, arr);
    if (jsc_update_jcb (h, job->lwj_id, JSC_RDL_ALLOC, jcb) != 0) {
        flux_log (h, LOG_ERR, "error updating jcb"); 
        goto done;
    }
    Jput (arr);
    if ((update_state (h, job->lwj_id, job->state, J_ALLOCATED)) != 0) {
        flux_log (h, LOG_ERR, "failed to update the state of job %ld",
                  job->lwj_id);
        goto done; ;
    }

done:
    if (jcb) 
        Jput (jcb);
    if (rdlstr) 
        free (rdlstr);
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
    if ((update_state (h, job->lwj_id, job->state, J_RUNREQUEST)) != 0) {
        flux_log (h, LOG_ERR, "failed to update the state of job %ld",
                  job->lwj_id);
        return -1;
    }
    if (flux_event_send (h, NULL, "wrexec.run.%ld", job->lwj_id) < 0) {
        flux_log (h, LOG_ERR, "request_run event send failed: %s",
                  strerror (errno));
        return -1;
    }
    flux_log (h, LOG_DEBUG, "job %ld runrequest", job->lwj_id);
    return 0;
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
 ********************************************************************************/

static int inline release_resources (ssrvctx_t *ctx, flux_lwj_t *job)
{
    return ctx->sops.release_resources (ctx->h, ctx->rctx.root_rdl, 
                                 ctx->rctx.root_uri, job);
}

/*
 * schedule_job() searches through all of the idle resources to
 * satisfy a job's requirements.  If enough resources are found, it
 * proceeds to allocate those resources and update the kvs's lwj entry
 * in preparation for job execution.
 */
static int schedule_job (ssrvctx_t *ctx, flux_lwj_t *job)
{
    int rc = -1;
    flux_t h = ctx->h;
    bool reserve = false;
    struct rdl *rrdl = ctx->rctx.root_rdl;
    struct rdl *frdl = NULL;            /* found rdl */
    struct resource *fr = NULL;         /* found resource */
    const char *ruri = ctx->rctx.root_uri;

    if (!(frdl = ctx->sops.find_resources (h, rrdl,ruri,job,&reserve))) { 
        flux_log (h, LOG_ERR, "couldn't find resources for job %ld",
            job->lwj_id);
        goto done;
    }
    if (!(fr = rdl_resource_get (frdl, ruri))) {
        flux_log (h, LOG_ERR, "failed to get found resource for job %ld", 
        job->lwj_id);
        goto done;
    }
    if ((ctx->sops.select_resources (h, rrdl, ruri, fr, job, reserve)) != 0) {
        flux_log (h, LOG_ERR, "failed to select resources for job %ld",
            job->lwj_id);
        /* Scheduler specific job transition 
         * TODO: We will deal with revervation associated with backfill 
         * later.
         */
        job->state = J_PENDING;
        goto done;
    }

    /* Scheduler specific job transition */
    job->state = J_SELECTED;
    if (req_tpexec_allocate (ctx, job) != 0) {
        flux_log (h, LOG_ERR, "failed to request allocate for job %ld",
            job->lwj_id);
        goto done;
    }
    rdl_destroy (frdl);
    rc = 0;

done:
    return rc;
}

static int schedule_jobs (ssrvctx_t *ctx)
{
    int rc = 0;
    flux_lwj_t *job = NULL;
    /* TODO: 1. we might need to invoke prioritize_qeueu here 
       or making this as a continous operation by another thread 
       or comms module.
       2. when dynamic scheduling is supported, the loop
       should traverse through running job queue as well. 
     */
    zlist_t *jobs = ctx->p_queue;
    job = zlist_first (jobs);
    while (!rc && job) {
        if (job->state == J_SCHEDREQ) {
            rc = schedule_job (ctx, job);
        }
        job = zlist_next (jobs);
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

    flux_log (h, LOG_DEBUG, "attempting job %ld state change from %s to %s",
              job->lwj_id, jsc_job_num2state (oldstate),
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
                    || trans (J_CANCELLED, newstate, &(job->state)))
            q_move_to_cqueue (ctx, job); 
            if ((release_resources (ctx, job) == 0))  
                flux_event_send (h, NULL, "sched.res.event");
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
    flux_log (h, LOG_ERR, "job %ld bad state transition from %s to %s",
              job->lwj_id, jsc_job_num2state (oldstate),
              jsc_job_num2state (newstate));
    return -1;
}

/* TODO: we probably need to abstract out resource status & control  API 
 * For now, the only resource event is raised when a job releases its 
 * RDL allocation.   
 */ 
static int res_event_cb (flux_t h, int t, zmsg_t **zmsg, void *arg)
{
    schedule_jobs (getctx ((flux_t)arg));
    return 0;
}

#if ENABLE_TIMER_EVENT
static int timer_event_cb (flux_t h, void *arg)
{
    //flux_log (h, LOG_ERR, "TIMER CALLED");
    schedule_jobs (getctx ((flux_t)arg));
    return 0;
}
#endif 

static int job_status_cb (JSON jcb, void *arg, int errnum)
{
    ssrvctx_t *ctx = getctx ((flux_t)arg);
    flux_lwj_t *j = NULL;
    job_state_t ns = J_FOR_RENT;

    if (errnum > 0) {
        flux_log (ctx->h, LOG_ERR, "job_status_cb: errnum passed in");
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

int mod_main (flux_t h, zhash_t *args)
{
    int rc = -1;
    ssrvctx_t *ctx = NULL;
    char *schedplugin = "sched.plugin1";

    if (!(ctx = getctx (h))) {
        flux_log (h, LOG_ERR, "can't find or allocate the context");
        goto done;
    }
    if (flux_rank (h) != 0) {
        flux_log (h, LOG_ERR, "sched module must only run on rank 0");
        goto done;
    }
    flux_log (h, LOG_INFO, "sched comms module starting");
    if (load_sched_plugin (ctx, schedplugin) != 0) {
        flux_log (h, LOG_ERR, "failed to load scheduler plugin");
        goto done;
    }
    flux_log (h, LOG_INFO, "scheduler plugin loaded");
    if (load_rdl (ctx, args) != 0) {
        flux_log (h, LOG_ERR, "failed to setup and load RDL");
        goto done;
    }
    flux_log (h, LOG_INFO, "scheduler plugin loaded");
    if (reg_events (ctx) != 0) {
        flux_log (h, LOG_ERR, "failed to reg events");
        goto done;
    }
    flux_log (h, LOG_INFO, "events registered");
    if (flux_reactor_start (h) < 0) {
        flux_log (h, LOG_ERR, "flux_reactor_start: %s", strerror (errno));
        rc =  -1;
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

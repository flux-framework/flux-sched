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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <json.h>
#include <time.h>
#include <inttypes.h>

#include "src/common/libutil/jsonutil.h"
#include "src/common/libutil/log.h"
#include "src/common/libutil/shortjson.h"
#include "src/common/libutil/xzmalloc.h"
#include "rdl.h"
#include "scheduler.h"

const char *IDLETAG = "idle";
const char *CORETYPE = "core";
static const char *module_name = "sim_sched";

static struct stab_struct jobstate_tab[] = {
    {j_null, "null"},
    {j_reserved, "reserved"},
    {j_submitted, "submitted"},
    {j_unsched, "unsched"},
    {j_pending, "pending"},
    {j_runrequest, "runrequest"},
    {j_allocated, "allocated"},
    {j_starting, "starting"},
    {j_running, "running"},
    {j_cancelled, "cancelled"},
    {j_complete, "complete"},
    {j_reaped, "reaped"},
    {-1, NULL},
};

int send_rdl_update (flux_t h, struct rdl *rdl)
{
    int rc = 0;
    flux_msg_t *msg = NULL;
    JSON o = Jnew ();

    Jadd_int64 (o, "rdl_int", (int64_t)rdl);

    if (!(msg = flux_event_encode ("rdl.update", Jtostr (o)))
        || flux_send (h, msg, 0) < 0) {
        rc = -1;
    }

    Jput (o);
    flux_msg_destroy (msg);
    return rc;
}

struct rdl *get_free_subset (struct rdl *rdl, const char *type)
{
    JSON tags = Jnew ();
    Jadd_bool (tags, IDLETAG, true);
    JSON args = Jnew ();
    Jadd_obj (args, "tags", tags);
    Jadd_str (args, "type", type);
    struct rdl *frdl = rdl_find (rdl, args);
    Jput (args);
    Jput (tags);
    return frdl;
}

static int64_t count_free (struct resource *r, const char *type)
{
    int64_t curr_count = 0;
    JSON o = NULL;
    const char *curr_type = NULL;
    struct resource *child = NULL;

    if (r) {
        rdl_resource_iterator_reset (r);
        while ((child = rdl_resource_next_child (r))) {
            curr_count += count_free (child, type);
            rdl_resource_destroy (child);
        }
        rdl_resource_iterator_reset (r);

        o = rdl_resource_json (r);
        Jget_str (o, "type", &curr_type);
        if (!strcmp (type, curr_type)) {
            curr_count++;
        }
        Jput (o);
    }

    return curr_count;
}

int64_t get_free_count (struct rdl *rdl, const char *uri, const char *type)
{
    struct resource *fr = NULL;
    int64_t count = 0;

    if ((fr = rdl_resource_get (rdl, uri)) == NULL) {
        return -1;
    }
    count = count_free (fr, type);
    rdl_resource_destroy (fr);

    return count;
}

static void remove_job_resources_helper (struct rdl *rdl,
                                         const char *uri,
                                         struct resource *curr_res)
{
    struct resource *rdl_res = NULL, *child_res = NULL;
    char *res_path = NULL;
    const char *type = NULL, *child_name = NULL;
    JSON o = NULL;

    rdl_resource_iterator_reset (curr_res);
    while ((child_res = rdl_resource_next_child (curr_res))) {
        o = rdl_resource_json (child_res);
        Jget_str (o, "type", &type);
        // Check if child matches, if so, unlink from hierarchy
        if (strcmp (type, CORETYPE) == 0) {
            res_path = xasprintf ("%s:%s", uri, rdl_resource_path (curr_res));
            rdl_res = rdl_resource_get (rdl, res_path);
            child_name = rdl_resource_name (child_res);
            rdl_resource_unlink_child (rdl_res, child_name);
            free (res_path);
        } else {  // Else, recursive call on child
            remove_job_resources_helper (rdl, uri, child_res);
        }
        Jput (o);
        rdl_resource_destroy (child_res);
    }
    rdl_resource_iterator_reset (curr_res);
}

void remove_job_resources_from_rdl (struct rdl *rdl,
                                    const char *uri,
                                    flux_lwj_t *job)
{
    struct resource *job_rdl_root = NULL;

    job_rdl_root = rdl_resource_get (job->rdl, uri);
    remove_job_resources_helper (rdl, uri, job_rdl_root);
    rdl_resource_destroy (job_rdl_root);
}

void trigger_cb (flux_t h,
                 flux_msg_handler_t *w,
                 const flux_msg_t *msg,
                 void *arg)
{
    clock_t start, diff;
    double seconds;
    bool sched_loop;
    const char *json_str = NULL;
    JSON o = NULL;
    sim_state_t *sim_state = NULL;
    ctx_t *ctx = getctx (h);

    if (flux_request_decode (msg, NULL, &json_str) < 0 || json_str == NULL
        || !(o = Jfromstr (json_str))) {
        flux_log (h, LOG_ERR, "%s: bad message", __FUNCTION__);
        Jput (o);
        return;
    }

    flux_log (h, LOG_DEBUG, "Setting sim_state to new values");

    sim_state = json_to_sim_state (o);

    start = clock ();

    handle_kvs_queue (ctx);
    handle_event_queue (ctx);

    if ((sched_loop =
             should_run_schedule_loop (ctx, (int)sim_state->sim_time))) {
        flux_log (h, LOG_DEBUG, "Running the schedule loop");
        if (schedule_jobs (ctx, sim_state->sim_time) > 0) {
            queue_timer_change (ctx, module_name);
        }
        end_schedule_loop (ctx);
    }

    diff = clock () - start;
    seconds = ((double)diff) / CLOCKS_PER_SEC;
    sim_state->sim_time += seconds;
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

    handle_timer_queue (ctx, sim_state);

    send_rdl_update (h, ctx->rdl);
    send_reply_request (h, module_name, sim_state);

    free_simstate (sim_state);
    Jput (o);
}

void handle_kvs_queue (ctx_t *ctx)
{
    kvs_event_t *kvs_event = NULL;
    // zlist_sort (ctx->kvs_queue, compare_kvs_events);
    while (zlist_size (ctx->kvs_queue) > 0) {
        kvs_event = (kvs_event_t *)zlist_pop (ctx->kvs_queue);
        flux_log (ctx->h,
                  LOG_DEBUG,
                  "Event to be handled - key: %s, val: %s",
                  kvs_event->key,
                  kvs_event->val);
        lwjstate_cb (kvs_event->key, kvs_event->val, ctx->h, kvs_event->errnum);
        free (kvs_event->key);
        free (kvs_event->val);
        free (kvs_event);
    }
}

int queue_kvs_cb (const char *key, const char *val, void *arg, int errnum)
{
    int key_len;
    int val_len;
    char *key_copy = NULL;
    char *val_copy = NULL;
    kvs_event_t *kvs_event = NULL;
    flux_t h = arg;
    ctx_t *ctx = getctx (h);

    if (key != NULL) {
        key_len = strlen (key) + 1;
        key_copy = (char *)malloc (sizeof (char) * key_len);
        strncpy (key_copy, key, key_len);
    }
    if (val != NULL) {
        val_len = strlen (val) + 1;
        val_copy = (char *)malloc (sizeof (char) * val_len);
        strncpy (val_copy, val, val_len);
    }

    kvs_event = (kvs_event_t *)malloc (sizeof (kvs_event_t));
    kvs_event->errnum = errnum;
    kvs_event->key = key_copy;
    kvs_event->val = val_copy;
    flux_log (h,
              LOG_DEBUG,
              "Event queued - key: %s, val: %s",
              kvs_event->key,
              kvs_event->val);
    zlist_append (ctx->kvs_queue, kvs_event);

    return 0;
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

static inline bool is_newjob (JSON jcb)
{
    int64_t os, ns;
    get_states (jcb, &os, &ns);
    return ((os == J_NULL) && (ns == J_NULL))? true : false;
}

static int append_to_pqueue (ctx_t *ctx, JSON jcb)
{
    int rc = -1;
    int64_t jid = -1;;
    flux_lwj_t *job = NULL;

    get_jobid (jcb, &jid);
    if ( !(job = (flux_lwj_t *) xzmalloc (sizeof (*job))))
        oom ();

    job->lwj_id = jid;
    job->state = (lwj_state_e) J_NULL;
    if (zlist_append (ctx->p_queue, job) != 0) {
        flux_log (ctx->h, LOG_ERR, "failed to append to pending job queue.");
        goto done;
    }
    rc = 0;
done:
    return rc;
}

static int job_status_cb (const char *jcbstr, void *arg, int errnum)
{
    JSON jcb = NULL;
    ctx_t *ctx = getctx ((flux_t)arg);
    flux_lwj_t *j = NULL;
    job_state_t ns = J_FOR_RENT;
    flux_event_t e;

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
    Jput (jcb);

    e.t = lwj_event;
    e.ev.je = (lwj_event_e) ns;
    e.lwj = j;
    return action_j_event (ctx, &e);
}

int wait_for_lwj_init (flux_t h)
{
    int rc = 0;
    kvsdir_t *dir = NULL;

    if (kvs_watch_once_dir (h, &dir, "lwj") < 0) {
        flux_log (h, LOG_ERR, "wait_for_lwj_init: %s", strerror (errno));
        rc = -1;
        goto ret;
    }

    flux_log (h, LOG_DEBUG, "wait_for_lwj_init %s", kvsdir_key (dir));

ret:
    if (dir)
        kvsdir_destroy (dir);
    return rc;
}

void freectx (void *arg)
{
    ctx_t *ctx = arg;
    zlist_destroy (&ctx->p_queue);
    zlist_destroy (&ctx->r_queue);
    zlist_destroy (&ctx->c_queue);
    zlist_destroy (&ctx->ev_queue);
    zlist_destroy (&ctx->kvs_queue);
    zlist_destroy (&ctx->timer_queue);
    free (ctx);
}

ctx_t *getctx (flux_t h)
{
    ctx_t *ctx = (ctx_t *)flux_aux_get (h, "sim_sched");

    if (!ctx) {
        ctx = malloc (sizeof (*ctx));
        ctx->h = h;
        ctx->p_queue = zlist_new ();
        ctx->r_queue = zlist_new ();
        ctx->c_queue = zlist_new ();
        ctx->ev_queue = zlist_new ();
        ctx->kvs_queue = zlist_new ();
        ctx->timer_queue = zlist_new ();
        ctx->in_sim = false;
        ctx->run_schedule_loop = false;
        ctx->uri = "default";
        ctx->rdl = NULL;
        flux_aux_set (h, "sim_sched", ctx, freectx);
    }

    return ctx;
}

int unwatch_lwj (ctx_t *ctx, flux_lwj_t *job)
{
    char *key = NULL;
    int rc = 0;

    if (!(key = xasprintf ("lwj.%"PRId64".state", job->lwj_id))) {
        flux_log (ctx->h, LOG_ERR, "update_job_state key create failed");
        rc = -1;
    } else if (kvs_unwatch (ctx->h, key)) {
        flux_log (ctx->h, LOG_ERR, "failed to unwatch %s", key);
        rc = -1;
    } else {
        flux_log (ctx->h, LOG_DEBUG, "unwatched %s", key);
    }
    free (key);

    key = xasprintf ("lwj.%"PRId64"", job->lwj_id);
    // dump_kvs_dir (ctx->h, key);
    free (key);

    return rc;
}

flux_lwj_t *find_lwj (ctx_t *ctx, int64_t id)
{
    flux_lwj_t *j = NULL;

    j = zlist_first (ctx->p_queue);
    while (j) {
        if (j->lwj_id == id)
            break;
        j = zlist_next (ctx->p_queue);
    }
    if (j)
        return j;

    j = zlist_first (ctx->r_queue);
    while (j) {
        if (j->lwj_id == id)
            break;
        j = zlist_next (ctx->r_queue);
    }

    return j;
}

void issue_lwj_event (ctx_t *ctx, lwj_event_e e, flux_lwj_t *j)
{
    flux_event_t *ev = (flux_event_t *)xzmalloc (sizeof (flux_event_t));
    ev->t = lwj_event;
    ev->ev.je = e;
    ev->lwj = j;

    if (zlist_append (ctx->ev_queue, ev) == -1) {
        flux_log (ctx->h, LOG_ERR, "enqueuing an event failed");
        goto ret;
    }
    if (signal_event (ctx) == -1) {
        flux_log (ctx->h, LOG_ERR, "signaling an event failed");
        goto ret;
    }

ret:
    return;
}

void lwjstate_cb (const char *key, const char *val, void *arg, int errnum)
{
    int64_t lwj_id;
    flux_lwj_t *j = NULL;
    lwj_event_e e;
    flux_t h = arg;
    ctx_t *ctx = getctx (h);

    if (errnum > 0) {
        /* Ignore ENOENT.  It is expected when this cb is called right
         * after registration.
         */
        if (errnum != ENOENT) {
            flux_log (h,
                      LOG_ERR,
                      "lwjstate_cb key(%s), val(%s): %s",
                      key,
                      val,
                      strerror (errnum));
        }
        goto ret;
    }

    if (extract_lwjid (key, &lwj_id) == -1) {
        flux_log (h, LOG_ERR, "ill-formed key: %s", key);
        goto ret;
    }
    flux_log (h, LOG_DEBUG, "lwjstate_cb: %"PRId64", %s", lwj_id, val);

    j = find_lwj (ctx, lwj_id);
    if (j) {
        e = stab_lookup (jobstate_tab, val);
        issue_lwj_event (ctx, e, j);
    } else
        flux_log (h, LOG_ERR, "lwjstate_cb: find_lwj %"PRId64" failed", lwj_id);

ret:
    return;
}

int signal_event (ctx_t *ctx)
{
    int rc = 0;
    flux_msg_t *msg = NULL;

    if (ctx->in_sim) {
        queue_timer_change (ctx, module_name);
        goto ret;
    } else if (!(msg = flux_event_encode ("sim_sched.event", NULL))
               || flux_send (ctx->h, msg, 0) < 0) {
        flux_log (ctx->h, LOG_ERR, "flux_send: %s", strerror (errno));
        rc = -1;
    }

    flux_msg_destroy (msg);
ret:
    return rc;
}

/* The val argument is for the *next* job id.  Hence, the job id of
 * the new job will be (val - 1).
 */
#if 0
static int
event_cb (flux_t h, int typemask, zmsg_t **zmsg, void *arg)
{
    flux_event_t *e = NULL;

    while ( (e = zlist_pop (ev_queue)) != NULL) {
        action (e);
        free (e);
    }

    zmsg_destroy (zmsg);

    return 0;
}
#endif

int extract_lwjid (const char *k, int64_t *i)
{
    int rc = 0;
    char *kcopy = NULL;
    char *lwj = NULL;
    char *id = NULL;

    if (!k) {
        rc = -1;
        goto ret;
    }

    kcopy = strdup (k);
    lwj = strtok (kcopy, ".");
    if (strncmp (lwj, "lwj", 3) != 0) {
        rc = -1;
        goto ret;
    }
    id = strtok (NULL, ".");
    *i = strtoul (id, (char **)NULL, 10);

ret:
    return rc;
}

int extract_lwjinfo (ctx_t *ctx, flux_lwj_t *j)
{
    char *key = NULL;
    char *state;
    int64_t reqnodes = 0;
    int64_t reqtasks = 0;
    int64_t io_rate = 0;
    int rc = -1;

    if (!(key = xasprintf ("lwj.%"PRId64".state", j->lwj_id))) {
        flux_log (ctx->h, LOG_ERR, "extract_lwjinfo state key create failed");
        goto ret;
    } else if (kvs_get_string (ctx->h, key, &state) < 0) {
        flux_log (
            ctx->h, LOG_ERR, "extract_lwjinfo %s: %s", key, strerror (errno));
        goto ret;
    } else {
        j->state = stab_lookup (jobstate_tab, state);
        flux_log (ctx->h, LOG_DEBUG, "extract_lwjinfo got %s: %s", key, state);
        free (key);
    }

    if (!(key = xasprintf ("lwj.%"PRId64".nnodes", j->lwj_id))) {
        flux_log (ctx->h, LOG_ERR, "extract_lwjinfo nnodes key create failed");
        goto ret;
    } else if (kvs_get_int64 (ctx->h, key, &reqnodes) < 0) {
        flux_log (ctx->h,
                  LOG_ERR,
                  "extract_lwjinfo get %s: %s",
                  key,
                  strerror (errno));
        goto ret;
    } else {
        j->req.nnodes = reqnodes;
        flux_log (
            ctx->h, LOG_DEBUG, "extract_lwjinfo got %s: %"PRId64"", key, reqnodes);
        free (key);
    }

    if (!(key = xasprintf ("lwj.%"PRId64".ntasks", j->lwj_id))) {
        flux_log (ctx->h, LOG_ERR, "extract_lwjinfo ntasks key create failed");
        goto ret;
    } else if (kvs_get_int64 (ctx->h, key, &reqtasks) < 0) {
        flux_log (ctx->h,
                  LOG_ERR,
                  "extract_lwjinfo get %s: %s",
                  key,
                  strerror (errno));
        goto ret;
    } else {
        /* Assuming a 1:1 relationship right now between cores and tasks */
        j->req.ncores = reqtasks;
        flux_log (
            ctx->h, LOG_DEBUG, "extract_lwjinfo got %s: %"PRId64"", key, reqtasks);
        free (key);
        j->alloc.nnodes = 0;
        j->alloc.ncores = 0;
        j->rdl = NULL;
        rc = 0;
    }

    if (!(key = xasprintf ("lwj.%"PRId64".io_rate", j->lwj_id))) {
        flux_log (ctx->h, LOG_ERR, "extract_lwjinfo io_rate key create failed");
        goto ret;
    } else if (kvs_get_int64 (ctx->h, key, &io_rate) < 0) {
        flux_log (ctx->h,
                  LOG_ERR,
                  "extract_lwjinfo get %s: %s",
                  key,
                  strerror (errno));
        goto ret;
    } else {
        j->req.io_rate = io_rate;
        j->alloc.io_rate = -1;  // currently not used
        flux_log (
            ctx->h, LOG_DEBUG, "extract_lwjinfo got %s: %"PRId64"", key, io_rate);
        free (key);
    }

ret:
    return rc;
}

void queue_timer_change (ctx_t *ctx, const char *module)
{
    zlist_append (ctx->timer_queue, (void *)module);
}

void handle_timer_queue (ctx_t *ctx, sim_state_t *sim_state)
{
    while (zlist_size (ctx->timer_queue) > 0)
        set_next_event (zlist_pop (ctx->timer_queue), sim_state);

    // Set scheduler loop to run in next occuring scheduler block
    double *this_timer = zhash_lookup (sim_state->timers, module_name);
    double next_schedule_block =
        sim_state->sim_time
        + (SCHED_INTERVAL - ((int)sim_state->sim_time % SCHED_INTERVAL));
    if (ctx->run_schedule_loop
        && (next_schedule_block < *this_timer || *this_timer < 0)) {
        *this_timer = next_schedule_block;
    }
    flux_log (ctx->h,
              LOG_DEBUG,
              "run_sched_loop: %d, next_schedule_block: %f, this_timer: %f",
              ctx->run_schedule_loop,
              next_schedule_block,
              *this_timer);
}

// Set the timer for "module" to happen relatively soon
// If the mod is sim_exec, it shouldn't happen immediately
// because the scheduler still needs to transition through
// 3->4 states before the sim_exec module can actually "exec" a job
void set_next_event (const char *module, sim_state_t *sim_state)
{
    double next_event;
    double *timer = zhash_lookup (sim_state->timers, module);
    next_event =
        sim_state->sim_time + ((!strcmp (module, "sim_exec")) ? .0001 : .00001);
    if (*timer > next_event || *timer < 0) {
        *timer = next_event;
    }
}

void handle_event_queue (ctx_t *ctx)
{
    flux_event_t *e = NULL;
    bool resources_released = false;

    flux_log (ctx->h,
              LOG_DEBUG,
              "handling %d queued events",
              (int)zlist_size (ctx->ev_queue));
    for (e = (flux_event_t *)zlist_pop (ctx->ev_queue); e != NULL;
         e = (flux_event_t *)zlist_pop (ctx->ev_queue)) {
        if (e->t == res_event) {
            resources_released = !action (ctx, e) || resources_released;
        } else {
            action (ctx, e);
        }
        free (e);
    }

    if (resources_released) {
        queue_schedule_loop (ctx);
    }
}

int issue_res_event (ctx_t *ctx, flux_lwj_t *lwj)
{
    int rc = 0;
    flux_event_t *newev = (flux_event_t *)xzmalloc (sizeof (flux_event_t));

    // TODO: how to update the status of each entry as "free"
    // then destroy zlist_t without having to destroy
    // the elements
    // release lwj->resource

    newev->t = res_event;
    newev->ev.re = r_released;
    newev->lwj = lwj;

    if (zlist_append (ctx->ev_queue, newev) == -1) {
        flux_log (ctx->h, LOG_ERR, "enqueuing an event failed");
        rc = -1;
        goto ret;
    }
    if (signal_event (ctx) == -1) {
        flux_log (ctx->h, LOG_ERR, "signal the event-enqueued event ");
        rc = -1;
        goto ret;
    }

ret:
    return rc;
}

int move_to_r_queue (ctx_t *ctx, flux_lwj_t *lwj)
{
    zlist_remove (ctx->p_queue, lwj);
    return zlist_append (ctx->r_queue, lwj);
}

int move_to_c_queue (ctx_t *ctx, flux_lwj_t *lwj)
{
    zlist_remove (ctx->r_queue, lwj);
    return zlist_append (ctx->c_queue, lwj);
}

int action_j_event (ctx_t *ctx, flux_event_t *e)
{
    /* e->lwj->state is the current state
     * e->ev.je      is the new state
     */
    /*
        flux_log (ctx->h, LOG_DEBUG, "attempting job %"PRId64" state change from %s to
       %s",
              e->lwj->lwj_id, stab_rlookup (jobstate_tab, e->lwj->state),
                              stab_rlookup (jobstate_tab, e->ev.je));
        */
    switch (e->lwj->state) {
        case j_null:
            if (e->ev.je != j_reserved) {
                goto bad_transition;
            }
            e->lwj->state = j_reserved;
            break;

        case j_reserved:
            if (e->ev.je != j_submitted) {
                goto bad_transition;
            }
            extract_lwjinfo (ctx, e->lwj);
            if (e->lwj->state != j_submitted) {
                flux_log (ctx->h,
                          LOG_ERR,
                          "job %"PRId64" read state mismatch ",
                          e->lwj->lwj_id);
                goto bad_transition;
            }
            e->lwj->state = j_unsched;
            queue_schedule_loop (ctx);
            break;

        case j_submitted:
            if (e->ev.je != j_allocated) {
                goto bad_transition;
            }
            e->lwj->state = j_allocated;
            request_run (ctx, e->lwj);
            break;

        case j_unsched:
            /* TODO */
            goto bad_transition;
            break;

        case j_pending:
            /* TODO */
            goto bad_transition;
            break;

        case j_allocated:
            if (e->ev.je != j_runrequest) {
                goto bad_transition;
            }
            e->lwj->state = j_runrequest;
            if (ctx->in_sim)
                queue_timer_change (ctx, "sim_exec");
            break;

        case j_runrequest:
            if (e->ev.je != j_starting) {
                goto bad_transition;
            }
            e->lwj->state = j_starting;
            break;

        case j_starting:
            if (e->ev.je != j_running) {
                goto bad_transition;
            }
            e->lwj->state = j_running;
            move_to_r_queue (ctx, e->lwj);
            break;

        case j_running:
            if (e->ev.je != j_complete) {
                goto bad_transition;
            }
            /* TODO move this to j_complete case once reaped is implemented */
            move_to_c_queue (ctx, e->lwj);
            unwatch_lwj (ctx, e->lwj);
            issue_res_event (ctx, e->lwj);
            break;

        case j_cancelled:
            /* TODO */
            goto bad_transition;
            break;

        case j_complete:
            if (e->ev.je != j_reaped) {
                goto bad_transition;
            }
            //        move_to_c_queue (e->lwj);
            break;

        case j_reaped:
            if (e->ev.je != j_complete) {
                goto bad_transition;
            }
            e->lwj->state = j_reaped;
            break;

        default:
            flux_log (ctx->h,
                      LOG_ERR,
                      "job %"PRId64" unknown state %d",
                      e->lwj->lwj_id,
                      e->lwj->state);
            break;
    }

    return 0;

bad_transition:
    flux_log (ctx->h,
              LOG_ERR,
              "job %"PRId64" bad state transition from %s to %s",
              e->lwj->lwj_id,
              stab_rlookup (jobstate_tab, e->lwj->state),
              stab_rlookup (jobstate_tab, e->ev.je));
    return -1;
}

int action_r_event (ctx_t *ctx, flux_event_t *e)
{
    int rc = -1;

    if ((e->ev.re == r_released) || (e->ev.re == r_attempt)) {
        release_resources (ctx, ctx->rdl, ctx->uri, e->lwj);
        rc = 0;
    }

    return rc;
}

int action (ctx_t *ctx, flux_event_t *e)
{
    int rc = 0;

    switch (e->t) {
        case lwj_event:
            rc = action_j_event (ctx, e);
            break;

        case res_event:
            rc = action_r_event (ctx, e);
            break;

        default:
            flux_log (ctx->h, LOG_ERR, "unknown event type");
            break;
    }

    return rc;
}

int request_run (ctx_t *ctx, flux_lwj_t *job)
{
    int rc = -1;
    char *topic = NULL;
    flux_msg_t *msg = NULL;

    if (update_job_state (ctx, job, j_runrequest) < 0) {
        flux_log (ctx->h,
                  LOG_ERR,
                  "request_run failed to update job %"PRId64" to %s",
                  job->lwj_id,
                  stab_rlookup (jobstate_tab, j_runrequest));
        goto done;
    }
    if (kvs_commit (ctx->h) < 0) {
        flux_log (ctx->h, LOG_ERR, "kvs_commit error!");
        goto done;
    }
    if (!ctx->in_sim) {
        topic = xasprintf ("rexec.run.%"PRId64"", job->lwj_id);
        if (!(msg = flux_event_encode (topic, NULL))
            || flux_send (ctx->h, msg, 0) < 0) {
            flux_log (ctx->h,
                      LOG_ERR,
                      "request_run event send failed: %s",
                      strerror (errno));
            goto done;
        }
    } else {
        topic = xasprintf ("sim_exec.run.%"PRId64"", job->lwj_id);
        msg = flux_msg_create (FLUX_MSGTYPE_REQUEST);
        flux_msg_set_topic (msg, topic);
        if (flux_send (ctx->h, msg, 0) < 0) {
            flux_log (ctx->h,
                      LOG_ERR,
                      "request_run request send failed: %s",
                      strerror (errno));
            goto done;
        }
    }
    flux_log (ctx->h, LOG_DEBUG, "job %"PRId64" runrequest", job->lwj_id);
    rc = 0;
done:
    if (topic)
        free (topic);

    flux_msg_destroy (msg);
    return rc;
}

char *ctime_iso8601_now (char *buf, size_t sz)
{
    struct tm tm;
    time_t now = time (NULL);

    memset (buf, 0, sz);

    if (!localtime_r (&now, &tm))
        err_exit ("localtime");
    strftime (buf, sz, "%FT%T", &tm);

    return buf;
}

int stab_lookup (struct stab_struct *ss, const char *s)
{
    while (ss->s != NULL) {
        if (!strcmp (ss->s, s))
            return ss->i;
        ss++;
    }
    return -1;
}

const char *stab_rlookup (struct stab_struct *ss, int i)
{
    while (ss->s != NULL) {
        if (ss->i == i)
            return ss->s;
        ss++;
    }
    return "unknown";
}

/*
 * Add the allocated resources to the job, and
 * change its state to "allocated".
 */
int update_job (ctx_t *ctx, flux_lwj_t *job)
{
    flux_log (ctx->h, LOG_DEBUG, "updating job %"PRId64"", job->lwj_id);
    int rc = -1;

    if (update_job_state (ctx, job, j_allocated)) {
        flux_log (ctx->h,
                  LOG_ERR,
                  "update_job failed to update job %"PRId64" to %s",
                  job->lwj_id,
                  stab_rlookup (jobstate_tab, j_allocated));
    } else if (update_job_resources (ctx, job)) {
        kvs_commit (ctx->h);
        flux_log (ctx->h,
                  LOG_ERR,
                  "update_job %"PRId64" resrc update failed",
                  job->lwj_id);
    } else if (kvs_commit (ctx->h) < 0) {
        flux_log (ctx->h, LOG_ERR, "kvs_commit error!");
    } else {
        rc = 0;
    }

    flux_log (ctx->h, LOG_DEBUG, "updated job %"PRId64"", job->lwj_id);
    return rc;
}

/*
 * Update the job's kvs entry for state and mark the time.
 * Intended to be part of a series of changes, so the caller must
 * invoke the kvs_commit at some future point.
 */
int update_job_state (ctx_t *ctx, flux_lwj_t *job, lwj_event_e e)
{
    char buf[64];
    char *key = NULL;
    char *key2 = NULL;
    int rc = -1;
    const char *state;

    ctime_iso8601_now (buf, sizeof (buf));

    state = stab_rlookup (jobstate_tab, e);
    if (!strcmp (state, "unknown")) {
        flux_log (ctx->h, LOG_ERR, "unknown job state %d", e);
    } else if (!(key = xasprintf ("lwj.%"PRId64".state", job->lwj_id))) {
        flux_log (ctx->h, LOG_ERR, "update_job_state key create failed");
    } else if (kvs_put_string (ctx->h, key, state) < 0) {
        flux_log (ctx->h,
                  LOG_ERR,
                  "update_job_state %"PRId64" state update failed: %s",
                  job->lwj_id,
                  strerror (errno));
    } else if (!(key2 = xasprintf ("lwj.%"PRId64".%s-time", job->lwj_id, state))) {
        flux_log (ctx->h, LOG_ERR, "update_job_state key2 create failed");
    } else if (kvs_put_string (ctx->h, key2, buf) < 0) {
        flux_log (ctx->h,
                  LOG_ERR,
                  "update_job_state %"PRId64" %s-time failed: %s",
                  job->lwj_id,
                  state,
                  strerror (errno));
    } else {
        rc = 0;
        flux_log (ctx->h,
                  LOG_DEBUG,
                  "updated job %"PRId64"'s state in the kvs to %s",
                  job->lwj_id,
                  state);
    }

    free (key);
    free (key2);

    return rc;
}

/*
 * Create lwj entries to tell wrexecd how many tasks to launch per
 * node.  The key has the form: lwj.<jobID>.rank.<nodeID>.cores The
 * value will be the number of tasks to launch on that node.
 */
int update_job_resources (ctx_t *ctx, flux_lwj_t *job)
{
    printf ("%s\n", ctx->uri);
    uint64_t node = 0;
    uint32_t cores = 0;
    struct resource *jr = rdl_resource_get (job->rdl, ctx->uri);
    int rc = -1;

    if (jr)
        rc = update_job_cores (ctx, jr, job, &node, &cores);
    else
        flux_log (ctx->h, LOG_ERR, "%s received a null resource", __FUNCTION__);

    return rc;
}

/*
 * Recursively search the resource r and update this job's lwj key
 * with the core count per rank (i.e., node for the time being)
 */
int update_job_cores (ctx_t *ctx,
                      struct resource *jr,
                      flux_lwj_t *job,
                      uint64_t *pnode,
                      uint32_t *pcores)
{
    bool imanode = false;
    char *key = NULL;
    char *lwjtag = NULL;
    const char *type = NULL;
    json_object *o = NULL;
    json_object *o2 = NULL;
    json_object *o3 = NULL;
    struct resource *c;
    int rc = 0;

    if (jr) {
        o = rdl_resource_json (jr);
        if (o) {
            // flux_log (ctx->h, LOG_DEBUG, "updating: %s",
            // json_object_to_json_string (o));
        } else {
            flux_log (ctx->h, LOG_ERR, "%s invalid resource", __FUNCTION__);
            rc = -1;
            goto ret;
        }
    } else {
        flux_log (ctx->h, LOG_ERR, "%s received a null resource", __FUNCTION__);
        rc = -1;
        goto ret;
    }

    Jget_str (o, "type", &type);
    if (strcmp (type, "node") == 0) {
        *pcores = 0;
        imanode = true;
    } else if (strcmp (type, CORETYPE) == 0) {
        /* we need to limit our allocation to just the tagged cores */
        lwjtag = xasprintf ("lwj.%"PRId64"", job->lwj_id);
        Jget_obj (o, "tags", &o2);
        Jget_obj (o2, lwjtag, &o3);
        if (o3) {
            (*pcores)++;
        }
        free (lwjtag);
    }
    json_object_put (o);

    while ((rc == 0) && (c = rdl_resource_next_child (jr))) {
        rc = update_job_cores (ctx, c, job, pnode, pcores);
        rdl_resource_destroy (c);
    }

    if (imanode) {
        if (!(key = xasprintf ("lwj.%"PRId64".rank.%"PRId64".cores", job->lwj_id,
                               *pnode))
            < 0) {
            flux_log (ctx->h, LOG_ERR, "%s key create failed", __FUNCTION__);
            rc = -1;
            goto ret;
        } else if (kvs_put_int64 (ctx->h, key, *pcores) < 0) {
            flux_log (ctx->h,
                      LOG_ERR,
                      "%s %"PRId64" node failed: %s",
                      __FUNCTION__,
                      job->lwj_id,
                      strerror (errno));
            rc = -1;
            goto ret;
        }
        free (key);
        (*pnode)++;
    }

ret:
    return rc;
}

static int print_resources_helper (struct resource *r, int indent_level)
{
    int rc = 0, i = 0;
    struct resource *c;
    JSON o = NULL;

    if (r) {
        for (i = 0; i < indent_level; i++) {
            printf ("\t");
        }
        o = rdl_resource_json (r);
        printf ("%s", Jtostr (o));
        Jput (o);

        rdl_resource_iterator_reset (r);
        while (!rc && (c = rdl_resource_next_child (r))) {
            rc = print_resources_helper (c, indent_level + 1);
            rdl_resource_destroy (c);
        }
        rdl_resource_iterator_reset (r);
    } else {
        rc = -1;
    }

    return rc;
}

int print_resources (struct resource *r)
{
    return print_resources_helper (r, 0);
}

/*
 * Initialize the rdl resources with "idle" tags on each core
 */
int idlize_resources (struct resource *r)
{
    int rc = 0;
    struct resource *c;

    if (r) {
        rdl_resource_tag (r, IDLETAG);
        rdl_resource_iterator_reset (r);
        while (!rc && (c = rdl_resource_next_child (r))) {
            rc = idlize_resources (c);
            rdl_resource_destroy (c);
        }
        rdl_resource_iterator_reset (r);
    } else {
        rc = -1;
    }

    return rc;
}

int release_resources (ctx_t *ctx,
                       struct rdl *rdl,
                       const char *uri,
                       flux_lwj_t *job)
{
    int rc = -1;
    struct resource *jr = rdl_resource_get (job->rdl, uri);
    char *lwjtag = NULL;

    lwjtag = xasprintf ("lwj.%"PRId64"", job->lwj_id);

    if (jr) {
        rc = release_lwj_resource (ctx, rdl, jr, lwjtag);
        deallocate_bandwidth (ctx, rdl, uri, job);
        rdl_resource_destroy (jr);
    } else {
        flux_log (ctx->h,
                  LOG_ERR,
                  "release_resources failed to get resources: %s",
                  strerror (errno));
    }
    free (lwjtag);

    return rc;
}

int release_lwj_resource (ctx_t *ctx,
                          struct rdl *rdl,
                          struct resource *jr,
                          char *lwjtag)
{
    char *uri = NULL;
    const char *type = NULL;
    int rc = 0;
    JSON o = NULL;
    struct resource *child, *curr;

    uri = xasprintf ("%s:%s", ctx->uri, rdl_resource_path (jr));
    curr = rdl_resource_get (rdl, uri);

    if (curr) {
        o = rdl_resource_json (curr);
        Jget_str (o, "type", &type);
        if (strcmp (type, CORETYPE) == 0) {
            rdl_resource_delete_tag (curr, lwjtag);
            rdl_resource_delete_tag (curr, "lwj");
            rdl_resource_tag (curr, IDLETAG);
        }
        Jput (o);
        rdl_resource_destroy (curr);

        rdl_resource_iterator_reset (jr);
        while (!rc && (child = rdl_resource_next_child (jr))) {
            rc = release_lwj_resource (ctx, rdl, child, lwjtag);
            rdl_resource_destroy (child);
        }
        rdl_resource_iterator_reset (jr);
    } else {
        flux_log (ctx->h,
                  LOG_ERR,
                  "release_lwj_resource failed to get %s",
                  uri);
        rc = -1;
    }
    free (uri);

    return rc;
}

// Walk the job's rdl until you reach the cores. Keep track of
// ancestors the whole way down. When you reach a core, deallocate the
// job's bw for that core all the way up (core -> root)
static void deallocate_bandwidth_helper (ctx_t *ctx,
                                         struct rdl *rdl,
                                         struct resource *jr,
                                         int64_t io_rate,
                                         zlist_t *ancestors)
{
    struct resource *curr, *child, *ancestor;
    char *uri = NULL;
    const char *type = NULL;
    JSON o = NULL;

    uri = xasprintf ("%s:%s", ctx->uri, rdl_resource_path (jr));
    curr = rdl_resource_get (rdl, uri);

    if (curr) {
        o = rdl_resource_json (curr);
        Jget_str (o, "type", &type);
        if (strcmp (type, CORETYPE) == 0) {
            // Deallocate bandwidth of this resource
            deallocate_resource_bandwidth (ctx, curr, io_rate);
            // Deallocate bandwidth of this resource's ancestors
            ancestor = zlist_first (ancestors);
            while (ancestor != NULL) {
                deallocate_resource_bandwidth (ctx, ancestor, io_rate);
                ancestor = zlist_next (ancestors);
            }
        } else {  // if not a core, recurse
            zlist_push (ancestors, curr);
            rdl_resource_iterator_reset (jr);
            while ((child = rdl_resource_next_child (jr))) {
                deallocate_bandwidth_helper (
                    ctx, rdl, child, io_rate, ancestors);
                rdl_resource_destroy (child);
            }
            zlist_pop (ancestors);
        }
        Jput (o);
        rdl_resource_destroy (curr);
    } else {
        flux_log (ctx->h,
                  LOG_ERR,
                  "deallocate_bandwidth_helper failed to get %s",
                  uri);
    }
    free (uri);

    return;
}

void deallocate_bandwidth (ctx_t *ctx,
                           struct rdl *rdl,
                           const char *uri,
                           flux_lwj_t *job)
{
    flux_log (ctx->h,
              LOG_DEBUG,
              "deallocate_bandwidth job: %"PRId64", io_rate: %"PRId64"",
              job->lwj_id,
              job->req.io_rate);

    zlist_t *ancestors = zlist_new ();
    struct resource *jr = rdl_resource_get (job->rdl, uri);

    if (jr) {
        rdl_resource_iterator_reset (jr);
        deallocate_bandwidth_helper (ctx, rdl, jr, job->req.io_rate, ancestors);
        rdl_resource_destroy (jr);
    } else {
        flux_log (ctx->h,
                  LOG_ERR,
                  "deallocate_bandwidth failed to get resources: %s",
                  strerror (errno));
    }

    zlist_destroy (&ancestors);
    return;
}

void deallocate_resource_bandwidth (ctx_t *ctx,
                                    struct resource *r,
                                    int64_t amount)
{
    int64_t old_alloc_bw;
    int64_t new_alloc_bw;
    JSON o = rdl_resource_json (r);

    rdl_resource_get_int (r, "alloc_bw", &old_alloc_bw);
    new_alloc_bw = old_alloc_bw - amount;

    // flux_log (h, LOG_DEBUG, "deallocating bandwidth (was: %"PRId64", is: %"PRId64") at
    // %s", old_alloc_bw, new_alloc_bw, Jtostr (o));

    if (new_alloc_bw < 0) {
        flux_log (ctx->h,
                  LOG_ERR,
                  "too much bandwidth deallocated (%"PRId64") - %s",
                  amount,
                  Jtostr (o));
    }
    rdl_resource_set_int (r, "alloc_bw", new_alloc_bw);
    Jput (o);
}

void allocate_resource_bandwidth (struct resource *r, int64_t amount)
{
    int64_t old_alloc_bw;
    int64_t new_alloc_bw;

    rdl_resource_get_int (r, "alloc_bw", &old_alloc_bw);
    new_alloc_bw = amount + old_alloc_bw;
    rdl_resource_set_int (r, "alloc_bw", new_alloc_bw);
}

bool allocate_core (flux_lwj_t *job,
                    struct resource *resource,
                    zlist_t *ancestors,
                    struct rdl_accumulator *accum,
                    char *lwjtag)
{
    bool output = false;
    if (allocate_bandwidth (job, resource, ancestors)) {
        job->req.ncores--;
        job->alloc.ncores++;
        rdl_resource_tag (resource, lwjtag);
        rdl_resource_set_int (resource, "lwj", job->lwj_id);
        rdl_resource_delete_tag (resource, IDLETAG);
        output = (rdl_accumulator_add (accum, resource) >= 0);
    }
    return output;
}

/*
 * Walk the tree, find the required resources and tag with the lwj_id
 * to which it is allocated.
 */
bool allocate_resources (struct rdl *rdl,
                         const char *hierarchy,
                         struct resource *fr,
                         struct rdl_accumulator *accum,
                         flux_lwj_t *job,
                         zlist_t *ancestors)
{
    char *lwjtag = NULL;
    char *uri = NULL;
    const char *type = NULL;
    JSON o = NULL, o2 = NULL, o3 = NULL;
    struct resource *child, *curr;
    bool found = false;

    uri = xasprintf ("%s:%s", hierarchy, rdl_resource_path (fr));
    curr = rdl_resource_get (rdl, uri);
    free (uri);

    o = rdl_resource_json (curr);
    Jget_str (o, "type", &type);
    lwjtag = xasprintf ("lwj.%"PRId64"", job->lwj_id);
    if (job->req.nnodes && (strcmp (type, "node") == 0)) {
        job->req.nnodes--;
        job->alloc.nnodes++;
    } else if (job->req.ncores && (strcmp (type, CORETYPE) == 0)
               && (job->req.ncores > job->req.nnodes)) {
        /* We put the (job->req.ncores > job->req.nnodes) requirement
         * here to guarantee at least one core per node. */
        Jget_obj (o, "tags", &o2);
        Jget_obj (o2, IDLETAG, &o3);
        if (o3) {
            allocate_core (job, curr, ancestors, accum, lwjtag);
        }
    }
    free (lwjtag);
    Jput (o);

    found = !(job->req.nnodes || job->req.ncores);

    zlist_push (ancestors, curr);
    rdl_resource_iterator_reset (fr);
    while (!found && (child = rdl_resource_next_child (fr))) {
        found =
            allocate_resources (rdl, hierarchy, child, accum, job, ancestors);
        rdl_resource_destroy (child);
    }
    rdl_resource_iterator_reset (fr);
    zlist_pop (ancestors);
    rdl_resource_destroy (curr);

    return found;
}

int64_t get_avail_bandwidth (struct resource *r)
{
    int64_t max_bw;
    int64_t alloc_bw;

    rdl_resource_get_int (r, "max_bw", &max_bw);
    rdl_resource_get_int (r, "alloc_bw", &alloc_bw);
    return max_bw - alloc_bw;
}

#if CZMQ_VERSION < CZMQ_MAKE_VERSION(3, 0, 1)
// Compare two job_t's based on submit time
// Return true if they should be swapped
// AKA job1 was submitted after job2
bool job_compare_t (void *item1, void *item2)
{
    job_t *job1 = (job_t *)item1;
    job_t *job2 = (job_t *)item2;
    return job1->submit_time > job2->submit_time;
}
#else
// Compare two job_t's based on submit time
// Return > 0 if job1 was submitted after job2,
//        < 0 if job1 was submitted before job2,
//        = 0 if submit times are (essentially) equivalent
int job_compare_t (void *item1, void *item2)
{
    double t1 = ((job_t *)item1)->submit_time;
    double t2 = ((job_t *)item2)->submit_time;
    double delta = t1 - t2;
    if (fabs (delta) < DBL_EPSILON)
        return 0;
    else if (delta > 0)
        return 1;
    else
        return (-1);
}
#endif /* CZMQ_VERSION > 3.0.0 */

#if CZMQ_VERSION < CZMQ_MAKE_VERSION(3, 0, 1)
// Return true if job1 is expected to terminate after job2
bool job_compare_termination_fn (void *item1, void *item2)
{
    job_t *job1, *job2;
    double term1, term2;

    job1 = (job_t *)item1;
    job2 = (job_t *)item2;

    term1 = job1->start_time + job1->time_limit;
    term2 = job2->start_time + job2->time_limit;

    return (term1 > term2);
}
#else
// Compare two job_t's based on submit time
// Return > 0 if job1 was submitted after job2,
//        < 0 if job1 was submitted before job2,
//        = 0 if submit times are (essentially) equivalent
int job_compare_termination_fn (void *item1, void *item2)
{
    job_t *job1 = (job_t *)item1;
    job_t *job2 = (job_t *)item2;
    double t1 = job1->start_time + job1->time_limit;
    double t2 = job2->start_time + job2->time_limit;
    double delta = t1 - t2;
    if (fabs (delta) < DBL_EPSILON)
        return 0;
    else if (delta > 0)
        return 1;
    else
        return (-1);
}
#endif /* CZMQ_VERSION > 3.0.0 */

void queue_schedule_loop (ctx_t *ctx)
{
    flux_log (ctx->h, LOG_DEBUG, "Schedule loop queued");
    ctx->run_schedule_loop = true;
}

bool should_run_schedule_loop (ctx_t *ctx, int time)
{
    flux_log (ctx->h,
              LOG_DEBUG,
              "run_schedule_loop: %d, time 'mod' 30: %d",
              ctx->run_schedule_loop,
              time % 30);
    return ctx->run_schedule_loop && !(time % 30);
}

void end_schedule_loop (ctx_t *ctx)
{
    ctx->run_schedule_loop = false;
}

// Received an event that a simulation is starting
void start_cb (flux_t h,
               flux_msg_handler_t *w,
               const flux_msg_t *msg,
               void *arg)
{
    ctx_t *ctx = getctx (h);

    flux_log (h, LOG_DEBUG, "received a start event");
    if (send_join_request (h, module_name, -1) < 0) {
        flux_log (h,
                  LOG_ERR,
                  "submit module failed to register with sim module");
        return;
    }

    flux_log (h, LOG_DEBUG, "sent a join request");

    // Turn off normal functionality, switch to sim_mode
    ctx->in_sim = true;
    if (flux_event_unsubscribe (h, "sim.start") < 0) {
        flux_log (h, LOG_ERR, "failed to unsubscribe from \"sim.start\"");
        return;
    } else {
        flux_log (h, LOG_DEBUG, "unsubscribed from \"sim.start\"");
    }
    if (flux_event_unsubscribe (h, "sim_sched.event") < 0) {
        flux_log (h, LOG_ERR, "failed to unsubscribe from \"sim_sched.event\"");
        return;
    } else {
        flux_log (h, LOG_DEBUG, "unsubscribed from \"sim_sched.event\"");
    }

    return;
}

int init_and_start_scheduler (flux_t h,
                              ctx_t *ctx,
                              zhash_t *args,
                              struct flux_msg_handler_spec *tab)
{
    int rc = 0;
    char *path;
    struct resource *r = NULL;
    struct rdllib *rdllib = NULL;
    uint32_t rank;

    if (flux_get_rank (h, &rank) < 0)
        return -1;

    if (rank != 0) {
        flux_log (h, LOG_ERR, "sim_ched module must only run on rank 0");
        rc = -1;
        goto ret;
    }
    flux_log (h, LOG_INFO, "sim_sched comms module starting");

    if (!(path = zhash_lookup (args, "rdl-conf"))) {
        flux_log (h, LOG_ERR, "rdl-conf argument is not set");
        rc = -1;
        goto ret;
    }

    if (!(rdllib = rdllib_open ())
        || !(ctx->rdl = rdl_loadfile (rdllib, path))) {
        flux_log (h,
                  LOG_ERR,
                  "failed to load resources from %s: %s",
                  path,
                  strerror (errno));
        rc = -1;
        goto ret;
    }

    if (!(ctx->uri = zhash_lookup (args, "rdl-resource"))) {
        flux_log (h, LOG_INFO, "using default rdl resource");
        ctx->uri = "default";
    }

    if ((r = rdl_resource_get (ctx->rdl, ctx->uri))) {
        flux_log (h, LOG_DEBUG, "setting up rdl resources");
        if (idlize_resources (r)) {
            flux_log (h,
                      LOG_ERR,
                      "failed to idlize %s: %s",
                      ctx->uri,
                      strerror (errno));
            rc = -1;
            goto ret;
        }
        flux_log (h, LOG_DEBUG, "successfully set up rdl resources");
    } else {
        flux_log (
            h, LOG_ERR, "failed to get %s: %s", ctx->uri, strerror (errno));
        rc = -1;
        goto ret;
    }

    if (flux_event_subscribe (h, "sim.start") < 0) {
        flux_log (h, LOG_ERR, "subscribing to event: %s", strerror (errno));
        return -1;
    }
    if (flux_event_subscribe (h, "sim_sched.event") < 0) {
        flux_log (h, LOG_ERR, "subscribing to event: %s", strerror (errno));
        rc = -1;
        goto ret;
    }
    if (flux_msg_handler_addvec (h, tab, NULL) < 0) {
        flux_log (h, LOG_ERR, "flux_msg_handler_addvec: %s", strerror (errno));
        return -1;
    }

    goto skip_for_sim;

    if (wait_for_lwj_init (h) == -1) {
        flux_log (h, LOG_ERR, "wait for lwj failed: %s", strerror (errno));
        rc = -1;
        goto ret;
    }

    if (jsc_notify_status (h, job_status_cb, (void *)h) != 0) {
        flux_log (h, LOG_ERR, "error registering a job status change CB");
        rc = -1;
        goto ret;
    }

skip_for_sim:
    send_alive_request (h, module_name);
    flux_log (h, LOG_DEBUG, "sent alive request");

    if (flux_reactor_run (flux_get_reactor (h), 0) < 0) {
        flux_log (h, LOG_ERR, "flux_reactor_run: %s", strerror (errno));
        rc = -1;
        goto ret;
    }

    rdllib_close (rdllib);

ret:
    flux_msg_handler_delvec (tab);
    return rc;
}

/*
 * schedule_job() searches through all of the idle resources (cores
 * right now) to satisfy a job's requirements.  If enough resources
 * are found, it proceeds to allocate those resources and update the
 * kvs's lwj entry in preparation for job execution. Returns 1 if the
 * job was succesfully scheduled, 0 if it was not.
 */
int schedule_job (ctx_t *ctx,
                  struct rdl *rdl,
                  struct rdl *free_rdl,
                  const char *uri,
                  int64_t free_cores,
                  flux_lwj_t *job)
{
    int rc = 0;
    struct rdl_accumulator *a = NULL;

    struct resource *free_root = NULL; /* found resource */

    if (!job || !rdl || !uri) {
        flux_log (ctx->h, LOG_ERR, "schedule_job invalid arguments");
        goto ret;
    }

    if (free_cores > 0 && free_rdl) {
        free_root = rdl_resource_get (free_rdl, uri);
    } else {
        flux_log (ctx->h,
                  LOG_DEBUG,
                  "schedule_job called on job %"PRId64", no cores available, "
                  "skipping",
                  job->lwj_id);
        goto ret;
    }

    flux_log (ctx->h,
              LOG_DEBUG,
              "schedule_job called on job %"PRId64", request cores: %d, free cores: "
              "%"PRId64"",
              job->lwj_id,
              job->req.ncores,
              free_cores);

    if (free_root && free_cores >= job->req.ncores) {
        zlist_t *ancestors = zlist_new ();
        // TODO: revert this in the deallocation/rollback
        int old_nnodes = job->req.nnodes;
        int old_ncores = job->req.ncores;
        int old_io_rate = job->req.io_rate;
        int old_alloc_nnodes = job->alloc.nnodes;
        int old_alloc_ncores = job->alloc.ncores;
        rdl_resource_iterator_reset (free_root);
        a = rdl_accumulator_create (rdl);
        if (allocate_resources (rdl, uri, free_root, a, job, ancestors)) {
            flux_log (ctx->h, LOG_INFO, "scheduled job %"PRId64"", job->lwj_id);
            job->rdl = rdl_accumulator_copy (a);
            job->state = j_submitted;
            rc = update_job (ctx, job);
            if (rc == 0)
                rc = 1;
        } else {
            flux_log (ctx->h,
                      LOG_DEBUG,
                      "not enough resources to allocate, rolling back");
            job->req.io_rate = old_io_rate;
            job->req.nnodes = old_nnodes;
            job->req.ncores = old_ncores;
            job->alloc.nnodes = old_alloc_nnodes;
            job->alloc.ncores = old_alloc_ncores;

            if (rdl_accumulator_is_empty (a)) {
                flux_log (ctx->h,
                          LOG_DEBUG,
                          "no resources found in accumulator");
            } else {
                job->rdl = rdl_accumulator_copy (a);
                release_resources (ctx, rdl, uri, job);
                rdl_destroy (job->rdl);
            }
        }
        zlist_destroy (&ancestors);
        rdl_accumulator_destroy (a);
    } else {
        flux_log (ctx->h,
                  LOG_DEBUG,
                  "not enough available cores, skipping this job");
    }
    rdl_resource_destroy (free_root);

ret:
    return rc;
}

bool resources_equal (struct resource *r1, struct resource *r2)
{
    struct resource *c1 = NULL, *c2 = NULL;
    JSON o1 = NULL, o2 = NULL;
    bool rc = false;

    if (r1 && r2) {
        o1 = rdl_resource_json (r1);
        o2 = rdl_resource_json (r2);
        // TODO: improve this simple comparison
        rc = !strcmp (Jtostr (o1), Jtostr (o2));
        Jput (o1);
        Jput (o2);

        rdl_resource_iterator_reset (r1);
        rdl_resource_iterator_reset (r2);
        while (rc && (c1 = rdl_resource_next_child (r1))
               && (c2 = rdl_resource_next_child (r2))) {
            rc = resources_equal (c1, c2);
            rdl_resource_destroy (c1);
            rdl_resource_destroy (c2);
        }
        rdl_resource_iterator_reset (r1);
        rdl_resource_iterator_reset (r2);
    } else {
        rc = false;
    }

    return rc;
}

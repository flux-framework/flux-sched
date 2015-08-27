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
#include <string.h>
#include <errno.h>
#include <libgen.h>
#include <czmq.h>
#include <json.h>
#include <dlfcn.h>
#include <time.h>
#include <inttypes.h>

#include "src/common/libutil/jsonutil.h"
#include "src/common/libutil/log.h"
#include "src/common/libutil/shortjson.h"
#include "src/common/libutil/xzmalloc.h"
#include "rdl.h"
#include "scheduler.h"
#include "simulator.h"

#define MAX_STR_LEN 128

static const char *module_name = "sim_sched";

/****************************************************************
 *
 *                 INTERNAL DATA STRUCTURE
 *
 ****************************************************************/
struct stab_struct {
    int i;
    const char *s;
};

typedef struct {
	char *key;
	char *val;
	int errnum;
} kvs_event_t;

/****************************************************************
 *
 *                 STATIC DATA
 *
 ****************************************************************/
static zlist_t *p_queue = NULL;
static zlist_t *r_queue = NULL;
static zlist_t *c_queue = NULL;
static zlist_t *ev_queue = NULL;
static flux_t h = NULL;
static struct rdl *rdl = NULL;
static char* resource = NULL;
static const char* IDLETAG = "idle";
static const char* CORETYPE = "core";

static bool in_sim = false;
static sim_state_t *sim_state = NULL;
static zlist_t *kvs_queue = NULL;
static zlist_t *timer_queue = NULL;

static struct stab_struct jobstate_tab[] = {
    { j_null,      "null" },
    { j_reserved,  "reserved" },
    { j_submitted, "submitted" },
    { j_unsched,   "unsched" },
    { j_pending,   "pending" },
    { j_runrequest,"runrequest" },
    { j_allocated, "allocated" },
    { j_starting,  "starting" },
    { j_running,   "running" },
    { j_cancelled, "cancelled" },
    { j_complete,  "complete" },
    { j_reaped,    "reaped" },
    { -1, NULL },
};

//Set the timer for "module" to happen relatively soon
//If the mod is sim_exec, it shouldn't happen immediately
//because the scheduler still needs to transition through
//3->4 states before the sim_exec module can actually "exec" a job
static void set_next_event(const char* module)
{
	double next_event;
	double *timer = zhash_lookup (sim_state->timers, module);
	next_event = sim_state->sim_time + ( (!strcmp (module, "sim_exec")) ? .0001 : .00001);
	if (*timer > next_event || *timer < 0){
		*timer = next_event;
		flux_log (h, LOG_DEBUG, "next %s event set for %f", module, next_event);
	}
}

static void handle_timer_queue ()
{
	while (zlist_size (timer_queue) > 0)
		set_next_event (zlist_pop (timer_queue));
}

static void queue_timer_change (const char* module)
{
	zlist_append (timer_queue, (void *) module);
}

/****************************************************************
 *
 *         Resource Description Library Setup
 *
 ****************************************************************/
#if 0
static void f_err (flux_t h, const char *msg, ...)
{
    va_list ap;
    va_start (ap, msg);
    flux_vlog (h, LOG_ERR, msg, ap);
    va_end (ap);
}

/* XXX: Borrowed from flux.c and subject to change... */
static void setup_rdl_lua (void)
{
    char *s;
    char  exe_path [MAXPATHLEN];
    char *exe_dir;
    char *rdllib;

    memset (exe_path, 0, MAXPATHLEN);
    if (readlink ("/proc/self/exe", exe_path, MAXPATHLEN - 1) < 0)
        err_exit ("readlink (/proc/self/exe)");
    exe_dir = dirname (exe_path);

    s = getenv ("LUA_CPATH");
    setenvf ("LUA_CPATH", 1, "%s/dlua/?.so;%s", exe_dir, s ? s : ";");
    s = getenv ("LUA_PATH");
    setenvf ("LUA_PATH", 1, "%s/dlua/?.lua;%s", exe_dir, s ? s : ";");

    flux_log (h, LOG_DEBUG, "LUA_PATH %s", getenv ("LUA_PATH"));
    flux_log (h, LOG_DEBUG, "LUA_CPATH %s", getenv ("LUA_CPATH"));

    asprintf (&rdllib, "%s/lib/librdl.so", exe_dir);
    if (!dlopen (rdllib, RTLD_NOW | RTLD_GLOBAL)) {
        flux_log (h, LOG_ERR, "dlopen %s failed", rdllib);
        return;
    }
    free(rdllib);

    rdllib_set_default_errf (h, (rdl_err_f)(&f_err));
}
#endif

//Reply back to the sim module with the updated sim state (in JSON form)
int send_reply_request (flux_t h, sim_state_t *sim_state)
{
	JSON o = sim_state_to_json (sim_state);
	Jadd_bool (o, "event_finished", true);
    if (flux_json_request (h, FLUX_NODEID_ANY,
                              FLUX_MATCHTAG_NONE, "sim.reply", o) < 0) {
		Jput (o);
		return -1;
	}
	flux_log(h, LOG_DEBUG, "sent a reply request");
   Jput (o);
   free_simstate (sim_state);
   return 0;
}

static int
signal_event ( )
{
    int rc = 0;
    zmsg_t *zmsg = NULL;

    if (in_sim && sim_state != NULL) {
        queue_timer_change (module_name);
        //send_reply_request (h, sim_state);
        goto ret;
    } else if (!(zmsg = flux_event_encode ("sim_sched.event", NULL))
             || flux_sendmsg (h, &zmsg) < 0) {
        flux_log (h, LOG_ERR,
                  "flux_sendmsg: %s", strerror (errno));
        rc = -1;
    }

    zmsg_destroy (&zmsg);
ret:
    return rc;
}


static flux_lwj_t *
find_lwj (int64_t id)
{
    flux_lwj_t *j = NULL;

    j = zlist_first (p_queue);
    while (j) {
        if (j->lwj_id == id)
            break;
        j = zlist_next (p_queue);
    }
    if (j)
        return j;

    j = zlist_first (r_queue);
    while (j) {
        if (j->lwj_id == id)
            break;
        j = zlist_next (r_queue);
    }

    return j;
}


/****************************************************************
 *
 *              Utility Functions
 *
 ****************************************************************/

static char * ctime_iso8601_now (char *buf, size_t sz)
{
    struct tm tm;
    time_t now = time (NULL);

    memset (buf, 0, sz);

    if (!localtime_r (&now, &tm))
        err_exit ("localtime");
    strftime (buf, sz, "%FT%T", &tm);

    return buf;
}

static int
stab_lookup (struct stab_struct *ss, const char *s)
{
    while (ss->s != NULL) {
        if (!strcmp (ss->s, s))
            return ss->i;
        ss++;
    }
    return -1;
}

static const char *
stab_rlookup (struct stab_struct *ss, int i)
{
    while (ss->s != NULL) {
        if (ss->i == i)
            return ss->s;
        ss++;
    }
    return "unknown";
}

/*
 * Update the job's kvs entry for state and mark the time.
 * Intended to be part of a series of changes, so the caller must
 * invoke the kvs_commit at some future point.
 */
int update_job_state (flux_lwj_t *job, lwj_event_e e)
{
    char buf [64];
    char *key = NULL;
    char *key2 = NULL;
    int rc = -1;
    const char *state;

    ctime_iso8601_now (buf, sizeof (buf));

    state = stab_rlookup (jobstate_tab, e);
    if (!strcmp (state, "unknown")) {
        flux_log (h, LOG_ERR, "unknown job state %d", e);
    } else if (asprintf (&key, "lwj.%ld.state", job->lwj_id) < 0) {
        flux_log (h, LOG_ERR, "update_job_state key create failed");
    } else if (kvs_put_string (h, key, state) < 0) {
        flux_log (h, LOG_ERR, "update_job_state %ld state update failed: %s",
                  job->lwj_id, strerror (errno));
    } else if (asprintf (&key2, "lwj.%ld.%s-time", job->lwj_id, state) < 0) {
        flux_log (h, LOG_ERR, "update_job_state key2 create failed");
    } else if (kvs_put_string (h, key2, buf) < 0) {
        flux_log (h, LOG_ERR, "update_job_state %ld %s-time failed: %s",
                  job->lwj_id, state, strerror (errno));
    } else {
        rc = 0;
        flux_log (h, LOG_DEBUG, "updated job %ld's state in the kvs to %s",
                  job->lwj_id, state);
    }

    free (key);
    free (key2);

    return rc;
}

#if 0
static inline void
set_event (flux_event_t *e,
           event_class_e c, int ei, flux_lwj_t *j)
{
    e->t = c;
    e->lwj = j;
    switch (c) {
    case lwj_event:
        e->ev.je = (lwj_event_e) ei;
        break;
    case res_event:
        e->ev.re = (res_event_e) ei;
        break;
    default:
        flux_log (h, LOG_ERR, "unknown ev class");
        break;
    }
    return;
}
#endif


static int
extract_lwjid (const char *k, int64_t *i)
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
    if (strncmp(lwj, "lwj", 3) != 0) {
        rc = -1;
        goto ret;
    }
    id = strtok (NULL, ".");
    *i = strtoul(id, (char **) NULL, 10);

ret:
    return rc;
}

static int
extract_lwjinfo (flux_lwj_t *j)
{
    char *key = NULL;
    char *state;
    int64_t reqnodes = 0;
    int64_t reqtasks = 0;
    int rc = -1;

    if (asprintf (&key, "lwj.%ld.state", j->lwj_id) < 0) {
        flux_log (h, LOG_ERR, "extract_lwjinfo state key create failed");
        goto ret;
    } else if (kvs_get_string (h, key, &state) < 0) {
        flux_log (h, LOG_ERR, "extract_lwjinfo %s: %s", key, strerror (errno));
        goto ret;
    } else {
        j->state = stab_lookup (jobstate_tab, state);
        flux_log (h, LOG_DEBUG, "extract_lwjinfo got %s: %s", key, state);
        free(key);
    }

    if (asprintf (&key, "lwj.%ld.nnodes", j->lwj_id) < 0) {
        flux_log (h, LOG_ERR, "extract_lwjinfo nnodes key create failed");
        goto ret;
    } else if (kvs_get_int64 (h, key, &reqnodes) < 0) {
        flux_log (h, LOG_ERR, "extract_lwjinfo get %s: %s",
                  key, strerror (errno));
        goto ret;
    } else {
        j->req.nnodes = reqnodes;
        flux_log (h, LOG_DEBUG, "extract_lwjinfo got %s: %ld", key, reqnodes);
        free(key);
    }

    if (asprintf (&key, "lwj.%ld.ntasks", j->lwj_id) < 0) {
        flux_log (h, LOG_ERR, "extract_lwjinfo ntasks key create failed");
        goto ret;
    } else if (kvs_get_int64 (h, key, &reqtasks) < 0) {
        flux_log (h, LOG_ERR, "extract_lwjinfo get %s: %s",
                  key, strerror (errno));
        goto ret;
    } else {
        /* Assuming a 1:1 relationship right now between cores and tasks */
        j->req.ncores = reqtasks;
        flux_log (h, LOG_DEBUG, "extract_lwjinfo got %s: %ld", key, reqtasks);
        free(key);
        j->alloc.nnodes = 0;
        j->alloc.ncores = 0;
        j->rdl = NULL;
        rc = 0;
    }

ret:
    return rc;
}


static void
issue_lwj_event (lwj_event_e e, flux_lwj_t *j)
{
    flux_event_t *ev
        = (flux_event_t *) xzmalloc (sizeof (flux_event_t));
    ev->t = lwj_event;
    ev->ev.je = e;
    ev->lwj = j;

    if (zlist_append (ev_queue, ev) == -1) {
        flux_log (h, LOG_ERR,
                  "enqueuing an event failed");
        goto ret;
    }
    if (signal_event () == -1) {
        flux_log (h, LOG_ERR,
                  "signaling an event failed");
        goto ret;
    }

ret:
    return;
}

/****************************************************************
 *
 *         Scheduler Activities
 *
 ****************************************************************/

/*
 * Initialize the rdl resources with "idle" tags on each core
 */
static int
idlize_resources (struct resource *r)
{
    int rc = 0;
    struct resource *c;

    if (r) {
        rdl_resource_tag (r, IDLETAG);
        while (!rc && (c = rdl_resource_next_child (r))) {
            rc = idlize_resources (c);
            rdl_resource_destroy (c);
        }
    } else {
        flux_log (h, LOG_ERR, "idlize_resources passed a null resource");
        rc = -1;
    }

    return rc;
}

/*
 * Walk the tree, find the required resources and tag with the lwj_id
 * to which it is allocated.
 */
static bool
allocate_resources (struct resource *fr, struct rdl_accumulator *a,
                    flux_lwj_t *job)
{
    char *lwjtag = NULL;
    char *uri = NULL;
    const char *type = NULL;
    json_object *o = NULL;
    json_object *o2 = NULL;
    json_object *o3 = NULL;
    struct resource *c;
    struct resource *r;
    bool found = false;

    asprintf (&uri, "%s:%s", resource, rdl_resource_path (fr));
    r = rdl_resource_get (rdl, uri);
    free (uri);

    o = rdl_resource_json (r);
    Jget_str (o, "type", &type);
    asprintf (&lwjtag, "lwj.%ld", job->lwj_id);
    if (job->req.nnodes && (strcmp (type, "node") == 0)) {
		/*
        Jget_obj (o, "tags", &o2);
        Jget_obj (o2, IDLETAG, &o3);
        if (o3) {
		*/
			job->req.nnodes--;
			job->alloc.nnodes++;
			/*
            rdl_resource_tag (r, lwjtag);
            rdl_resource_delete_tag (r, IDLETAG);
            rdl_accumulator_add (a, r);
			}*/
			//flux_log (h, LOG_DEBUG, "allocated node: %s", json_object_to_json_string (o));
    } else if (job->req.ncores && (strcmp (type, CORETYPE) == 0) &&
               (job->req.ncores > job->req.nnodes)) {
        /* We put the (job->req.ncores > job->req.nnodes) requirement
         * here to guarantee at least one core per node. */
        Jget_obj (o, "tags", &o2);
        Jget_obj (o2, IDLETAG, &o3);
        if (o3) {
            job->req.ncores--;
            job->alloc.ncores++;
            rdl_resource_tag (r, lwjtag);
            rdl_resource_delete_tag (r, IDLETAG);
            rdl_accumulator_add (a, r);
            //flux_log (h, LOG_DEBUG, "allocated core: %s", json_object_to_json_string (o));
        }
    }
    free (lwjtag);
    json_object_put (o);

    found = !(job->req.nnodes || job->req.ncores);

    while (!found && (c = rdl_resource_next_child (fr))) {
        found = allocate_resources (c, a, job);
        rdl_resource_destroy (c);
    }

    return found;
}

/*
 * Recursively search the resource r and update this job's lwj key
 * with the core count per rank (i.e., node for the time being)
 */
static int
update_job_cores (struct resource *jr, flux_lwj_t *job,
                  uint64_t *pnode, uint32_t *pcores)
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
            //flux_log (h, LOG_DEBUG, "updating: %s", json_object_to_json_string (o));
        } else {
            flux_log (h, LOG_ERR, "update_job_cores invalid resource");
            rc = -1;
            goto ret;
        }
    } else {
        flux_log (h, LOG_ERR, "update_job_cores passed a null resource");
        rc = -1;
        goto ret;
    }

    Jget_str (o, "type", &type);
    if (strcmp (type, "node") == 0) {
        *pcores = 0;
        imanode = true;
    } else if (strcmp (type, CORETYPE) == 0) {
        /* we need to limit our allocation to just the tagged cores */
        asprintf (&lwjtag, "lwj.%ld", job->lwj_id);
        Jget_obj (o, "tags", &o2);
        Jget_obj (o2, lwjtag, &o3);
        if (o3) {
            (*pcores)++;
        }
        free (lwjtag);
    }
    json_object_put (o);

    while ((rc == 0) && (c = rdl_resource_next_child (jr))) {
        rc = update_job_cores (c, job, pnode, pcores);
        rdl_resource_destroy (c);
    }

    if (imanode) {
        if (asprintf (&key, "lwj.%ld.rank.%ld.cores", job->lwj_id,
                      *pnode) < 0) {
            flux_log (h, LOG_ERR, "update_job_cores key create failed");
            rc = -1;
            goto ret;
        } else if (kvs_put_int64 (h, key, *pcores) < 0) {
            flux_log (h, LOG_ERR, "update_job_cores %ld node failed: %s",
                      job->lwj_id, strerror (errno));
            rc = -1;
            goto ret;
        }
        free (key);
        (*pnode)++;
    }

ret:
    return rc;
}

/*
 * Create lwj entries to tell wrexecd how many tasks to launch per
 * node.
 * The key has the form:  lwj.<jobID>.rank.<nodeID>.cores
 * The value will be the number of tasks to launch on that node.
 */
static int
update_job_resources (flux_lwj_t *job)
{
    uint64_t node = 0;
    uint32_t cores = 0;
    struct resource *jr = rdl_resource_get (job->rdl, resource);
    int rc = -1;

    if (jr)
        rc = update_job_cores (jr, job, &node, &cores);
    else
        flux_log (h, LOG_ERR, "update_job_resources passed a null resource");

    return rc;
}

/*
 * Add the allocated resources to the job, and
 * change its state to "allocated".
 */
static int
update_job (flux_lwj_t *job)
{
	flux_log (h, LOG_DEBUG, "updating job %ld", job->lwj_id);
    int rc = -1;

    if (update_job_state (job, j_allocated)) {
        flux_log (h, LOG_ERR, "update_job failed to update job %ld to %s",
                  job->lwj_id, stab_rlookup (jobstate_tab, j_allocated));
    } else if (update_job_resources(job)) {
		kvs_commit (h);
        flux_log (h, LOG_ERR, "update_job %ld resrc update failed", job->lwj_id);
    } else if (kvs_commit (h) < 0) {
        flux_log (h, LOG_ERR, "kvs_commit error!");
    } else {
        rc = 0;
    }

	flux_log (h, LOG_DEBUG, "updated job %ld", job->lwj_id);
    return rc;
}

static struct rdl *get_free_subset (struct rdl *rdl, const char *type)
{
	JSON tags = Jnew();
	Jadd_bool (tags, IDLETAG, true);
	JSON args = Jnew ();
	Jadd_obj (args, "tags", tags);
	Jadd_str (args, "type", type);
    struct rdl *frdl = rdl_find (rdl, args);
	Jput (args);
	Jput (tags);
	return frdl;
}

static int64_t get_free_count (struct rdl *rdl, const char *uri, const char *type)
{
	JSON o;
	int64_t count = -1;
	int rc = -1;
	struct resource *fr = NULL;

	if ((fr = rdl_resource_get (rdl, uri)) == NULL) {
		flux_log (h, LOG_ERR, "failed to get found resources: %s", uri);
		return -1;
	}

	o = rdl_resource_aggregate_json (fr);
	if (o) {
		const char *json_string = Jtostr (o);
		flux_log (h, LOG_DEBUG, "agg json - %s", json_string);
		if (!Jget_int64(o, type, &count)) {
			flux_log (h, LOG_ERR, "schedule_job failed to get %s: %d",
					  type, rc);
			return -1;
		} else {
			flux_log (h, LOG_DEBUG, "schedule_job found %ld idle %ss", count, type);
		}
		Jput (o);
	}

	return count;
}

/*
 * schedule_job() searches through all of the idle resources (cores
 * right now) to satisfy a job's requirements.  If enough resources
 * are found, it proceeds to allocate those resources and update the
 * kvs's lwj entry in preparation for job execution.
 */
int schedule_job (struct rdl *rdl, const char *uri, flux_lwj_t *job, bool first_job)
{
    //int64_t nodes = -1;
    int rc = 0;
    struct rdl_accumulator *a = NULL;

	//The "cache"
    static int64_t cores = -1;
    static struct rdl *frdl = NULL;            /* found rdl */
    static struct resource *fr = NULL;         /* found resource */
	static bool cache_valid = false;

    if (!job || !rdl || !uri) {
        flux_log (h, LOG_ERR, "schedule_job invalid arguments");
        goto ret;
    }

	flux_log (h, LOG_DEBUG, "beginning the scheduling of job %ld", job->lwj_id);

	//Cache results between schedule loops
	if (!cache_valid || first_job) {
		flux_log (h, LOG_DEBUG, "refreshing cache");
		frdl = get_free_subset (rdl, "core");
		if (frdl) {
			cores = get_free_count (frdl, uri, "core");
			fr = rdl_resource_get (frdl, uri);
		}
		cache_valid = true;
	}

	if (frdl && fr && cores > 0) {
		if (cores >= job->req.ncores) {
			rdl_resource_iterator_reset (fr);
			a = rdl_accumulator_create (rdl);
			if (allocate_resources (fr, a, job)) {
				flux_log (h, LOG_INFO, "scheduled job %ld", job->lwj_id);
				job->rdl = rdl_accumulator_copy (a);
				job->state = j_submitted;
				rc = update_job (job);
				rdl_destroy (frdl);

				//Clear the "cache"
				cache_valid = false;
				frdl = NULL;
				fr = NULL;
				cores = -1;
			}
			rdl_accumulator_destroy (a);
		}
	}

ret:
    return rc;
}

int schedule_jobs (struct rdl *rdl, const char *uri, zlist_t *jobs)
{
    flux_lwj_t *job = NULL;
    int rc = 0;
	bool first_job = true;

    job = zlist_first (jobs);
    while (!rc && job) {
		if (job->state == j_unsched) {
			rc = schedule_job(rdl, uri, job, first_job);
			first_job = false;
		}
        job = zlist_next (jobs);
    }
	flux_log (h, LOG_DEBUG, "Finished iterating over the jobs list");
    return rc;
}


/****************************************************************
 *
 *         Actions Led by Current State + an Event
 *
 ****************************************************************/

static int
request_run (flux_lwj_t *job)
{
    int rc = -1;
    char *topic = NULL;
    zmsg_t *zmsg = NULL;

    if (update_job_state (job, j_runrequest) < 0) {
        flux_log (h, LOG_ERR, "request_run failed to update job %ld to %s",
                  job->lwj_id, stab_rlookup (jobstate_tab, j_runrequest));
        goto done;
    }
    if (kvs_commit (h) < 0) {
        flux_log (h, LOG_ERR, "kvs_commit error!");
        goto done;
    }
    if (!in_sim) {
        topic = xasprintf ("rexec.run.%ld", job->lwj_id);
        if (!(zmsg = flux_event_encode (topic, NULL))
            || flux_sendmsg (h, &zmsg) < 0) {
            flux_log (h, LOG_ERR, "request_run event send failed: %s",
                      strerror (errno));
            goto done;
        }
    } else {
        topic = xasprintf ("sim_exec.run.%ld", job->lwj_id);
        if (flux_json_request (h, FLUX_NODEID_ANY,
                               FLUX_MATCHTAG_NONE, topic, NULL) < 0) {
            flux_log (h, LOG_ERR, "request_run request send failed: %s",
                      strerror (errno));
            goto done;
        }
    }
    flux_log (h, LOG_DEBUG, "job %ld runrequest", job->lwj_id);
    rc = 0;
done:
    if (topic)
        free (topic);

    zmsg_destroy (&zmsg);
    return rc;
}


static int
issue_res_event (flux_lwj_t *lwj)
{
    int rc = 0;
    flux_event_t *newev
        = (flux_event_t *) xzmalloc (sizeof (flux_event_t));

    // TODO: how to update the status of each entry as "free"
    // then destroy zlist_t without having to destroy
    // the elements
    // release lwj->resource

    newev->t = res_event;
    newev->ev.re = r_released;
    newev->lwj = lwj;

    if (zlist_append (ev_queue, newev) == -1) {
        flux_log (h, LOG_ERR,
                  "enqueuing an event failed");
        rc = -1;
        goto ret;
    }
    if (signal_event () == -1) {
        flux_log (h, LOG_ERR,
                  "signal the event-enqueued event ");
        rc = -1;
        goto ret;
    }

ret:
    return rc;
}

static int
release_lwj_resource (struct rdl *rdl, struct resource *jr, int64_t lwj_id)
{
    char *lwjtag = NULL;
    char *uri = NULL;
    const char *type = NULL;
    int rc = 0;
    json_object *o = NULL;
    struct resource *c;
    struct resource *r;

    asprintf (&uri, "%s:%s", resource, rdl_resource_path (jr));
    r = rdl_resource_get (rdl, uri);

    if (r) {
        o = rdl_resource_json (r);
        Jget_str (o, "type", &type);
        if (strcmp (type, CORETYPE) == 0) {
            asprintf (&lwjtag, "lwj.%ld", lwj_id);
            rdl_resource_delete_tag (r, lwjtag);
            rdl_resource_tag (r, IDLETAG);
            free (lwjtag);
        }
        flux_log (h, LOG_DEBUG, "resource released: %s", json_object_to_json_string (o));
        json_object_put (o);

        while (!rc && (c = rdl_resource_next_child (jr))) {
            rc = release_lwj_resource (rdl, c, lwj_id);
            rdl_resource_destroy (c);
        }
    } else {
        flux_log (h, LOG_ERR, "release_lwj_resource failed to get %s", uri);
        rc = -1;
    }
    free (uri);

    return rc;
}

/*
 * Find resources allocated to this job, and remove the lwj tag.
 */
int release_resources (struct rdl *rdl, const char *uri, flux_lwj_t *job)
{
    int rc = -1;
    struct resource *jr = rdl_resource_get (job->rdl, uri);

    if (jr) {
        rdl_resource_iterator_reset (jr);
        rc = release_lwj_resource (rdl, jr, job->lwj_id);
    } else {
        flux_log (h, LOG_ERR, "release_resources failed to get resources: %s",
                  strerror (errno));
    }
	rdl_destroy (job->rdl);

    return rc;
}

static int
move_to_r_queue (flux_lwj_t *lwj)
{
    zlist_remove (p_queue, lwj);
    return zlist_append (r_queue, lwj);
}

static int
move_to_c_queue (flux_lwj_t *lwj)
{
    zlist_remove (r_queue, lwj);
    return zlist_append (c_queue, lwj);
}


static int
action_j_event (flux_event_t *e)
{
    /* e->lwj->state is the current state
     * e->ev.je      is the new state
     */
    /*
	flux_log (h, LOG_DEBUG, "attempting job %ld state change from %s to %s",
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
        extract_lwjinfo (e->lwj);
        if (e->lwj->state != j_submitted) {
            flux_log (h, LOG_ERR,
                      "job %ld read state mismatch ", e->lwj->lwj_id);
            goto bad_transition;
        }
        e->lwj->state = j_unsched;
        schedule_jobs (rdl, resource, p_queue);
        break;

    case j_submitted:
        if (e->ev.je != j_allocated) {
            goto bad_transition;
        }
        e->lwj->state = j_allocated;
        request_run(e->lwj);
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
		if (in_sim)
			queue_timer_change ("sim_exec");
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
        move_to_r_queue (e->lwj);
        break;

    case j_running:
        if (e->ev.je != j_complete) {
            goto bad_transition;
        }
        /* TODO move this to j_complete case once reaped is implemented */
        move_to_c_queue (e->lwj);
        issue_res_event (e->lwj);
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
        flux_log (h, LOG_ERR, "job %ld unknown state %d",
                  e->lwj->lwj_id, e->lwj->state);
        break;
    }

    return 0;

bad_transition:
    flux_log (h, LOG_ERR, "job %ld bad state transition from %s to %s",
              e->lwj->lwj_id, stab_rlookup (jobstate_tab, e->lwj->state),
                              stab_rlookup (jobstate_tab, e->ev.je));
    return -1;
}


static int
action_r_event (flux_event_t *e)
{
    int rc = -1;

    if ((e->ev.re == r_released) || (e->ev.re == r_attempt)) {
        release_resources (rdl, resource, e->lwj);
        schedule_jobs (rdl, resource, p_queue);
        rc = 0;
    }

    return rc;
}


static int
action (flux_event_t *e)
{
    int rc = 0;

    switch (e->t) {
    case lwj_event:
        rc = action_j_event (e);
        break;

    case res_event:
        rc = action_r_event (e);
        break;

    default:
        flux_log (h, LOG_ERR, "unknown event type");
        break;
    }

    return rc;
}


/****************************************************************
 *
 *         Abstractions for KVS Callback Registeration
 *
 ****************************************************************/
static int
wait_for_lwj_init ()
{
    int rc = 0;
    kvsdir_t *dir = NULL;

    if (kvs_watch_once_dir (h, &dir, "lwj") < 0) {
        flux_log (h, LOG_ERR, "wait_for_lwj_init: %s",
                  strerror (errno));
        rc = -1;
        goto ret;
    }

    flux_log (h, LOG_DEBUG, "wait_for_lwj_init %s",
              kvsdir_key(dir));

ret:
    if (dir)
        kvsdir_destroy (dir);
    return rc;
}


static int
reg_newlwj_hdlr (kvs_set_int64_f func)
{
    if (kvs_watch_int64 (h,"lwj.next-id", func, (void *) h) < 0) {
        flux_log (h, LOG_ERR, "watch lwj.next-id: %s",
                  strerror (errno));
        return -1;
    }
    flux_log (h, LOG_DEBUG, "registered lwj creation callback");

    return 0;
}


static int
reg_lwj_state_hdlr (const char *path, kvs_set_string_f func)
{
    int rc = 0;
    char *k = NULL;

    asprintf (&k, "%s.state", path);
    if (kvs_watch_string (h, k, func, (void *)h) < 0) {
        flux_log (h, LOG_ERR,
                  "watch a lwj state in %s: %s.",
                  k, strerror (errno));
        rc = -1;
        goto ret;
    }
    flux_log (h, LOG_DEBUG, "registered lwj %s.state change callback", path);

ret:
    free (k);
    return rc;
}


/****************************************************************
 *                KVS Watch Callback Functions
 ****************************************************************/
static void
lwjstate_cb (const char *key, const char *val, void *arg, int errnum)
{
    int64_t lwj_id;
    flux_lwj_t *j = NULL;
    lwj_event_e e;

    if (errnum > 0) {
        /* Ignore ENOENT.  It is expected when this cb is called right
         * after registration.
         */
        if (errnum != ENOENT) {
            flux_log (h, LOG_ERR, "lwjstate_cb key(%s), val(%s): %s",
                      key, val, strerror (errnum));
        }
        goto ret;
    }

    if (extract_lwjid (key, &lwj_id) == -1) {
        flux_log (h, LOG_ERR, "ill-formed key");
        goto ret;
    }
    flux_log (h, LOG_DEBUG, "lwjstate_cb: %ld, %s", lwj_id, val);

    j = find_lwj (lwj_id);
    if (j) {
        e = stab_lookup (jobstate_tab, val);
        issue_lwj_event (e, j);
    } else
        flux_log (h, LOG_ERR, "lwjstate_cb: find_lwj %ld failed", lwj_id);

ret:
    return;
}

static int queue_kvs_cb (const char *key, const char *val, void *arg, int errnum)
{
	int key_len;
	int val_len;
	char *key_copy = NULL;
	char *val_copy = NULL;
	kvs_event_t *kvs_event = NULL;

	if (key != NULL){
		key_len = strlen (key) + 1;
		key_copy = (char *) malloc (sizeof (char) * key_len);
		strncpy (key_copy, key, key_len);
	}
	if (val != NULL){
		val_len = strlen (val) + 1;
		val_copy = (char *) malloc (sizeof (char) * val_len);
		strncpy (val_copy, val, val_len);
	}

	kvs_event = (kvs_event_t *) malloc (sizeof (kvs_event_t));
	kvs_event->errnum = errnum;
	kvs_event->key = key_copy;
	kvs_event->val = val_copy;
	flux_log (h, LOG_DEBUG, "Event queued - key: %s, val: %s", kvs_event->key, kvs_event->val);
	zlist_append (kvs_queue, kvs_event);

    return 0;
}

/*
//Compare two kvs events based on their state
//Return true if they should be swapped
//AKA item1 is further along than item2
static bool compare_kvs_events (void *item1, void *item2)
{
	int state1 = -1;
	int state2 = -1;
	int64_t id1 = -1;
	int64_t id2 = -1;
	kvs_event_t *event1 = (kvs_event_t *) item1;
	kvs_event_t *event2 = (kvs_event_t *) item2;

	if (event1->val != NULL)
		state1 = stab_lookup (jobstate_tab, event1->val);
	if (event2->val != NULL)
		state2 = stab_lookup (jobstate_tab, event2->val);

	if (state1 != state2)
		return state1 > state2;

	extract_lwjid (event1->key, &id1);
	extract_lwjid (event2->key, &id2);
	return id1 > id2;
}
*/

static void handle_kvs_queue ()
{
	kvs_event_t *kvs_event = NULL;
	//zlist_sort (kvs_queue, compare_kvs_events);
	while (zlist_size (kvs_queue) > 0){
		kvs_event = (kvs_event_t *) zlist_pop (kvs_queue);
		//flux_log (h, LOG_DEBUG, "Event to be handled - key: %s, val: %s", kvs_event->key, kvs_event->val);
		lwjstate_cb (kvs_event->key, kvs_event->val, NULL, kvs_event->errnum);
		free (kvs_event->key);
		free (kvs_event->val);
		free (kvs_event);
	}
}

/* The val argument is for the *next* job id.  Hence, the job id of
 * the new job will be (val - 1).
 */
static int
newlwj_cb (const char *key, int64_t val, void *arg, int errnum)
{
    char path[MAX_STR_LEN];
    flux_lwj_t *j = NULL;

    if (errnum > 0) {
        /* Ignore ENOENT.  It is expected when this cb is called right
         * after registration.
         */
        if (errnum != ENOENT) {
            flux_log (h, LOG_ERR, "newlwj_cb key(%s), val(%ld): %s",
                      key, val, strerror (errnum));
            goto error;
        }
        goto ret;
    } else if (val < 0) {
        flux_log (h, LOG_ERR, "newlwj_cb key(%s), val(%ld)", key, val);
        goto error;
    } else {
        flux_log (h, LOG_DEBUG, "newlwj_cb key(%s), val(%ld)", key, val);
    }

    if ( !(j = (flux_lwj_t *) xzmalloc (sizeof (flux_lwj_t))) ) {
        flux_log (h, LOG_ERR, "oom");
        goto error;
    }
	//j->lwj_id = val;
    j->lwj_id = val - 1;
    j->state = j_null;
    snprintf (path, MAX_STR_LEN, "lwj.%ld", j->lwj_id);
    if (zlist_append (p_queue, j) == -1) {
        flux_log (h, LOG_ERR,
                  "appending a job to pending queue failed");
        goto error;
    }
    if (reg_lwj_state_hdlr (path, queue_kvs_cb) == -1) {
        flux_log (h, LOG_ERR,
                  "register lwj state change "
                  "handling callback: %s",
                  strerror (errno));
        goto error;
    }
ret:
    return 0;

error:
	flux_log (h, LOG_ERR, "newlwj_cb failed");
    if (j)
        free (j);

    return 0;
}

static int newlwj_rpc (flux_t h, int typemask, zmsg_t **zmsg, void *arg)
{
	JSON o;
	JSON o_resp;
	const char *key;
	int64_t id;
	int rc = 0;

    if (flux_json_request_decode (*zmsg, &o) < 0
		    || !Jget_str (o, "key", &key)
		    || !Jget_int64 (o, "val", &id)) {
		flux_log (h, LOG_ERR, "%s: bad message", __FUNCTION__);
		Jput (o);
		rc = -1;
	}
	else {
		id = id + 1; //mimics the original kvs cb
		newlwj_cb (key, id, NULL, 0);
	}

	o_resp = Jnew ();
	Jadd_int (o_resp, "rc", rc);
    flux_json_respond (h, o_resp, zmsg);
	Jput (o_resp);

	if (o)
		Jput (o);
	zmsg_destroy (zmsg);

	return 0;
}

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

static int trigger_cb (flux_t h, int typemask, zmsg_t **zmsg, void *arg)
{
	JSON o = NULL;
	clock_t start, diff;
	double seconds;

    if (flux_json_request_decode (*zmsg, &o) < 0) {
		flux_log (h, LOG_ERR, "%s: bad message", __FUNCTION__);
		Jput (o);
		return -1;
	}

	flux_log (h, LOG_DEBUG, "Setting sim_state to new values");

	sim_state = json_to_sim_state (o);

	start = clock();

	handle_kvs_queue();
	event_cb (h, typemask, zmsg, arg);

	diff = clock() - start;
	seconds = ((double) diff) / CLOCKS_PER_SEC;
	sim_state->sim_time += seconds;

	handle_timer_queue();

	send_reply_request (h, sim_state);

	Jput (o);
	zmsg_destroy (zmsg);
	return 0;
}

static int send_join_request(flux_t h)
{
	JSON o = Jnew ();
	Jadd_str (o, "mod_name", module_name);
	Jadd_int (o, "rank", flux_rank (h));
	Jadd_double (o, "next_event", -1);
    if (flux_json_request (h, FLUX_NODEID_ANY,
                              FLUX_MATCHTAG_NONE, "sim.join", o) < 0) {
		Jput (o);
		return -1;
	}
	Jput (o);
	return 0;
}

//Received an event that a simulation is starting
static int start_cb (flux_t h, int typemask, zmsg_t **zmsg, void *arg)
{
	flux_log(h, LOG_DEBUG, "received a start event");
	if (send_join_request (h) < 0){
		flux_log (h, LOG_ERR, "submit module failed to register with sim module");
		return -1;
	}

	flux_log (h, LOG_DEBUG, "sent a join request");

	//Turn off normal functionality, switch to sim_mode
	in_sim = true;
	if (flux_event_unsubscribe (h, "sim.start") < 0){
		flux_log (h, LOG_ERR, "failed to unsubscribe from \"sim.start\"");
		return -1;
	} else {
		flux_log (h, LOG_DEBUG, "unsubscribed from \"sim.start\"");
	}
	if (flux_event_unsubscribe (h, "sim_sched.event") < 0){
		flux_log (h, LOG_ERR, "failed to unsubscribe from \"sim_sched.event\"");
		return -1;
	} else {
		flux_log (h, LOG_DEBUG, "unsubscribed from \"sim_sched.event\"");
	}

	//Cleanup
	zmsg_destroy (zmsg);

	return 0;
}

/****************************************************************
 *
 *        High Level Job and Resource Event Handlers
 *
 ****************************************************************/
static msghandler_t htab[] = {
    { FLUX_MSGTYPE_EVENT,   "sim.start",     start_cb },
    { FLUX_MSGTYPE_REQUEST, "sim_sched.trigger", trigger_cb },
    { FLUX_MSGTYPE_EVENT,   "sim_sched.event",   event_cb },
    { FLUX_MSGTYPE_REQUEST, "sim_sched.lwj-watch",  newlwj_rpc },
};

const int htablen = sizeof (htab) / sizeof (htab[0]);

int mod_main (flux_t p, int argc, char **argv)
{
    int i, rc = 0;
    char *path = NULL;
    struct rdllib *l = NULL;
    struct resource *r = NULL;

    h = p;
    if (flux_rank (h) != 0) {
        flux_log (h, LOG_ERR, "sim_ched module must only run on rank 0");
        rc = -1;
        goto ret;
    }
    flux_log (h, LOG_INFO, "sim_sched comms module starting");

    for (i = 0; i < argc; i++) {
        if (!strncmp ("rdl-conf=", argv[i], sizeof ("rdl-conf")))
            path = xstrdup (strstr (argv[i], "=") + 1);
        else if (!strncmp ("rdl-resource=", argv[i], sizeof ("rdl-resource")))
            resource = xstrdup (strstr (argv[i], "=") + 1);
        else {
            flux_log (h, LOG_ERR, "module load option %s invalid", argv[i]);
            goto ret;
        }
    }
    if (!path) {
        flux_log (h, LOG_ERR, "rdl-conf argument is not set");
        rc = -1;
        goto ret;
    }
    //setup_rdl_lua ();
    if (!(l = rdllib_open ()) || !(rdl = rdl_loadfile (l, path))) {
        flux_log (h, LOG_ERR, "failed to load resources from %s: %s",
                  path, strerror (errno));
        rc = -1;
        goto ret;
    }
    if (!resource) {
        flux_log (h, LOG_INFO, "using default rdl resource");
        resource = "default";
    }

    if ((r = rdl_resource_get (rdl, resource))) {
        if (idlize_resources (r)) {
            flux_log (h, LOG_ERR, "failed to idlize %s: %s", resource,
                      strerror (errno));
            rc = -1;
            goto ret;
        }
    } else {
        flux_log (h, LOG_ERR, "failed to get %s: %s", resource,
                  strerror (errno));
        rc = -1;
        goto ret;
    }

    p_queue = zlist_new ();
    r_queue = zlist_new ();
    c_queue = zlist_new ();
    ev_queue = zlist_new ();
	kvs_queue = zlist_new ();
	timer_queue = zlist_new ();
    if (!p_queue || !r_queue || !c_queue || !ev_queue) {
        flux_log (h, LOG_ERR,
                  "init for queues failed: %s",
                  strerror (errno));
        rc = -1;
        goto ret;
    }
	if (flux_event_subscribe (h, "sim.start") < 0){
        flux_log (h, LOG_ERR, "subscribing to event: %s", strerror (errno));
		return -1;
	}
    if (flux_event_subscribe (h, "sim_sched.event") < 0) {
        flux_log (h, LOG_ERR,
                  "subscribing to event: %s",
                  strerror (errno));
        rc = -1;
        goto ret;
    }
	if (flux_msghandler_addvec (h, htab, htablen, NULL) < 0) {
		flux_log (h, LOG_ERR, "flux_msghandler_add: %s", strerror (errno));
		return -1;
	}

	goto skip_for_sim;

    if (wait_for_lwj_init () == -1) {
        flux_log (h, LOG_ERR, "wait for lwj failed: %s",
                  strerror (errno));
        rc = -1;
        goto ret;
    }

    if (reg_newlwj_hdlr (newlwj_cb) == -1) {
        flux_log (h, LOG_ERR,
                  "register new lwj handling "
                  "callback: %s",
                  strerror (errno));
        rc = -1;
        goto ret;
    }

skip_for_sim:
	send_alive_request (h, module_name);

    if (flux_reactor_start (h) < 0) {
        flux_log (h, LOG_ERR,
                  "flux_reactor_start: %s",
                  strerror (errno));
        rc =  -1;
        goto ret;
    }

    zlist_destroy (&p_queue);
    zlist_destroy (&r_queue);
    zlist_destroy (&c_queue);
    zlist_destroy (&ev_queue);
    zlist_destroy (&kvs_queue);
    zlist_destroy (&timer_queue);

    rdllib_close(l);

ret:
    free(path);
    return rc;
}

MOD_NAME ("sim_sched");

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

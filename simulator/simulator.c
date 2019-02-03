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

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <libgen.h>
#include <dlfcn.h>
#include <flux/core.h>

#include "src/common/libutil/log.h"
#include "src/common/libutil/shortjansson.h"
#include "src/common/libutil/xzmalloc.h"
#include "simulator.h"

sim_state_t *new_simstate ()
{
    sim_state_t *sim_state = (sim_state_t *)malloc (sizeof (sim_state_t));
    sim_state->timers = zhash_new ();
    sim_state->sim_time = 0;
    return sim_state;
}

void free_simstate (sim_state_t *sim_state)
{
    if (sim_state != NULL) {
        if (sim_state->timers != NULL) {
            zhash_destroy (&(sim_state->timers));
        }
        free (sim_state);
    }
}

static int add_timers_to_json (const char *key, double *event_time, json_t *o)
{
    if (event_time != NULL)
        Jadd_double (o, key, *event_time);
    else
        Jadd_double (o, key, -1);
    return 0;
}

json_t *sim_state_to_json (sim_state_t *sim_state)
{
    json_t *o = Jnew ();
    json_t *event_timers = Jnew ();

    void *item = NULL;
    const char *key = NULL;
    for (item = zhash_first (sim_state->timers);
         item;
         item = zhash_next (sim_state->timers)) {
        key = zhash_cursor (sim_state->timers);
        add_timers_to_json (key, item, event_timers);
    }

    // build the main json obg
    Jadd_double (o, "sim_time", sim_state->sim_time);
    Jadd_obj (o, "event_timers", event_timers);

    Jput (event_timers);
    return o;
}

static void add_timers_to_hash (json_t *o, zhash_t *hash)
{
    json_t *value;
    const char *key;
    double *event_time;
    json_object_foreach (o, key, value) {
        event_time = (double *)malloc (sizeof (double));
        *event_time = json_real_value (value);

        // Insert key,value pair into sim_state hashtable
        zhash_insert (hash, key, event_time);
    }
}

sim_state_t *json_to_sim_state (json_t *o)
{
    sim_state_t *sim_state = new_simstate ();
    json_t *event_timers;

    Jget_double (o, "sim_time", &sim_state->sim_time);
    if (Jget_obj (o, "event_timers", &event_timers)) {
        add_timers_to_hash (event_timers, sim_state->timers);
    }

    return sim_state;
}

void free_job (job_t *job)
{
    free (job->user);
    free (job->jobname);
    free (job->account);
    free (job);
}

job_t *blank_job ()
{
    job_t *job = malloc (sizeof (job_t));
    job->id = -1;
    job->user = NULL;
    job->jobname = NULL;
    job->account = NULL;
    job->submit_time = 0;
    job->start_time = 0;
    job->execution_time = 0;
    job->io_time = 0;
    job->time_limit = 0;
    job->nnodes = 0;
    job->ncpus = 0;
    job->ngpus= 0;
    job->kvs_dir = NULL;
    return job;
}

/* Helper for put_job_in_kvs()
 */
static int txn_dir_pack (flux_kvs_txn_t *txn, flux_kvsdir_t *dir,
		         const char *name, const char *fmt, ...)
{
    va_list ap;
    char *key;
    int rc;

    if (!(key = flux_kvsdir_key_at (dir, name)))
        return -1;

    va_start (ap, fmt);
    rc = flux_kvs_txn_vpack (txn, 0, key, fmt, ap);
    va_end (ap);

    free (key);
    return rc;
}

int put_job_in_kvs (job_t *job, const char *initial_state)
{
    if (job->kvs_dir == NULL)
        return (-1);

    flux_t *h = flux_kvsdir_handle (job->kvs_dir);
    flux_kvs_txn_t *txn;
    flux_future_t *f = NULL;

    if (!(txn = flux_kvs_txn_create ()))
        goto error;
    if (txn_dir_pack (txn, job->kvs_dir, "user", "s", job->user) < 0)
        goto error;
    if (txn_dir_pack (txn, job->kvs_dir, "jobname", "s", job->jobname) < 0)
        goto error;
    if (txn_dir_pack (txn, job->kvs_dir, "account", "s", job->account) < 0)
        goto error;
    if (txn_dir_pack (txn, job->kvs_dir, "submit_time", "f",
                      job->submit_time) < 0)
        goto error;
    if (txn_dir_pack (txn, job->kvs_dir, "execution_time", "f",
                      job->execution_time) < 0)
        goto error;
    if (txn_dir_pack (txn, job->kvs_dir, "time_limit", "f",
                      job->time_limit) < 0)
        goto error;
    if (txn_dir_pack (txn, job->kvs_dir, "nnodes", "i", job->nnodes) < 0)
        goto error;
    if (txn_dir_pack (txn, job->kvs_dir, "ncpus", "i", job->ncpus) < 0)
        goto error;
    if (txn_dir_pack (txn, job->kvs_dir, "io_rate", "I", job->io_rate) < 0)
        goto error;
    if (txn_dir_pack (txn, job->kvs_dir, "state", "s", initial_state) < 0)
        goto error;
    if (!(f = flux_kvs_commit (h, NULL, 0, txn))
        || flux_future_get (f, NULL) < 0)
        goto error;
    flux_kvs_txn_destroy (txn);
    flux_future_destroy (f);
    return (0);
error:
    flux_log_error (h, "%s", __FUNCTION__);
    flux_kvs_txn_destroy (txn);
    flux_future_destroy (f);
    return (-1);
}

int set_job_timestamps (flux_kvsdir_t *dir, double t_starting,
                        double t_running, double t_completing,
                        double t_complete, double t_io)
{
    flux_t *h = flux_kvsdir_handle (dir);
    flux_kvs_txn_t *txn;
    flux_future_t *f = NULL;

    if (!(txn = flux_kvs_txn_create ()))
        goto error;
    if (t_starting != SIM_TIME_NONE) {
        if (txn_dir_pack (txn, dir, "starting_time", "f", t_starting) < 0)
	    goto error;
    }
    if (t_running != SIM_TIME_NONE) {
        if (txn_dir_pack (txn, dir, "running_time", "f", t_running) < 0)
	    goto error;
    }
    if (t_completing != SIM_TIME_NONE) {
        if (txn_dir_pack (txn, dir, "completing_time", "f", t_completing) < 0)
	    goto error;
    }
    if (t_complete != SIM_TIME_NONE) {
        if (txn_dir_pack (txn, dir, "complete_time", "f", t_complete) < 0)
	    goto error;
    }
    if (t_io != SIM_TIME_NONE) {
        if (txn_dir_pack (txn, dir, "io_time", "f", t_io) < 0)
	    goto error;
    }
    if (!(f = flux_kvs_commit (h, NULL, 0, txn))
        || flux_future_get (f, NULL) < 0)
        goto error;
    flux_kvs_txn_destroy (txn);
    flux_future_destroy (f);
    return 0;
error:
    flux_log_error (h, "%s", __FUNCTION__);
    flux_kvs_txn_destroy (txn);
    flux_future_destroy (f);
    return (-1);
}

/* Helper for pull_job_from_kvs(), while KVS API is anemic with respect
 * to flux_kvsdir_t.
 * N.B. this function only works on scalar types
 */
static int lookup_dir_unpack (flux_kvsdir_t *dir, const char *name,
                       const char *fmt, ...)
{
    flux_t *h = flux_kvsdir_handle (dir);
    flux_future_t *f = NULL;
    char *key = NULL;
    json_t *obj = NULL;
    const char *json_str;
    va_list ap;
    int rc = -1;

    if (strchr (fmt, 's') || strchr (fmt, 'o') || strchr (fmt, 'O'))
        goto inval;
    if (!(key = flux_kvsdir_key_at (dir, name)))
        goto done;
    if (!(f = flux_kvs_lookup (h, NULL, 0, key)))
        goto done;
    if (flux_kvs_lookup_get (f, &json_str) < 0)
        goto done;
    if (!(obj = json_loads (json_str, JSON_DECODE_ANY, NULL)))
        goto inval;
    va_start (ap, fmt);
    rc = json_vunpack_ex (obj, NULL, 0, fmt, ap);
    va_end (ap);
    if (rc < 0)
        goto inval;
    rc = 0;
inval:
    errno = EINVAL;
done:
    json_decref (obj); // N.B. destroys any returned string, object, array
    free (key);
    flux_future_destroy (f);
    return rc;
}

/* Helper for pull_job_from_kvs(), while KVS API is anemic with respect
 * to flux_kvsdir_t.
 */
static int lookup_dir_string (flux_kvsdir_t *dir, const char *name, char **valp)
{
    flux_t *h = flux_kvsdir_handle (dir);
    flux_future_t *f = NULL;
    const char *s;
    char *key;
    char *cpy;
    int rc = -1;

    if (!(key = flux_kvsdir_key_at (dir, name)))
        goto done;
    if (!(f = flux_kvs_lookup (h, NULL, 0, key)))
        goto done;
    if (flux_kvs_lookup_get_unpack (f, "s", &s) < 0)
        goto done;
    if (!(cpy = strdup (s))) {
        errno = ENOMEM;
        goto done;
    }
    *valp = cpy;
    rc = 0;
done:
    free (key);
    flux_future_destroy (f);
    return rc;
}

job_t *pull_job_from_kvs (int id, flux_kvsdir_t *kvsdir)
{
    if (kvsdir == NULL)
        return NULL;

    flux_t *h = flux_kvsdir_handle (kvsdir);
    job_t *job = blank_job ();

    job->kvs_dir = kvsdir;
    job->id = id;

    if (lookup_dir_string (job->kvs_dir, "user", &job->user) < 0)
        goto error;
    if (lookup_dir_string (job->kvs_dir, "jobname", &job->jobname) < 0)
        goto error;
    if (lookup_dir_string (job->kvs_dir, "account", &job->account) < 0)
        goto error;
    if (lookup_dir_unpack (job->kvs_dir, "submit_time", "f",
                           &job->submit_time) < 0)
        goto error;
    if (lookup_dir_unpack (job->kvs_dir, "execution_time", "f",
                           &job->execution_time) < 0)
        goto error;
    if (lookup_dir_unpack (job->kvs_dir, "time_limit", "f",
                           &job->time_limit) < 0)
        goto error;
    if (lookup_dir_unpack (job->kvs_dir, "nnodes", "i", &job->nnodes) < 0)
        goto error;
    if (lookup_dir_unpack (job->kvs_dir, "ncpus", "i", &job->ncpus) < 0)
        goto error;
    if (lookup_dir_unpack (job->kvs_dir, "io_rate", "I", &job->io_rate) < 0)
        goto error;

    /* Not set in put_job_in_kvs().
     * Allow them to not be set here without error.
     */
    (void)lookup_dir_unpack (job->kvs_dir, "starting_time", "f",
		             &job->start_time);
    (void)lookup_dir_unpack (job->kvs_dir, "io_time", "f", &job->io_time);

    return job;
error:
    flux_log_error (h, "%s", __FUNCTION__);
    return NULL;
}

int send_alive_request (flux_t *h, const char *module_name)
{
    int rc = 0;
    flux_future_t *future = NULL;
    json_t *o = Jnew ();
    uint32_t rank;

    if (flux_get_rank (h, &rank) < 0)
        return -1;

    Jadd_str (o, "mod_name", module_name);
    Jadd_int (o, "rank", rank);

    future = flux_rpc (h, "sim.alive", Jtostr (o), FLUX_NODEID_ANY, FLUX_RPC_NORESPONSE);
    if (!future) {
        rc = -1;
    }

    Jput (o);
    flux_future_destroy (future);

    return rc;
}

// Reply back to the sim module with the updated sim state (in JSON form)
int send_reply_request (flux_t *h,
                        const char *module_name,
                        sim_state_t *sim_state)
{
    int rc = 0;
    flux_future_t *future = NULL;
    json_t *o = NULL;

    o = sim_state_to_json (sim_state);
    Jadd_str (o, "mod_name", module_name);

    future = flux_rpc (h, "sim.reply", Jtostr (o), FLUX_NODEID_ANY, FLUX_RPC_NORESPONSE);
    if (!future) {
        rc = -1;
    }

    flux_log (h, LOG_DEBUG, "sent a reply request: %s", Jtostr (o));
    Jput (o);
    flux_future_destroy(future);
    return rc;
}

// Request to join the simulation
int send_join_request (flux_t *h, const char *module_name, double next_event)
{
    int rc = 0;
    flux_future_t *future = NULL;
    json_t *o = Jnew ();
    uint32_t rank;

    if (flux_get_rank (h, &rank) < 0)
        return -1;

    Jadd_str (o, "mod_name", module_name);
    Jadd_int (o, "rank", rank);
    Jadd_double (o, "next_event", next_event);

    future = flux_rpc (h, "sim.join", Jtostr (o), FLUX_NODEID_ANY, FLUX_RPC_NORESPONSE);
    if (!future) {
        rc = -1;
    }

    Jput (o);
    flux_future_destroy (future);
    return rc;
}

zhash_t *zhash_fromargv (int argc, char **argv)
{
    zhash_t *args = zhash_new ();
    int i;

    if (args) {
        for (i = 0; i < argc; i++) {
            char *key = strdup (argv[i]);
            char *val = strchr (key, '=');
            if (val) {
                *val++ = '\0';
                zhash_update (args, key, strdup (val));
                zhash_freefn (args, key, free);
            }
            free (key);
        }
    }
    return args;
}

static int job_kvspath (flux_t *h, int jobid, char **path)
{
    flux_future_t *f;
    const char *s;
    char *cpy;
    int rc = -1;

    if (!(f = flux_rpc_pack (h, "job.kvspath", FLUX_NODEID_ANY, 0,
                             "{s:[i]}", "ids", jobid)))
        goto done;
    if (flux_rpc_get_unpack (f, "{s:[s]}", "paths", &s) < 0)
        goto done;
    if (!(cpy = strdup (s)))
        goto done;
    *path = cpy;
    rc = 0;
done:
    flux_future_destroy (f);
    return rc;
}

// Get a kvsdir handle to jobid [jobid] using job.kvspath service.
//  If service doesn't answer, fall back to `lwj.%d`
flux_kvsdir_t *job_kvsdir (flux_t *h, int jobid)
{
    char *path = NULL;
    const flux_kvsdir_t *dir;
    flux_kvsdir_t *cpy = NULL;
    flux_future_t *f = NULL;

    if (job_kvspath (h, jobid, &path) < 0) {
        if (asprintf (&path, "lwj.%d", jobid) < 0) {
            flux_log_error (h, "%s", __FUNCTION__);
            goto done;
        }
        flux_log (h, LOG_DEBUG,
                  "%s: failed to resolve job directory, falling back to %s",
                  __FUNCTION__, path);
    }
    if (!(f = flux_kvs_lookup (h, NULL, FLUX_KVS_READDIR, path))
		    || flux_kvs_lookup_get_dir (f, &dir) < 0) {
        flux_log_error (h, "%s: flux_kvs_lookup %s", __FUNCTION__, path);
	goto done;
    }
    if (!(cpy = flux_kvsdir_copy (dir)))
        goto done;
done:
    free (path);
    flux_future_destroy (f);
    return cpy;
}

/*
 * vi: ts=4 sw=4 expandtab
 */

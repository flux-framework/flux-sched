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
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <flux/core.h>

#include "src/common/libutil/log.h"
#include "src/common/libutil/shortjansson.h"
#include "src/common/libutil/xzmalloc.h"
#include "simulator.h"

static const char *module_name = "submit";
static zlist_t *jobs;  // TODO: remove from "global" scope

#if CZMQ_VERSION < CZMQ_MAKE_VERSION(3, 0, 1)
// Compare two job_t's based on submit time
// Return true if they should be swapped
// AKA job1 was submitted after job2
bool compare_job_t (void *item1, void *item2)
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
int compare_job_t (void *x, void *y)
{
    double t1 = ((job_t *)x)->submit_time;
    double t2 = ((job_t *)y)->submit_time;
    double delta = t1 - t2;
    if (fabs (delta) < DBL_EPSILON)
        return 0;
    else if (delta > 0)
        return 1;
    else
        return (-1);
}
#endif /* CZMQ_VERSION > 3.0.0 */

// Figure out when the next submit time is
// This assumes the list is sorted by submit time
double get_next_submit_time ()
{
    job_t *job;
    if (zlist_size (jobs) > 0) {
        job = zlist_head (jobs);
        return job->submit_time;
    }
    return -1;
}

// Convert the string representation of time in the csv to sec
double convert_time_to_sec (char *time)
{
    int hours, minutes, seconds;
    sscanf (time, "%d:%d:%d", &hours, &minutes, &seconds);
    return (double)((hours * 3600) + (minutes * 60) + seconds);
}

// Populate a field in the job_t based off a value extracted from the csv
int insert_into_job (flux_t *h, job_t *job, char *column_name, char *value)
{
    if (!strcmp (column_name, "JobID")) {
        job->id = atoi (value);
    } else if (!strcmp (column_name, "User")) {
        job->user = xasprintf ("%s", value);
    } else if (!strcmp (column_name, "JobName")) {
        job->jobname = xasprintf ("%s", value);
    } else if (!strcmp (column_name, "Account")) {
        job->account = xasprintf ("%s", value);
    } else if (!strcmp (column_name, "NNodes")) {
        job->nnodes = atoi (value);
    } else if (!strcmp (column_name, "NCPUS")) {
        job->ncpus = atoi (value);
    } else if (!strcmp (column_name, "Timelimit")) {
        job->time_limit = convert_time_to_sec (value);
    } else if (!strcmp (column_name, "Submit")) {
        char *endptr;
        struct tm tm_spec; 
        double stime = strtod(value, &endptr);
        // Check if you parsed only a bit of the string (e.g. just the year)
        // Trailing whitespace is not an error.
        if (*endptr != '\0' && *endptr != ' ' && *endptr != '\t') {
            endptr = strptime(value, "%Y-%m-%dT%H:%M:%S", &tm_spec);
            if (endptr == NULL) {
                endptr = value;
            } else {
                stime = (double) mktime(&tm_spec);
            }
        }
        // endptr gets set by strtod or by strptime
        if (endptr == value) {
            flux_log (h, LOG_WARNING, "Incorrect Submit format, expects %s"
                        " or seconds since epoch; replacing '%s' with %f",
                        "%Y-%m-%dT%H:%M:%S (yyyy-mm-ddThh:mm:ss)",
                        value, stime);
        }
        job->submit_time = stime;
    } else if (!strcmp (column_name, "Elapsed")) {
        job->execution_time = convert_time_to_sec (value);
    } else if (!strncmp (column_name,
                         "IORate(MB)",
                         10)) {  // ignore the \n at the end using strncmp
        job->io_rate = atol (value);
    }
    return 0;
}

// Take the header string from the csv and tokenize it based on the ","
// Then insert each column name into a zlist
int populate_header (char *header_line, zlist_t *header_list)
{
    char *token_copy, *token;
    token = strtok (header_line, ",");
    while (token != NULL) {
        token_copy = xasprintf ("%s", token);
        zlist_append (header_list, token_copy);
        token = strtok (NULL, ",");
    }
    return 0;
}

// Populate a list of jobs using the data contained in the csv
// TODO: breakup this function into several smaller functions
int parse_job_csv (flux_t *h, char *filename, zlist_t *jobs)
{
    const int MAX_LINE_LEN = 500;  // sort of arbitrary, works for my data
    char curr_line[MAX_LINE_LEN];  // current line of the input file
    zlist_t *header = NULL;  // column names
    char *fget_rc = NULL;  // stores the return code of fgets
    char *token = NULL;  // current token from the current line
    char *curr_column = NULL;  // current column name (pulled from header)
    job_t *curr_job = NULL;  // current job object that we are populating

    FILE *fp = fopen (filename, "r");
    if (fp == NULL) {
        flux_log (h, LOG_ERR, "csv failed to open");
        return -1;
    }

    // Populate the header list of column names
    header = zlist_new ();
    fget_rc = fgets (curr_line, MAX_LINE_LEN, fp);
    if (fget_rc != NULL && feof (fp) == 0) {
        populate_header (curr_line, header);
        fget_rc = fgets (curr_line, MAX_LINE_LEN, fp);
        curr_column = zlist_first (header);
    } else {
        flux_log (h, LOG_ERR, "header not found");
    }

    // Start making jobs from the actual data
    while (fget_rc != NULL && feof (fp) == 0) {
        curr_job = blank_job ();
        token = strtok (curr_line, ",");
        // Walk through even column in record and insert the data into the job
        while (token != NULL) {
            if (curr_column == NULL) {
                flux_log (h, LOG_ERR, "column name is NULL");
                return -1;
            }
            insert_into_job (h, curr_job, curr_column, token);
            token = strtok (NULL, ",");
            curr_column = zlist_next (header);
        }
        zlist_append (jobs, curr_job);
        fget_rc = fgets (curr_line, MAX_LINE_LEN, fp);
        curr_column = zlist_first (header);
        if (curr_line[0] == '#')  // reached a comment line, stop processing
                                  // file
            break;
    }
    zlist_sort (jobs, compare_job_t);

    // Cleanup
    while (zlist_size (header) > 0)
        free (zlist_pop (header));
    zlist_destroy (&header);
    if (fclose (fp) != 0) {
        flux_log (h, LOG_ERR, "csv file failed to close");
        return -1;
    }
    return 0;
}

// Based on the sim_time, schedule any jobs that need to be scheduled
// Next, add an event timer for the scheduler to the sim_state
// Finally, updated the submit event timer with the next submit time
int schedule_next_job (flux_t *h, sim_state_t *sim_state)
{
    flux_future_t *f = NULL;
    flux_msg_t *msg = NULL;
    flux_kvsdir_t *dir = NULL;
    job_t *job = NULL;
    char *kvs_path = NULL;
    int64_t new_jobid = -1;
    double *new_sched_mod_time = NULL, *new_submit_mod_time = NULL;

    zhash_t *timers = sim_state->timers;

    // Get the next job to submit
    // Then craft a "job.create" from the job_t and wait for jobid in response
    job = zlist_pop (jobs);
    if (job == NULL) {
        flux_log (h, LOG_DEBUG, "no more jobs to submit");
        new_submit_mod_time = (double *)zhash_lookup (timers, module_name);
        *new_submit_mod_time = -1;
        return -1;
    }

    f = flux_rpc_pack (h, "job.create", FLUX_NODEID_ANY, 0,
                     "{ s:i s:i s:I }",
                     "nnodes", job->nnodes,
                     "ntasks", job->ncpus,
                     "walltime", (int64_t)job->time_limit);
    if (f == NULL) {
        flux_log (h, LOG_ERR, "%s: %s", __FUNCTION__, strerror (errno));
        return -1;
    }
    if (flux_rpc_get_unpack (f, "{ s:I s:s }",
                            "jobid", &new_jobid,
                            "kvs_path", &kvs_path) < 0) {
        flux_log (h, LOG_ERR, "%s: %s", __FUNCTION__, strerror (errno));
        flux_future_destroy (f);
        return -1;
    }
    flux_future_destroy (f);

    // Update lwj.%jobid%'s state in the kvs to "submitted"
    if (!(dir = job_kvsdir (h, new_jobid)))
        log_err_exit ("kvs_get_dir (id=%"PRId64")", new_jobid);
    job->kvs_dir = dir;
    if (put_job_in_kvs (job, "submitted") < 0)
        log_err_exit ("put_job_in_kvs");

    // Send "submitted" event
    if (!(msg = flux_event_pack ("wreck.state.submitted",
                                 "{ s:I s:s s:i s:i s:i}",
                                 "lwj", new_jobid,
                                 "kvs_path", kvs_path,
                                 "nnodes", (int)job->nnodes,
                                 "ntasks", (int)job->ncpus,
                                 "walltime", (int)job->time_limit))
        || flux_send (h, msg, 0) < 0) {
        return -1;
    }
    flux_msg_destroy (msg);

    // Update event timers in reply (submit and sched)
    new_sched_mod_time = (double *)zhash_lookup (timers, "sched");
    if (new_sched_mod_time != NULL)
        *new_sched_mod_time = sim_state->sim_time + .00001;
    flux_log (h,
              LOG_DEBUG,
              "added a sched timer that will occur at %f",
              *new_sched_mod_time);
    new_submit_mod_time = (double *)zhash_lookup (timers, module_name);
    if (get_next_submit_time () > *new_sched_mod_time)
        *new_submit_mod_time = get_next_submit_time ();
    else
        *new_submit_mod_time = *new_sched_mod_time + .0001;
    flux_log (h, LOG_INFO, "submitted job %"PRId64" (%d in csv)", new_jobid, job->id);
    flux_log (h,
              LOG_DEBUG,
              "next submit event will occur at %f",
              *new_submit_mod_time);

    // Cleanup
    free_job (job);
    return 0;
}

// Received an event that a simulation is starting
static void start_cb (flux_t *h,
                      flux_msg_handler_t *w,
                      const flux_msg_t *msg,
                      void *arg)
{
    flux_log (h, LOG_DEBUG, "received a start event");
    if (send_join_request (h, module_name, get_next_submit_time ()) < 0) {
        flux_log (h,
                  LOG_ERR,
                  "submit module failed to register with sim module");
        return;
    }
    flux_log (h, LOG_DEBUG, "sent a join request");

    if (flux_event_unsubscribe (h, "sim.start") < 0) {
        flux_log (h, LOG_ERR, "failed to unsubscribe from \"sim.start\"");
    } else {
        flux_log (h, LOG_DEBUG, "unsubscribed from \"sim.start\"");
    }
}

// Handle trigger requests from the sim module ("submit.trigger")
static void trigger_cb (flux_t *h,
                        flux_msg_handler_t *w,
                        const flux_msg_t *msg,
                        void *arg)
{
    json_t *o = NULL;
    const char *json_str = NULL;
    sim_state_t *sim_state = NULL;

    if (flux_msg_get_json (msg, &json_str) < 0 || json_str == NULL
        || !(o = Jfromstr (json_str))) {
        flux_log (h, LOG_ERR, "%s: bad message", __FUNCTION__);
        Jput (o);
        return;
    }

    // Logging
    flux_log (h,
              LOG_DEBUG,
              "received a trigger (submit.trigger): %s",
              json_str);

    // Handle the trigger
    sim_state = json_to_sim_state (o);
    schedule_next_job (h, sim_state);
    send_reply_request (h, module_name, sim_state);

    // Cleanup
    free_simstate (sim_state);
    Jput (o);
}

static const struct flux_msg_handler_spec htab[] = {
    {FLUX_MSGTYPE_EVENT, "sim.start", start_cb, 0},
    {FLUX_MSGTYPE_REQUEST, "submit.trigger", trigger_cb, 0},
    FLUX_MSGHANDLER_TABLE_END,
};

int mod_main (flux_t *h, int argc, char **argv)
{
    zhash_t *args = zhash_fromargv (argc, argv);
    if (!args)
        oom ();
    char *csv_filename;
    uint32_t rank;
    flux_msg_handler_t **handlers = NULL;
    int rc = -1;

    if (flux_get_rank (h, &rank) < 0)
        return -1;
    if (rank != 0) {
        flux_log (h, LOG_ERR, "submit module must only run on rank 0");
        return -1;
    }
    flux_log (h, LOG_INFO, "submit comms module starting");

    // Get the job data from the csv
    if (!(csv_filename = zhash_lookup (args, "job-csv"))) {
        flux_log (h, LOG_ERR, "job-csv argument is not set");
        return -1;
    }
    jobs = zlist_new ();
    parse_job_csv (h, csv_filename, jobs);
    flux_log (h, LOG_INFO, "submit comms module finished parsing job data");

    if (flux_event_subscribe (h, "sim.start") < 0) {
        flux_log (h, LOG_ERR, "subscribing to event: %s", strerror (errno));
        return -1;
    }
    if (flux_msg_handler_addvec (h, htab, NULL, &handlers) < 0) {
        flux_log (h, LOG_ERR, "flux_msg_handler_addvec: %s", strerror (errno));
        return -1;
    }

    send_alive_request (h, module_name);

    if (flux_reactor_run (flux_get_reactor (h), 0) < 0) {
        flux_log (h, LOG_ERR, "flux_reactor_run: %s", strerror (errno));
        goto done_delvec;
    }
    rc = 0;
done_delvec:
    flux_msg_handler_delvec (handlers);
    zhash_destroy (&args);
    return rc;
}

MOD_NAME ("submit");

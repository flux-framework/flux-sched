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
#include <flux/core.h>

#include "src/common/libutil/log.h"
#include "src/common/libutil/shortjson.h"
#include "src/common/libutil/xzmalloc.h"
#include "simulator.h"

typedef struct {
    sim_state_t *sim_state;
    flux_t h;
    bool rdl_changed;
    char *rdl_string;
    bool exit_on_complete;
} ctx_t;

static void freectx (void *arg)
{
    ctx_t *ctx = arg;
    free_simstate (ctx->sim_state);
    free (ctx->rdl_string);
    free (ctx);
}

static ctx_t *getctx (flux_t h, bool exit_on_complete)
{
    ctx_t *ctx = (ctx_t *)flux_aux_get (h, "simsrv");

    if (!ctx) {
        ctx = xzmalloc (sizeof (*ctx));
        ctx->h = h;
        ctx->sim_state = new_simstate ();
        ctx->rdl_string = NULL;
        ctx->rdl_changed = false;
        ctx->exit_on_complete = exit_on_complete;
        flux_aux_set (h, "simsrv", ctx, freectx);
    }

    return ctx;
}

// builds the trigger request and sends it to "mod_name"
// converts sim_state to JSON, formats request tag based on "mod_name"
static int send_trigger (flux_t h, char *mod_name, sim_state_t *sim_state)
{
    int rc = 0;
    flux_msg_t *msg = NULL;
    JSON o = NULL;
    char *topic = NULL;

    o = sim_state_to_json (sim_state);

    msg = flux_msg_create (FLUX_MSGTYPE_REQUEST);
    topic = xasprintf ("%s.trigger", mod_name);
    flux_msg_set_topic (msg, topic);
    flux_msg_set_payload_json (msg, Jtostr (o));
    if (flux_send (h, msg, 0) < 0) {
        flux_log (h, LOG_ERR, "failed to send trigger to %s", mod_name);
        rc = -1;
    }

    Jput (o);
    free (topic);
    return rc;
}

// Send out a call to all modules that the simulation is starting
// and that they should join
int send_start_event (flux_t h)
{
    int rc = 0;
    flux_msg_t *msg = NULL;
    uint32_t rank;

    if (flux_get_rank (h, &rank) < 0)
        return -1;

    JSON o = Jnew ();
    Jadd_str (o, "mod_name", "sim");
    Jadd_int (o, "rank", rank);
    Jadd_int (o, "sim_time", 0);

    if (!(msg = flux_event_encode ("sim.start", Jtostr (o)))
        || flux_send (h, msg, 0) < 0) {
        rc = -1;
    }

    Jput (o);
    flux_msg_destroy (msg);
    return rc;
}

// Send an event to all modules that the simulation has completed
int send_complete_event (flux_t h)
{
    int rc = 0;
    flux_msg_t *msg = NULL;

    if (!(msg = flux_event_encode ("sim.complete", NULL))
        || flux_send (h, msg, 0) < 0) {
        rc = -1;
    }

    flux_msg_destroy (msg);
    return rc;
}

// Looks at the current state and launches the next trigger
static int handle_next_event (ctx_t *ctx)
{
    zhash_t *timers;
    zlist_t *keys;
    sim_state_t *sim_state = ctx->sim_state;
    int rc = 0;

    // get the timer hashtable, make sure its full, and get a list of its keys
    timers = sim_state->timers;
    if (zhash_size (timers) < 1) {
        flux_log (ctx->h, LOG_ERR, "timer hashtable has no elements");
        return -1;
    }
    keys = zhash_keys (timers);

    // Get the next occuring event time/module
    double *min_event_time = NULL, *curr_event_time = NULL;
    char *mod_name = NULL, *curr_name = NULL;

    while (min_event_time == NULL && zlist_size (keys) > 0) {
        mod_name = zlist_pop (keys);
        min_event_time = (double *)zhash_lookup (timers, mod_name);
        if (*min_event_time < 0) {
            min_event_time = NULL;
            free (mod_name);
        }
    }
    if (min_event_time == NULL) {
        return -1;
    }
    while (zlist_size (keys) > 0) {
        curr_name = zlist_pop (keys);
        curr_event_time = (double *)zhash_lookup (timers, curr_name);
        if (*curr_event_time > 0
            && ((*curr_event_time < *min_event_time)
                || (*curr_event_time == *min_event_time
                    && !strcmp (curr_name, "sched")))) {
            free (mod_name);
            mod_name = curr_name;
            min_event_time = curr_event_time;
        }
    }

    // advance time then send the trigger to the module with the next event
    if (*min_event_time > sim_state->sim_time) {
        // flux_log (ctx->h, LOG_DEBUG, "Time was advanced from %f to %f while
        // triggering the next event for %s",
        //		  sim_state->sim_time, *min_event_time, mod_name);
        sim_state->sim_time = *min_event_time;
    } else {
        // flux_log (ctx->h, LOG_DEBUG, "Time was not advanced while triggering
        // the next event for %s", mod_name);
    }
    flux_log (ctx->h,
              LOG_DEBUG,
              "Triggering %s.  Curr sim time: %f",
              mod_name,
              sim_state->sim_time);

    *min_event_time = -1;
    rc = send_trigger (ctx->h, mod_name, sim_state);

    // clean up
    free (mod_name);
    zlist_destroy (&keys);
    return rc;
}

// Recevied a request to join the simulation ("sim.join")
static void join_cb (flux_t h,
                     flux_msg_handler_t *w,
                     const flux_msg_t *msg,
                     void *arg)
{
    int mod_rank;
    JSON request = NULL;
    const char *mod_name = NULL, *json_str = NULL;
    double *next_event = (double *)malloc (sizeof (double));
    ctx_t *ctx = arg;
    sim_state_t *sim_state = ctx->sim_state;
    uint32_t size;

    if (flux_msg_get_payload_json (msg, &json_str) < 0 || json_str == NULL
        || !(request = Jfromstr (json_str))
        || !Jget_str (request, "mod_name", &mod_name)
        || !Jget_int (request, "rank", &mod_rank)
        || !Jget_double (request, "next_event", next_event)) {
        flux_log (h, LOG_ERR, "%s: bad join message", __FUNCTION__);
        Jput (request);
        return;
    }
    if (flux_get_size (h, &size) < 0)
        return;
    if (mod_rank < 0 || mod_rank >= size) {
        Jput (request);
        flux_log (h, LOG_ERR, "%s: bad rank in join message", __FUNCTION__);
        return;
    }

    flux_log (h,
              LOG_DEBUG,
              "join rcvd from module %s on rank %d, next event at %f",
              mod_name,
              mod_rank,
              *next_event);

    zhash_t *timers = sim_state->timers;
    if (zhash_insert (timers, mod_name, next_event) < 0) {  // key already
                                                            // exists
        flux_log (h,
                  LOG_ERR,
                  "duplicate join request from %s, module already exists in "
                  "sim_state",
                  mod_name);
        return;
    }

    // TODO: this is horribly hackish, improve the handshake to avoid
    // this hardcoded # of modules. maybe use a timeout?  ZMQ provides
    // support for polling etc with timeouts, should try that
    static int num_modules = 3;
    num_modules--;
    if (num_modules <= 0) {
        if (handle_next_event (ctx) < 0) {
            flux_log (h, LOG_ERR, "failure while handling next event");
            return;
        }
    }

    Jput (request);
}

// Based on the simulation time, the previous timer value, and the
// reply timer value, try and determine what the new timer value
// should be.  There are ~12 possible cases captured by the ~7 if/else
// statements below.  The general idea is leave the timer alone if the
// reply = previous (echo'd back) and to use the reply time if the
// reply is > sim time but < prev. There are many permutations of the
// three paramters which result in an invalid state hopefully all of
// those cases are checked for and logged.

// TODO: verify all of this logic is correct, bugs could easily creep up here
static int check_for_new_timers (const char *key, void *item, void *argument)
{
    ctx_t *ctx = (ctx_t *)argument;
    sim_state_t *curr_sim_state = ctx->sim_state;
    double sim_time = curr_sim_state->sim_time;
    double *reply_event_time = (double *)item;
    double *curr_event_time =
        (double *)zhash_lookup (curr_sim_state->timers, key);

    if (*curr_event_time < 0 && *reply_event_time < 0) {
        // flux_log (ctx->h, LOG_DEBUG, "no timers found for %s, doing nothing",
        // key);
        return 0;
    } else if (*curr_event_time < 0) {
        if (*reply_event_time >= sim_time) {
            *curr_event_time = *reply_event_time;
            // flux_log (ctx->h, LOG_DEBUG, "change in timer accepted for %s",
            // key);
            return 0;
        } else {
            flux_log (ctx->h, LOG_ERR, "bad reply timer for %s", key);
            return -1;
        }
    } else if (*reply_event_time < 0) {
        flux_log (ctx->h, LOG_ERR, "event timer deleted from %s", key);
        return -1;
    } else if (*reply_event_time < sim_time
               && *curr_event_time != *reply_event_time) {
        flux_log (ctx->h,
                  LOG_ERR,
                  "incoming modified time is before sim time for %s",
                  key);
        return -1;
    } else if (*reply_event_time >= sim_time
               && *reply_event_time < *curr_event_time) {
        *curr_event_time = *reply_event_time;
        // flux_log (ctx->h, LOG_DEBUG, "change in timer accepted for %s", key);
        return 0;
    } else {
        // flux_log (ctx->h, LOG_DEBUG, "no changes made to %s timer, curr_time:
        // %f\t reply_time: %f", key, *curr_event_time, *reply_event_time);
        return 0;
    }
}

static void copy_new_state_data (ctx_t *ctx,
                                 sim_state_t *curr_sim_state,
                                 sim_state_t *reply_sim_state)
{
    if (reply_sim_state->sim_time > curr_sim_state->sim_time) {
        curr_sim_state->sim_time = reply_sim_state->sim_time;
    }

    zhash_foreach (reply_sim_state->timers, check_for_new_timers, ctx);
}

static void rdl_update_cb (flux_t h,
                           flux_msg_handler_t *w,
                           const flux_msg_t *msg,
                           void *arg)
{
    JSON o = NULL;
    const char *json_str = NULL, *rdl_str = NULL;
    ctx_t *ctx = (ctx_t *)arg;

    if (flux_msg_get_payload_json (msg, &json_str) < 0 || json_str == NULL
        || !(o = Jfromstr (json_str))) {
        flux_log (h, LOG_ERR, "%s: bad message", __FUNCTION__);
        Jput (o);
        return;
    }

    Jget_str (o, "rdl_string", &rdl_str);
    if (rdl_str) {
        free (ctx->rdl_string);
        ctx->rdl_string = strdup (rdl_str);
        ctx->rdl_changed = true;
    }

    Jput (o);
}

// Recevied a reply to a trigger ("sim.reply")
static void reply_cb (flux_t h,
                      flux_msg_handler_t *w,
                      const flux_msg_t *msg,
                      void *arg)
{
    const char *json_str = NULL;
    JSON request = NULL;
    ctx_t *ctx = arg;
    sim_state_t *curr_sim_state = ctx->sim_state;
    sim_state_t *reply_sim_state;

    if (flux_msg_get_payload_json (msg, &json_str) < 0 || json_str == NULL
        || !(request = Jfromstr (json_str))) {
        flux_log (h, LOG_ERR, "%s: bad reply message", __FUNCTION__);
        Jput (request);
        return;
    }

    // De-serialize and get new info
    reply_sim_state = json_to_sim_state (request);
    copy_new_state_data (ctx, curr_sim_state, reply_sim_state);

    if (handle_next_event (ctx) < 0) {
        flux_log (h, LOG_DEBUG, "No events remaining");
        if (ctx->exit_on_complete) {
            log_msg_exit ("exit_on_complete is set. Exiting now.");
        } else {
            send_complete_event (h);
        }
    }

    free_simstate (reply_sim_state);
    Jput (request);
}

static void alive_cb (flux_t h,
                      flux_msg_handler_t *w,
                      const flux_msg_t *msg,
                      void *arg)
{
    const char *json_str;

    if (flux_msg_get_payload_json (msg, &json_str) < 0 || json_str == NULL) {
        flux_log (h, LOG_ERR, "%s: bad reply message", __FUNCTION__);
        return;
    }

    flux_log (h, LOG_DEBUG, "received alive request - %s", json_str);

    if (send_start_event (h) < 0) {
        flux_log (h, LOG_ERR, "sim failed to send start event");
        return;
    }
    flux_log (h, LOG_DEBUG, "sending start event again");
}

static struct flux_msg_handler_spec htab[] = {
    {FLUX_MSGTYPE_REQUEST, "sim.join", join_cb},
    {FLUX_MSGTYPE_REQUEST, "sim.reply", reply_cb},
    {FLUX_MSGTYPE_REQUEST, "sim.alive", alive_cb},
    {FLUX_MSGTYPE_EVENT, "rdl.update", rdl_update_cb},
    FLUX_MSGHANDLER_TABLE_END,
};
const int htablen = sizeof (htab) / sizeof (htab[0]);

int mod_main (flux_t h, int argc, char **argv)
{
    zhash_t *args = zhash_fromargv (argc, argv);
    ctx_t *ctx;
    char *eoc_str;
    bool exit_on_complete;
    uint32_t rank;

    if (flux_get_rank (h, &rank) < 0)
        return -1;

    if (rank != 0) {
        flux_log (h, LOG_ERR, "sim module must only run on rank 0");
        return -1;
    }

    flux_log (h, LOG_INFO, "sim comms module starting");

    if (!(eoc_str = zhash_lookup (args, "exit-on-complete"))) {
        flux_log (h,
                  LOG_ERR,
                  "exit-on-complete argument is not set, defaulting to false");
        exit_on_complete = false;
    } else {
        exit_on_complete =
            (!strcmp (eoc_str, "true") || !strcmp (eoc_str, "True"));
    }
    ctx = getctx (h, exit_on_complete);

    if (flux_event_subscribe (h, "rdl.update") < 0) {
        flux_log (h, LOG_ERR, "subscribing to event: %s", strerror (errno));
        return -1;
    }

    if (flux_msg_handler_addvec (h, htab, ctx) < 0) {
        flux_log (h, LOG_ERR, "flux_msg_handler_add: %s", strerror (errno));
        return -1;
    }

    if (send_start_event (h) < 0) {
        flux_log (h, LOG_ERR, "sim failed to send start event");
        return -1;
    }
    flux_log (h, LOG_DEBUG, "sim sent start event");

    if (flux_reactor_run (flux_get_reactor (h), 0) < 0) {
        flux_log (h, LOG_ERR, "flux_reactor_run: %s", strerror (errno));
        return -1;
    }
    return 0;
}

MOD_NAME ("sim");

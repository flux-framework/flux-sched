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
#include <getopt.h>
#include <errno.h>
#include <czmq.h>
#include <flux/core.h>

#include "src/common/libutil/shortjansson.h"
#include "src/common/libutil/log.h"
#include "src/common/libutil/xzmalloc.h"

typedef struct {
    flux_t *h;
    int64_t jobid;
    char *start;
    char *complete;
    job_state_t tgt_state;
} wjctx_t;

static flux_t *sig_flux_h;

#define OPTIONS "+hc:s:j:"
static const struct option longopts[] = {
    {"help",          no_argument,        0, 'h'},
    {"sync-start",    required_argument,  0, 's'},
    {"sync-complete", required_argument,  0, 'c'},
    {"job-state",     required_argument,  0, 'j'},
    { 0, 0, 0, 0 },
};

static void usage (void)
{
    fprintf (stderr,
"Usage: flux-waitjob [OPTIONS] jobid\n"
" Block waiting until the job specified by jobid completes.\n"
" The OPTIONS are:\n"
"  -h, --help                    Display this message\n"
"  -s, --sync-start=filename1    Create an empty file (filename1) right \n"
"                                    after the reactor starts\n"
"  -c, --sync-complete=filename2 Create an empty file (filename2) right \n"
"                                    after jobid reaches the target state\n"
"  -j, --job-state=state         Wait for the job to become the target \n"
"                                    state, e.g., submitted, allocated, \n"
"                                    running, complete (default).\n"
);
    exit (1);
}

static void freectx (void *arg)
{
    wjctx_t *ctx = arg;
    if (ctx->start)
        free (ctx->start);
    if (ctx->complete)
        free (ctx->complete);
    free (ctx);
    ctx = NULL;
}

static wjctx_t *getctx (flux_t *h)
{
    wjctx_t *ctx = (wjctx_t *)flux_aux_get (h, "waitjob");
    if (!ctx) {
        ctx = xzmalloc (sizeof (*ctx));
        ctx->jobid = -1;
        ctx->h = h;
        ctx->start = NULL;
        ctx->complete = NULL;
        flux_aux_set (h, "waitjob", ctx, freectx);
    }
    return ctx;
}

static void touch_outfile (const char *fn)
{
    FILE *fp = NULL;
    if (fn && !(fp = fopen (fn, "w"))) {
        fprintf (stderr, "Failed to open %s\n", fn);
    }
    fclose (fp);
}

static inline void get_jobid (json_t *jcb, int64_t *j)
{
    Jget_int64 (jcb, JSC_JOBID, j);
}

static inline void get_states (json_t *jcb, int64_t *os, int64_t *ns)
{
    json_t *o = NULL;
    Jget_obj (jcb, JSC_STATE_PAIR, &o);
    Jget_int64 (o, JSC_STATE_PAIR_OSTATE, os);
    Jget_int64 (o, JSC_STATE_PAIR_NSTATE, ns);
}

static bool state_reached (wjctx_t *ctx)
{
    json_t *jcb = NULL;
    json_t *o = NULL;
    bool rc = false;
    char *json_str = NULL;
    int64_t state = J_NULL;

    if (jsc_query_jcb (ctx->h, ctx->jobid, JSC_STATE_PAIR, &json_str) == 0) {
        jcb = Jfromstr (json_str);
        Jget_obj (jcb, JSC_STATE_PAIR, &o);
        Jget_int64 (o, JSC_STATE_PAIR_NSTATE, &state);
        Jput (jcb);
        free (json_str);
        log_msg ("%"PRId64" already started (%s)",
                 ctx->jobid, jsc_job_num2state (state));
        if (state == ctx->tgt_state) {
            log_msg ("%"PRId64" already reached the target state", ctx->jobid);
            rc = true;
        }
    }
    return rc;
}

static int waitjob_cb (const char *jcbstr, void *arg, int errnum)
{
    json_t *jcb = NULL;
    int64_t os = 0, ns = 0, j = 0;
    flux_t *h = (flux_t *)arg;
    wjctx_t *ctx = getctx (h);

    if (errnum > 0) {
        log_errn (errnum, "waitjob_cb: jsc error");
        return -1;
    }

    if (!(jcb = Jfromstr (jcbstr))) {
        log_msg ("waitjob_cb: error parsing JSON string");
        return -1;
    }
    get_jobid (jcb, &j);
    get_states (jcb, &os, &ns);
    Jput (jcb);
    if ((j == ctx->jobid) && (ns == ctx->tgt_state)) {
        if (ctx->complete)
            touch_outfile (ctx->complete);
        log_msg ("waitjob_cb: completion notified");
        flux_reactor_stop (flux_get_reactor (ctx->h));
    }

    return 0;
}

static int wait_job_complete (flux_t *h)
{
    int rc = -1;
    sig_flux_h = h;
    wjctx_t *ctx = getctx (h);

    if (jsc_notify_status (h, waitjob_cb, (void *)h) != 0) {
        log_err ("failed to register a waitjob CB");
        goto done;
    }
    /* once jsc_notify_status is returned, all of JSC events
     * will be queued and delivered. It is safe to signal
     * readiness.
     */
    if (ctx->start)
        touch_outfile (ctx->start);

    if (state_reached (ctx)) {
        if (ctx->complete)
            touch_outfile (ctx->complete);
        log_msg ("wait_job_complete: completion detected");
        goto done;
    }
    if (flux_reactor_run (flux_get_reactor (h), 0) < 0) {
        log_err ("error in flux_reactor_run");
        goto done;
    }
    rc = 0;
done:
    return rc;
}

/******************************************************************************
 *                                                                            *
 *                            Main entry point                                *
 *                                                                            *
 ******************************************************************************/

int main (int argc, char *argv[])
{
    flux_t *h;
    int ch = 0;
    int64_t jobid = -1;
    char *sfn = NULL;
    char *cfn = NULL;
    char *state = NULL;
    int sc = -1;
    wjctx_t *ctx = NULL;

    log_init ("flux-waitjob");
    while ((ch = getopt_long (argc, argv, OPTIONS, longopts, NULL)) != -1) {
        switch (ch) {
            case 'h': /* --help */
                usage ();
                break;
            case 's': /* --sync-start */
                sfn = xstrdup (optarg);
                break;
            case 'c': /* --sync-complete */
                cfn = xstrdup (optarg);
                break;
            case 'j': /* --job-state */
                state = xstrdup (optarg);
                break;
            default:
                usage ();
                break;
        }
    }
    if (optind == argc)
        usage ();

    jobid = strtol (argv[optind], NULL, 10);
    if (jobid <= 0)
        log_err_exit ("jobid must be a positive number");
    else if (!(h = flux_open  (NULL, 0)))
        log_err_exit ("flux_open");

    ctx = getctx (h);
    if (sfn)
        ctx->start = sfn;
    if (cfn)
        ctx->complete = cfn;
    ctx->jobid = jobid;

    if (state) {
        if ((sc = jsc_job_state2num (state)) < 0)
            log_err_exit ("unknown job state");
        ctx->tgt_state = jsc_job_state2num (state);
    } else {
        ctx->tgt_state = J_COMPLETE;
    }

    wait_job_complete (h);

    flux_close (h);
    log_fini ();

    return 0;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

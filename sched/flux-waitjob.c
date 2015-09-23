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
#include <getopt.h>
#include <errno.h>
#include <czmq.h>
#include <json.h>
#include <flux/core.h>

#include "src/common/libutil/shortjson.h"
#include "src/common/libutil/jsonutil.h"
#include "src/common/libutil/log.h"
#include "src/common/libutil/xzmalloc.h"

typedef struct {
    flux_t h;
    int64_t jobid;
    char *sync;
} wjctx_t;

static flux_t sig_flux_h;

#define OPTIONS "hj:o:"
static const struct option longopts[] = {
    {"help",       no_argument,        0, 'h'},
    {"out",        required_argument,  0, 'o'},
    { 0, 0, 0, 0 },
};

static void usage (void)
{
    fprintf (stderr,
"Usage: flux-waitjob [OPTIONS] <jobid>\n"
"  -h, --help                 Display this message\n"
"  -o, --out=filename         Create an empty file when detects jobid completed\n"
);
    exit (1);
}

static void freectx (void *arg)
{
    wjctx_t *ctx = arg;
    if (ctx->sync)
        free (ctx->sync);
}

static wjctx_t *getctx (flux_t h)
{
    wjctx_t *ctx = (wjctx_t *)flux_aux_get (h, "waitjob");
    if (!ctx) {
        ctx = xzmalloc (sizeof (*ctx));
        ctx->sync = NULL;
        flux_aux_set (h, "waitjob", ctx, freectx);
    }
    return ctx;
}

static void sig_handler (int s)
{
    if (s == SIGINT) {
        fprintf (stdout, "Exit on INT");
        flux_close (sig_flux_h);
        log_fini ();
        exit (0);
    }
}

static void create_outfile (const char *fn)
{
    FILE *fp;
    if (!fn)
        fp = NULL;
    else if ( !(fp = fopen (fn, "w")))
        fprintf (stderr, "Failed to open %s\n", fn);
    fclose (fp);
}

static inline void get_jobid (JSON jcb, int64_t *j)
{
    Jget_int64 (jcb, JSC_JOBID, j);
}

static inline void get_states (JSON jcb, int64_t *os, int64_t *ns)
{
    JSON o = NULL;
    Jget_obj (jcb, JSC_STATE_PAIR, &o);
    Jget_int64 (o, JSC_STATE_PAIR_OSTATE, os);
    Jget_int64 (o, JSC_STATE_PAIR_NSTATE, ns);
}

static int waitjob_cb (const char *jcbstr, void *arg, int errnum)
{
    int64_t os = 0;
    int64_t ns = 0;
    int64_t j = 0;
    JSON jcb = NULL;
    wjctx_t *ctx = NULL;
    flux_t h = (flux_t)arg;

    ctx = getctx (h);
    if (errnum > 0) {
        flux_log (ctx->h, LOG_ERR, "waitjob_cb: errnum passed in");
        return -1;
    }

    if (!(jcb = Jfromstr (jcbstr))) {
        flux_log (ctx->h, LOG_ERR, "waitjob_cb: error parsing JSON string");
        return -1;
    }
    get_jobid (jcb, &j);
    get_states (jcb, &os, &ns);
    Jput (jcb);

    if ((j == ctx->jobid) && (ns == J_COMPLETE)) {
        if (ctx->sync)
            create_outfile (ctx->sync);
        raise (SIGINT);
    }

    return 0;
}

int wait_job_complete (flux_t h, int64_t jobid)
{
    int rc = -1;
    sig_flux_h = h;
    JSON jcb = NULL;
    JSON o = NULL;
    wjctx_t *ctx = getctx (h);
    ctx->jobid = jobid;
    char *json_str = NULL;
    int64_t state = J_NULL;

    if (jsc_query_jcb (h, jobid, JSC_STATE_PAIR, &json_str) == 0) {
        jcb = Jfromstr (json_str);
        Jget_obj (jcb, JSC_STATE_PAIR, &o);
        Jget_int64 (o, JSC_STATE_PAIR_NSTATE, &state);
        Jput (jcb);
        free (json_str);
        flux_log (h, LOG_INFO, "%ld already started (%s)",
                     jobid, jsc_job_num2state (state));
        if (state == J_COMPLETE) {
            flux_log (h, LOG_INFO, "%ld already completed", jobid);
            if (ctx->sync)
                create_outfile (ctx->sync);
            rc =0;
            goto done;
        }
    } else if (signal (SIGINT, sig_handler) == SIG_ERR) {
        goto done;
    } else if (jsc_notify_status (h, waitjob_cb, (void *)h) != 0) {
        flux_log (h, LOG_ERR, "failed to reg a waitjob CB");
        goto done;
    } else if (flux_reactor_start (h) < 0) {
        flux_log (h, LOG_ERR, "error in flux_reactor_start");
        goto done;
    }

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
    flux_t h;
    int ch = 0;
    int64_t jobid = -1;
    char *fn;
    wjctx_t *ctx = NULL;

    log_init ("flux-waitjob");
    while ((ch = getopt_long (argc, argv, OPTIONS, longopts, NULL)) != -1) {
        switch (ch) {
            case 'h': /* --help */
                usage ();
                break;
            case 'o': /* --out */
                fn = strdup (optarg);
                break;
            default:
                usage ();
                break;
        }
    }
    if (optind == argc)
        usage ();

    jobid = strtol (argv[optind], NULL, 10);

    if (!(h = flux_open  (NULL, 0)))
        err_exit ("flux_open");

    ctx = getctx (h);
    ctx->sync = strdup (fn);
    free (fn);

    flux_log_set_facility (h, "waitjob");
    wait_job_complete (h, jobid);

    flux_close (h);
    log_fini ();

    return 0;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

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
#include "config.h"
#endif

#include <limits.h>
#include <ctype.h>
#include <libgen.h>
#include <syslog.h>
#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <zmq.h>

#include "log.h"

static char *prog = "flux";

typedef struct {
    char *s;
    int n;
} match_t;

static match_t facility_tab[] = {
    { "daemon", LOG_DAEMON },
    { "local0", LOG_LOCAL0 },
    { "local1", LOG_LOCAL1 },
    { "local2", LOG_LOCAL2 },
    { "local3", LOG_LOCAL3 },
    { "local4", LOG_LOCAL4 },
    { "local5", LOG_LOCAL5 },
    { "local6", LOG_LOCAL6 },
    { "local7", LOG_LOCAL7 },
    { "user",   LOG_USER },
    { NULL,     0},
};

static match_t level_tab[] = {
    { "emerg",  LOG_EMERG },
    { "alert",  LOG_ALERT },
    { "crit",   LOG_CRIT },
    { "err",    LOG_ERR },
    { "warning", LOG_WARNING },
    { "notice", LOG_NOTICE },
    { "info",   LOG_INFO },
    { "debug",  LOG_DEBUG },
    { NULL,     0},
};

typedef enum { DEST_LOGF, DEST_SYSLOG } dest_t;

static dest_t dest = DEST_LOGF;

static char *filename = NULL;
static FILE *logf = NULL;

static int syslog_facility = LOG_DAEMON;
static int syslog_level = LOG_ERR;

static int
_match (const char *s, match_t *m)
{
    int i;

    for (i = 0; m[i].s != NULL; i++)
        if (!strcasecmp (m[i].s, s))
            return m[i].n;
    return -1;
}

static char *
_rmatch (int n, match_t *m)
{
    int i;

    for (i = 0; m[i].s != NULL; i++)
        if (m[i].n == n)
            return m[i].s;
    return NULL;
}

const char *
log_leveltostr (int n)
{
    return _rmatch (n, level_tab);
}

int
log_strtolevel (const char *s)
{
    return _match (s, level_tab);
}

void
log_init (char *p)
{
    prog = basename (p);
    logf = stderr;
    openlog (prog, LOG_NDELAY | LOG_PID, syslog_facility);
}

void
log_fini (void)
{
    closelog ();
    if (logf != NULL)
        fflush (logf);
    if (logf != stdout && logf != stderr && logf != NULL)
        fclose (logf);
}

static void
_set_syslog_facility (char *s)
{
    int n = _match (s, facility_tab);

    if (n < 0)
        msg_exit ("unknown syslog facility: %s", s);
    syslog_facility = n;
    closelog ();
    openlog (prog, LOG_NDELAY | LOG_PID, syslog_facility);
}

static void
_set_syslog_level (char *s)
{
    int n = _match (s, level_tab);

    if (n < 0)
        msg_exit ("unknown syslog level: %s", s);
    syslog_level = n;
}

void
log_set_dest (char *s)
{
    char *fac, *lev;
    FILE *f;

    if (strcmp (s, "syslog") == 0) {
        dest = DEST_SYSLOG;
    } else if (strncmp (s, "syslog:", 7) == 0) {
        if (!(fac = strdup (s + 7)))
            msg_exit ("out of memory");
        if ((lev = strchr (fac, ':')))
            *lev++ = '\0';
        _set_syslog_facility (fac);
        if (lev)
            _set_syslog_level (lev);
        free (fac);
        dest = DEST_SYSLOG;
    } else {
        if (strcmp (s, "stderr") == 0)
            logf = stderr;
        else if (strcmp (s, "stdout") == 0)
            logf = stdout;
        else if ((f = fopen (s, "a"))) {
            if (logf != stdout && logf != stderr && logf != NULL)
                fclose (logf);
            logf = f;
            filename = s;
        } else
            err_exit ("could not open %s for writing", s);
        dest = DEST_LOGF;
    }
}

char *
log_get_dest (void)
{
    int len = PATH_MAX + 1;
    char *res = malloc (len);

    if (!res)
        goto done;
    switch (dest) {
        case DEST_SYSLOG:
            snprintf (res, PATH_MAX + 1, "syslog:%s:%s",
                      _rmatch (syslog_facility, facility_tab),
                      _rmatch (syslog_level, level_tab));
            break;
        case DEST_LOGF:
            if (!logf)
                logf = stderr;
            snprintf (res, len, "%s", logf == stdout ? "stdout" :
                                      logf == stderr ? "stderr" : 
                                      logf == NULL   ? "unknown" : filename);
            break;
    }
done:
    return res;
}

static void
_verr (int errnum, const char *fmt, va_list ap)
{
    char *msg;
    char buf[128];
    const char *s = zmq_strerror (errnum);

    if (vasprintf (&msg, fmt, ap) < 0) {
        (void)vsnprintf (buf, sizeof (buf), fmt, ap);
        msg = buf;
    }
    switch (dest) {
        case DEST_LOGF:
            if (!logf)
                logf = stderr;
            fprintf (logf, "%s: %s: %s\n", prog, msg, s);
            fflush (logf);
            break;
        case DEST_SYSLOG:
            syslog (syslog_level, "%s: %s", msg, s);
            break;
    }
    if (msg != buf)
        free (msg);
}

void
log_msg (const char *fmt, va_list ap)
{
    char *msg;
    char buf[128];

    if (vasprintf (&msg, fmt, ap) < 0) {
        (void)vsnprintf (buf, sizeof (buf), fmt, ap);
        msg = buf;
    }
    switch (dest) {
        case DEST_LOGF:
            if (!logf)
                logf = stderr;
            fprintf (logf, "%s: %s\n", prog, msg);
            fflush (logf);
            break;
        case DEST_SYSLOG:
            syslog (syslog_level, "%s", msg);
            break;
    }
    if (msg != buf)
        free (msg);
}

/* Log message and errno string, then exit.
 */
void
err_exit (const char *fmt, ...)
{
    va_list ap;

    va_start (ap, fmt);
    _verr (errno, fmt, ap);
    va_end (ap);
    exit (1);
}

/* Log message and errno string.
 */
void
err (const char *fmt, ...)
{
    va_list ap;

    va_start (ap, fmt);
    _verr (errno, fmt, ap);
    va_end (ap);
}

/* Log message and errnum string, then exit.
 */
void
errn_exit (int errnum, const char *fmt, ...)
{
    va_list ap;

    va_start (ap, fmt);
    _verr (errnum, fmt, ap);
    va_end (ap);
    exit (1);
}

/* Log message and errnum string.
 */
void
errn (int errnum, const char *fmt, ...)
{
    va_list ap;

    va_start (ap, fmt);
    _verr (errnum, fmt, ap);
    va_end (ap);
}

/* Log message, then exit.
 */
void
msg_exit (const char *fmt, ...)
{
    va_list ap;

    va_start (ap, fmt);
    log_msg (fmt, ap);
    va_end (ap);
    exit (1);
}

/* Log message.
 */
void
msg (const char *fmt, ...)
{
    va_list ap;

    va_start (ap, fmt);
    log_msg (fmt, ap);
    va_end (ap);
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

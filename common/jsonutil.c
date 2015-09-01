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
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/resource.h>
#include <stdarg.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <math.h>
#include <limits.h>
#include <assert.h>
#include <json.h>
#include <zmq.h>

#include "log.h"
#include "xzmalloc.h"
#include "jsonutil.h"
#include "base64.h"

int util_json_size (json_object *o)
{
    const char *s = json_object_to_json_string (o);
    return strlen (s);
}

bool util_json_match (json_object *o1, json_object *o2)
{
    const char *s1 = json_object_to_json_string (o1);
    const char *s2 = json_object_to_json_string (o2);

    return !strcmp (s1, s2);
}

void util_json_object_add_boolean (json_object *o, char *name, bool val)
{
    json_object *no;

    if (!(no = json_object_new_boolean (val)))
        oom ();
    json_object_object_add (o, name, no);
}

void util_json_object_add_double (json_object *o, char *name, double n)
{
    json_object *no;

    if (!(no = json_object_new_double (n)))
        oom ();
    json_object_object_add (o, name, no);
}

void util_json_object_add_int (json_object *o, char *name, int i)
{
    json_object *no;

    if (!(no = json_object_new_int (i)))
        oom ();
    json_object_object_add (o, name, no);
}

void util_json_object_add_int64 (json_object *o, char *name, int64_t i)
{
    json_object *no;

    if (!(no = json_object_new_int64 (i)))
        oom ();
    json_object_object_add (o, name, no);
}


void util_json_object_add_string (json_object *o, char *name, const char *s)
{
    json_object *no;

    if (!(no = json_object_new_string (s)))
        oom ();
    json_object_object_add (o, name, no);
}

/* base64 */
void util_json_object_add_data (json_object *o, char *name,
                                uint8_t *dat, int len)
{
    char *buf;
    int dstlen;
    int size = base64_encode_length (len);

    buf = xzmalloc (size);
    (void) base64_encode_block (buf, &dstlen, dat, len);
    util_json_object_add_string (o, name, buf);
    free (buf);
}

void util_json_object_add_timeval (json_object *o, char *name,
                                   struct timeval *tvp)
{
    json_object *no;
    char tbuf[32];

    snprintf (tbuf, sizeof (tbuf), "%lu.%lu", tvp->tv_sec, tvp->tv_usec);
    if (!(no = json_object_new_string (tbuf)))
        oom ();
    json_object_object_add (o, name, no);
}

static json_object *util_json_object_object_get (json_object *o, char *name)
{
    json_object *ret = NULL;
    json_object_object_get_ex (o, name, &ret);
    return (ret);
}

int util_json_object_get_boolean (json_object *o, char *name, bool *vp)
{
    json_object *no = util_json_object_object_get (o, name);
    if (!no)
        return -1;
    *vp = json_object_get_boolean (no);
    return 0;
}

int util_json_object_get_double (json_object *o, char *name, double *dp)
{
    json_object *no = util_json_object_object_get (o, name);
    if (!no)
        return -1;
    *dp = json_object_get_double (no);
    return 0;
}

int util_json_object_get_int (json_object *o, char *name, int *ip)
{
    json_object *no = util_json_object_object_get (o, name);
    if (!no)
        return -1;
    *ip = json_object_get_int (no);
    return 0;
}

int util_json_object_get_int64 (json_object *o, char *name, int64_t *ip)
{
    json_object *no = util_json_object_object_get (o, name);
    if (!no)
        return -1;
    *ip = json_object_get_int64 (no);
    return 0;
}

int util_json_object_get_string (json_object *o, char *name, const char **sp)
{
    json_object *no = util_json_object_object_get (o, name);
    if (!no)
        return -1;
    *sp = json_object_get_string (no);
    return 0;
}

/* base64 */
int util_json_object_get_data (json_object *o, char *name,
                               uint8_t **datp, int *lenp)
{
    const char *s;
    int dlen, len;
    void *dst;

    if (util_json_object_get_string (o, name, &s) == -1)
        return -1;

    len = strlen (s);
    dst = xzmalloc (base64_decode_length (len));
    if (base64_decode_block (dst, &dlen, s, len) < 0) {
        free (dst);
        return -1;
    }

    *lenp = dlen;
    *datp = (uint8_t *) dst;
    return 0;
}

int util_json_object_get_timeval (json_object *o, char *name,
                                  struct timeval *tvp)
{
    struct timeval tv;
    char *endptr;
    json_object *no = util_json_object_object_get (o, name);
    if (!no)
        return -1;
    tv.tv_sec = strtoul (json_object_get_string (no), &endptr, 10);
    tv.tv_usec = *endptr ? strtoul (endptr + 1, NULL, 10) : 0;
    *tvp = tv;
    return 0;
}

int util_json_object_get_int_array (json_object *o, char *name,
                                    int **ap, int *lp)
{
    json_object *no = util_json_object_object_get (o, name);
    json_object *vo;
    int i, len, *arr = NULL;

    if (!no)
        goto error;
    len = json_object_array_length (no);
    arr = xzmalloc (sizeof (int) * len);
    for (i = 0; i < len; i++) {
        vo = json_object_array_get_idx (no, i);
        if (!vo)
            goto error;
        arr[i] = json_object_get_int (vo);
    }
    *ap = arr;
    *lp = len;
    return 0;
error:
    if (arr)
        free (arr);
    return -1;
}

json_object *util_json_object_new_object (void)
{
    json_object *o;

    if (!(o = json_object_new_object ()))
        oom ();
    return o;
}

json_object *rusage_to_json (struct rusage *ru)
{
    json_object *o = util_json_object_new_object ();
    util_json_object_add_timeval (o, "utime", &ru->ru_utime);
    util_json_object_add_timeval (o, "stime", &ru->ru_stime);
    util_json_object_add_int64 (o, "maxrss", ru->ru_maxrss);
    util_json_object_add_int64 (o, "ixrss", ru->ru_ixrss);
    util_json_object_add_int64 (o, "idrss", ru->ru_idrss);
    util_json_object_add_int64 (o, "isrss", ru->ru_isrss);
    util_json_object_add_int64 (o, "minflt", ru->ru_minflt);
    util_json_object_add_int64 (o, "majflt", ru->ru_majflt);
    util_json_object_add_int64 (o, "nswap", ru->ru_nswap);
    util_json_object_add_int64 (o, "inblock", ru->ru_inblock);
    util_json_object_add_int64 (o, "oublock", ru->ru_oublock);
    util_json_object_add_int64 (o, "msgsnd", ru->ru_msgsnd);
    util_json_object_add_int64 (o, "msgrcv", ru->ru_msgrcv);
    util_json_object_add_int64 (o, "nsignals", ru->ru_nsignals);
    util_json_object_add_int64 (o, "nvcsw", ru->ru_nvcsw);
    util_json_object_add_int64 (o, "nivcsw", ru->ru_nivcsw);
    return o;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

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

/* flux-rdltool -- Test interface to Flux RDL C API */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <getopt.h>
#include <json.h>

#include "rdl.h"

struct prog_ctx {
    const char *filename;
    char *cmd;
    char **args;
};

static void fatal (int code, char *format, ...)
{
    va_list ap;
    va_start (ap, format);
    vfprintf (stderr, format, ap);
    va_end (ap);
    exit (code);
}

static int parse_cmdline (struct prog_ctx *ctx, int ac, char **av)
{
    const char *usage = "[OPTIONS] CMD [ARGS]...\n"
        "Supported CMDs include:\n"
        " resource URI\t Print resource at URI\n"
        " tree URI\t print hierarchy tree at URI\n"
        " aggregate URI\t aggregate hierarchy tree at URI\n";
    const char *options = "+f";
    const struct option longopts[] = {
        { "config-file", required_argument, 0, 'f' },
        { 0, 0, 0, 0 },
    };
    int ch;

    while ((ch = getopt_long (ac, av, options, longopts, NULL)) != -1) {
        switch (ch) {
            case 'f':
                ctx->filename = optarg;
                break;
            default:
                fprintf (stderr, "Usage: flux-rdltool %s", usage);
                exit (1);
        }
    }
    if (optind == ac)
        fatal (1, "Missing command\n");

    ctx->cmd = strdup (av[optind]);
    ctx->args = av + optind + 1;

    return (0);
}

void prog_ctx_destroy (struct prog_ctx *ctx)
{
    if (ctx->cmd)
        free (ctx->cmd);
    free (ctx);
}

struct prog_ctx *prog_ctx_create (int ac, char **av)
{
    struct prog_ctx *ctx = malloc (sizeof (*ctx));
    if (ctx == NULL)
        fatal (1, "Out of memory");
    ctx->filename = NULL;
    ctx->cmd = NULL;
    parse_cmdline (ctx, ac, av);
    return (ctx);
}

void output_resource (struct prog_ctx *ctx, struct rdl *rdl, const char *uri)
{
    json_object *o;
    struct resource *r = rdl_resource_get (rdl, uri);
    if (r == NULL)
        fatal (1, "Failed to find resource `%s'\n", uri);

    o = rdl_resource_json (r);
    fprintf (stdout, "%s:\n%s\n", uri, json_object_to_json_string (o));
    json_object_put (o);
    rdl_resource_destroy (r);
    return;
}

void print_resource (struct prog_ctx *ctx, struct resource *r, int pad)
{
    struct resource *c;

    fprintf (stdout, "%*s/%s\n", pad, "", rdl_resource_name (r));

    rdl_resource_iterator_reset (r);
    while ((c = rdl_resource_next_child (r))) {
        print_resource (ctx, c, pad+1);
        rdl_resource_destroy (c);
    }
}

void output_tree (struct prog_ctx *ctx, struct rdl *rdl, const char *uri)
{
    struct resource *r = rdl_resource_get (rdl, uri);
    if (r == NULL)
        fatal (1, "Failed to find resource `%s'\n", uri);
    print_resource (ctx, r, 0);
    rdl_resource_destroy (r);
}

void aggregate (struct prog_ctx *ctx, struct rdl *rdl, const char *uri)
{
    json_object *o;
    struct resource *r = rdl_resource_get (rdl, uri);
    if (r == NULL)
        fatal (1, "Failed to find resource `%s'\n", uri);

    o = rdl_resource_aggregate_json (r);
    fprintf (stdout, "%s:\n%s\n", uri, json_object_to_json_string (o));
    json_object_put (o);
    rdl_resource_destroy (r);
    return;
}

int main (int ac, char **av)
{
    struct prog_ctx *ctx = prog_ctx_create (ac, av);
    struct rdllib *l = rdllib_open ();

    struct rdl *rdl = rdl_loadfile (l, ctx->filename);
    if (!rdl)
        fatal (1, "Failed to load config file: %s\n", ctx->filename);

    if (strcmp (ctx->cmd, "resource") == 0) {
        output_resource (ctx, rdl, ctx->args[0]);
    }
    else if (strcmp (ctx->cmd, "tree") == 0) {
        output_tree (ctx, rdl, ctx->args[0]);
    }
    else if (strcmp (ctx->cmd, "aggregate") == 0) {
        aggregate (ctx, rdl, ctx->args[0]);
    }
    else
        fatal (1, "Unknown command: %s\n", ctx->cmd);

    rdllib_close (l);
    prog_ctx_destroy (ctx);
    return (0);
}




/*
 * vi:ts=4 sw=4 expandtab
 */

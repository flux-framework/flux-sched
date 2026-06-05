/*****************************************************************************\
 * Copyright 2025 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "resource/reapi/bindings/c/reapi_cli.h"
#include "resource/policies/base/match_op.h"
#include "src/common/libtap/tap.h"

static const char *tiny_jgf =
    "{"
    "\"graph\": {"
    "\"nodes\": ["
    "{\"id\": \"0\", \"metadata\": {\"type\": \"cluster\", \"basename\": \"tiny\", \"name\": "
    "\"tiny0\", \"size\": 1, \"paths\": {\"containment\": \"/tiny0\"}}},"
    "{\"id\": \"1\", \"metadata\": {\"type\": \"node\", \"basename\": \"node\", \"name\": "
    "\"node0\", \"size\": 1, \"rank\": 0, \"paths\": {\"containment\": \"/tiny0/node0\"}}},"
    "{\"id\": \"2\", \"metadata\": {\"type\": \"core\", \"basename\": \"core\", \"name\": "
    "\"core0\", \"size\": 1, \"id\": 0, \"rank\": 0, \"paths\": {\"containment\": "
    "\"/tiny0/node0/core0\"}}}"
    "],"
    "\"edges\": [{\"source\": \"0\", \"target\": \"1\"}, {\"source\": \"1\", \"target\": \"2\"}]"
    "}"
    "}";

static const char *tiny_params =
    "{\"load_format\": \"jgf\", \"matcher_policy\": \"high\", "
    "\"match_format\": \"rv1\", \"matcher_name\": \"CA\"}";

static const char *simple_jobspec =
    "{"
    "\"version\": 1,"
    "\"resources\": ["
    "{"
    "\"type\": \"node\","
    "\"count\": 1,"
    "\"with\": ["
    "{"
    "\"type\": \"slot\","
    "\"count\": 1,"
    "\"label\": \"task\","
    "\"with\": [{\"type\": \"core\", \"count\": 1}]"
    "}"
    "]"
    "}"
    "],"
    "\"tasks\": [{\"command\": [\"sleep\", \"0\"], \"slot\": \"task\", \"count\": {\"per_slot\": "
    "1}}],"
    "\"attributes\": {\"system\": {\"duration\": 60.0}}"
    "}";

static int test_clone ()
{
    reapi_cli_ctx_t *ctx = reapi_cli_new ();
    ok (ctx != NULL, "reapi_cli_new succeeded");

    int rc = reapi_cli_initialize (ctx, tiny_jgf, tiny_params);
    if (rc < 0)
        BAIL_OUT ("reapi_cli_initialize failed: %s", reapi_cli_get_err_msg (ctx));
    ok (rc == 0, "reapi_cli_initialize succeeded");

    // Clone the context
    reapi_cli_ctx_t *clone = reapi_cli_clone (ctx);
    ok (clone != NULL, "reapi_cli_clone succeeded");

    // Test with NULL context
    errno = 0;
    reapi_cli_ctx_t *null_clone = reapi_cli_clone (NULL);
    ok (null_clone == NULL && errno == EINVAL,
        "reapi_cli_clone returns NULL with errno=EINVAL for NULL context");

    reapi_cli_destroy (clone);
    reapi_cli_destroy (ctx);
    return 0;
}

static int test_match_with_jobid ()
{
    reapi_cli_ctx_t *ctx = reapi_cli_new ();
    if (!ctx)
        BAIL_OUT ("reapi_cli_new failed");

    int rc = reapi_cli_initialize (ctx, tiny_jgf, tiny_params);
    if (rc < 0)
        BAIL_OUT ("reapi_cli_initialize failed: %s", reapi_cli_get_err_msg (ctx));

    uint64_t jobid = 100;
    bool reserved = false;
    char *R = NULL;
    int64_t at = 0;
    double ov = 0.0;

    // Test reapi_cli_match_with_jobid with explicit jobid
    errno = 0;
    rc = reapi_cli_match_with_jobid (ctx,
                                     MATCH_ALLOCATE,
                                     simple_jobspec,
                                     jobid,
                                     &reserved,
                                     &R,
                                     &at,
                                     &ov);
    ok (rc == 0, "reapi_cli_match_with_jobid succeeded");
    ok (R != NULL, "reapi_cli_match_with_jobid returned R string");
    free (R);
    R = NULL;

    // Cancel the allocation
    rc = reapi_cli_cancel (ctx, jobid, false);
    ok (rc == 0, "reapi_cli_cancel succeeded for allocated job");

    // Try to allocate with same jobid - currently not an error, resources just get reallocated
    // (Duplicate jobid detection may be a future feature)
    // errno = 0;
    // rc = reapi_cli_match_with_jobid (ctx, MATCH_ALLOCATE, simple_jobspec, jobid, &reserved, &R,
    // &at, &ov); ok (rc == -1 && errno == EEXIST,
    //     "reapi_cli_match_with_jobid returns -1 with errno=EEXIST for duplicate jobid");

    // Test with NULL context
    errno = 0;
    rc = reapi_cli_match_with_jobid (NULL,
                                     MATCH_ALLOCATE,
                                     simple_jobspec,
                                     200,
                                     &reserved,
                                     &R,
                                     &at,
                                     &ov);
    ok (rc == -1 && errno == EINVAL,
        "reapi_cli_match_with_jobid returns -1 with errno=EINVAL for NULL context");

    reapi_cli_destroy (ctx);
    return 0;
}

static int test_find ()
{
    reapi_cli_ctx_t *ctx = reapi_cli_new ();
    if (!ctx)
        BAIL_OUT ("reapi_cli_new failed");

    int rc = reapi_cli_initialize (ctx, tiny_jgf, tiny_params);
    if (rc < 0)
        BAIL_OUT ("reapi_cli_initialize failed: %s", reapi_cli_get_err_msg (ctx));

    // Allocate a job first
    uint64_t jobid = 1;
    bool reserved = false;
    char *R = NULL;
    int64_t at = 0;
    double ov = 0.0;

    rc = reapi_cli_match_with_jobid (ctx,
                                     MATCH_ALLOCATE,
                                     simple_jobspec,
                                     jobid,
                                     &reserved,
                                     &R,
                                     &at,
                                     &ov);
    if (rc < 0)
        BAIL_OUT ("reapi_cli_match_with_jobid failed");
    ok (rc == 0, "allocated job for find test");
    free (R);
    R = NULL;

    // Test find with allocated resources (using default format)
    char *R_found = NULL;
    rc = reapi_cli_find (ctx, "sched-now=allocated", NULL, &R_found);
    ok (rc == 0, "reapi_cli_find with allocated resources succeeded");
    ok (R_found != NULL, "reapi_cli_find returned non-NULL R string");
    if (R_found)
        free (R_found);

    // Test find with explicit format
    R_found = NULL;
    rc = reapi_cli_find (ctx, "sched-now=allocated", "rv1_nosched", &R_found);
    ok (rc == 0, "reapi_cli_find with explicit format succeeded");
    ok (R_found != NULL, "reapi_cli_find with format returned non-NULL R string");
    if (R_found)
        free (R_found);

    // Cancel and test find with no allocated resources
    rc = reapi_cli_cancel (ctx, jobid, false);
    ok (rc == 0, "reapi_cli_cancel succeeded");

    R_found = NULL;
    rc = reapi_cli_find (ctx, "sched-now=allocated", NULL, &R_found);
    ok (rc == 0, "reapi_cli_find with no allocations succeeded");
    ok (R_found == NULL, "reapi_cli_find returned NULL for empty result");

    // Test with NULL context
    errno = 0;
    rc = reapi_cli_find (NULL, "sched-now=allocated", NULL, &R_found);
    ok (rc == -1 && errno == EINVAL,
        "reapi_cli_find returns -1 with errno=EINVAL for NULL context");

    reapi_cli_destroy (ctx);
    return 0;
}

int main (int argc, char *argv[])
{
    plan (NO_PLAN);

    test_clone ();
    test_match_with_jobid ();
    test_find ();

    done_testing ();
    return EXIT_SUCCESS;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

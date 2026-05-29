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

static int test_status_by_rank ()
{
    reapi_cli_ctx_t *ctx = reapi_cli_new ();
    if (!ctx)
        BAIL_OUT ("reapi_cli_new failed");

    int rc = reapi_cli_initialize (ctx, tiny_jgf, tiny_params);
    if (rc < 0)
        BAIL_OUT ("reapi_cli_initialize failed: %s", reapi_cli_get_err_msg (ctx));

    reapi_cli_clear_err_msg (ctx);

    // Test setting rank 0 to down
    errno = 0;
    rc = reapi_cli_set_status_by_rank (ctx, 0, "DOWN");
    ok (rc == 0, "reapi_cli_set_status_by_rank succeeded for rank 0 down");

    // Verify it's down
    const char *status;
    rc = reapi_cli_get_status_by_rank (ctx, 0, &status);
    ok (rc == 0 && !strcmp (status, "DOWN"),
        "reapi_cli_get_status_by_rank returns DOWN for rank 0");

    // Test setting rank 0 back to up
    errno = 0;
    rc = reapi_cli_set_status_by_rank (ctx, 0, "UP");
    ok (rc == 0, "reapi_cli_set_status_by_rank succeeded for rank 0 up");

    // Verify it's up
    rc = reapi_cli_get_status_by_rank (ctx, 0, &status);
    ok (rc == 0 && !strcmp (status, "UP"), "reapi_cli_get_status_by_rank returns UP for rank 0");

    // Test with nonexistent rank
    errno = 0;
    rc = reapi_cli_set_status_by_rank (ctx, 999, "DOWN");
    ok (rc == -1 && errno == ENOENT,
        "reapi_cli_set_status_by_rank returns -1 with errno=ENOENT for nonexistent rank");

    const char *err_msg = reapi_cli_get_err_msg (ctx);
    ok (err_msg != NULL && strlen (err_msg) > 0,
        "error message is set after set_status_by_rank failure");
    reapi_cli_clear_err_msg (ctx);

    // Test get with nonexistent rank
    errno = 0;
    rc = reapi_cli_get_status_by_rank (ctx, 999, &status);
    ok (rc == -1 && errno == ENOENT,
        "reapi_cli_get_status_by_rank returns -1 with errno=ENOENT for nonexistent rank");

    err_msg = reapi_cli_get_err_msg (ctx);
    ok (err_msg != NULL && strlen (err_msg) > 0,
        "error message is set after get_status_by_rank failure");
    reapi_cli_clear_err_msg (ctx);

    // Test get with FLUX_NODEID_ANY
    errno = 0;
    rc = reapi_cli_get_status_by_rank (ctx, FLUX_NODEID_ANY, &status);
    ok (rc == -1 && errno == EINVAL,
        "reapi_cli_get_status_by_rank returns -1 with errno=EINVAL for FLUX_NODEID_ANY");

    reapi_cli_destroy (ctx);
    return 0;
}

static int test_status_by_path ()
{
    reapi_cli_ctx_t *ctx = reapi_cli_new ();
    if (!ctx)
        BAIL_OUT ("reapi_cli_new failed");

    int rc = reapi_cli_initialize (ctx, tiny_jgf, tiny_params);
    if (rc < 0)
        BAIL_OUT ("reapi_cli_initialize failed: %s", reapi_cli_get_err_msg (ctx));

    reapi_cli_clear_err_msg (ctx);

    // Test setting node to down
    errno = 0;
    rc = reapi_cli_set_status (ctx, "/tiny0/node0", "DOWN");
    ok (rc == 0, "reapi_cli_set_status succeeded for /tiny0/node0 down");

    // Verify it's down
    const char *status;
    rc = reapi_cli_get_status (ctx, "/tiny0/node0", &status);
    ok (rc == 0 && !strcmp (status, "DOWN"), "reapi_cli_get_status returns DOWN for /tiny0/node0");

    // Test setting back to up
    errno = 0;
    rc = reapi_cli_set_status (ctx, "/tiny0/node0", "UP");
    ok (rc == 0, "reapi_cli_set_status succeeded for /tiny0/node0 up");

    // Verify it's up
    rc = reapi_cli_get_status (ctx, "/tiny0/node0", &status);
    ok (rc == 0 && !strcmp (status, "UP"), "reapi_cli_get_status returns UP for /tiny0/node0");

    // Test with invalid path
    errno = 0;
    rc = reapi_cli_set_status (ctx, "/nonexistent", "DOWN");
    ok (rc == -1 && errno == EINVAL,
        "reapi_cli_set_status returns -1 with errno=EINVAL for invalid path");

    const char *err_msg = reapi_cli_get_err_msg (ctx);
    ok (err_msg != NULL && strlen (err_msg) > 0, "error message is set after set_status failure");
    reapi_cli_clear_err_msg (ctx);

    // Test get with invalid path
    errno = 0;
    rc = reapi_cli_get_status (ctx, "/nonexistent", &status);
    ok (rc == -1 && errno == ENOENT,
        "reapi_cli_get_status returns -1 with errno=ENOENT for invalid path");

    err_msg = reapi_cli_get_err_msg (ctx);
    ok (err_msg != NULL && strlen (err_msg) > 0, "error message is set after get_status failure");
    reapi_cli_clear_err_msg (ctx);

    reapi_cli_destroy (ctx);
    return 0;
}

static int test_null_parameters ()
{
    reapi_cli_ctx_t *ctx = reapi_cli_new ();
    if (!ctx)
        BAIL_OUT ("reapi_cli_new failed");

    int rc = reapi_cli_initialize (ctx, tiny_jgf, tiny_params);
    if (rc < 0)
        BAIL_OUT ("reapi_cli_initialize failed: %s", reapi_cli_get_err_msg (ctx));

    reapi_cli_clear_err_msg (ctx);

    // Test get_status with NULL status pointer
    errno = 0;
    rc = reapi_cli_get_status (ctx, "/tiny0/node0", NULL);
    ok (rc == -1 && errno == EINVAL,
        "reapi_cli_get_status returns -1 with errno=EINVAL for NULL status pointer");

    // Test get_status_by_rank with NULL status pointer
    errno = 0;
    rc = reapi_cli_get_status_by_rank (ctx, 0, NULL);
    ok (rc == -1 && errno == EINVAL,
        "reapi_cli_get_status_by_rank returns -1 with errno=EINVAL for NULL status pointer");

    // Test set_status with NULL path
    errno = 0;
    rc = reapi_cli_set_status (ctx, NULL, "DOWN");
    ok (rc == -1 && errno == EINVAL,
        "reapi_cli_set_status returns -1 with errno=EINVAL for NULL path");

    // Test get_status with NULL path
    const char *status;
    errno = 0;
    rc = reapi_cli_get_status (ctx, NULL, &status);
    ok (rc == -1 && errno == EINVAL,
        "reapi_cli_get_status returns -1 with errno=EINVAL for NULL path");

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

int main (int argc, char *argv[])
{
    plan (NO_PLAN);

    test_clone ();
    test_status_by_rank ();
    test_status_by_path ();
    test_null_parameters ();
    test_match_with_jobid ();

    done_testing ();
    return EXIT_SUCCESS;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

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
#include <jansson.h>
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
    rc = reapi_cli_set_rank_status (ctx, "0", RESOURCE_DOWN);
    ok (rc == 0, "reapi_cli_set_rank_status succeeded for rank 0 down");

    // Verify it's down
    resource_status_t status;
    const char *err_msg;
    rc = reapi_cli_get_rank_status (ctx, "0", &status);
    ok (rc == 0 && status == RESOURCE_DOWN, "reapi_cli_get_rank_status returns DOWN for rank 0");

    // Test setting rank 0 back to up
    errno = 0;
    rc = reapi_cli_set_rank_status (ctx, "0", RESOURCE_UP);
    ok (rc == 0, "reapi_cli_set_rank_status succeeded for rank 0 up");

    // Verify it's up
    rc = reapi_cli_get_rank_status (ctx, "0", &status);
    ok (rc == 0 && status == RESOURCE_UP, "reapi_cli_get_rank_status returns UP for rank 0");

    // Test with nonexistent rank (should succeed since unknown ranks are silently ignored)
    errno = 0;
    rc = reapi_cli_set_rank_status (ctx, "999", RESOURCE_DOWN);
    ok (rc == 0, "reapi_cli_set_rank_status succeeds for nonexistent rank (silently ignored)");

    // Test get with nonexistent rank
    errno = 0;
    rc = reapi_cli_get_rank_status (ctx, "999", &status);
    ok (rc == -1 && errno == EINVAL,
        "reapi_cli_get_rank_status returns -1 with errno=EINVAL for nonexistent rank");

    err_msg = reapi_cli_get_err_msg (ctx);
    ok (err_msg != NULL && strlen (err_msg) > 0,
        "error message is set after get_rank_status failure");
    reapi_cli_clear_err_msg (ctx);

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
    rc = reapi_cli_set_status (ctx, "/tiny0/node0", RESOURCE_DOWN);
    ok (rc == 0, "reapi_cli_set_status succeeded for /tiny0/node0 down");

    // Verify it's down
    resource_status_t status;
    rc = reapi_cli_get_status (ctx, "/tiny0/node0", &status);
    ok (rc == 0 && status == RESOURCE_DOWN, "reapi_cli_get_status returns DOWN for /tiny0/node0");

    // Test setting back to up
    errno = 0;
    rc = reapi_cli_set_status (ctx, "/tiny0/node0", RESOURCE_UP);
    ok (rc == 0, "reapi_cli_set_status succeeded for /tiny0/node0 up");

    // Verify it's up
    rc = reapi_cli_get_status (ctx, "/tiny0/node0", &status);
    ok (rc == 0 && status == RESOURCE_UP, "reapi_cli_get_status returns UP for /tiny0/node0");

    // Test with invalid path
    errno = 0;
    rc = reapi_cli_set_status (ctx, "/nonexistent", RESOURCE_DOWN);
    ok (rc == -1 && errno == EINVAL,
        "reapi_cli_set_status returns -1 with errno=EINVAL for invalid path");

    const char *err_msg = reapi_cli_get_err_msg (ctx);
    ok (err_msg != NULL && strlen (err_msg) > 0, "error message is set after set_status failure");
    reapi_cli_clear_err_msg (ctx);

    // Test get with invalid path
    errno = 0;
    rc = reapi_cli_get_status (ctx, "/nonexistent", &status);
    ok (rc == -1 && errno == EINVAL,
        "reapi_cli_get_status returns -1 with errno=EINVAL for invalid path");

    err_msg = reapi_cli_get_err_msg (ctx);
    ok (err_msg != NULL && strlen (err_msg) > 0, "error message is set after get_status failure");
    reapi_cli_clear_err_msg (ctx);

    reapi_cli_destroy (ctx);
    return 0;
}

static int test_null_parameters ()
{
    reapi_cli_ctx_t *ctx = reapi_cli_new ();
    resource_status_t status;
    int rc;

    if (!ctx)
        BAIL_OUT ("reapi_cli_new failed");

    rc = reapi_cli_initialize (ctx, tiny_jgf, tiny_params);
    if (rc < 0)
        BAIL_OUT ("reapi_cli_initialize failed: %s", reapi_cli_get_err_msg (ctx));

    reapi_cli_clear_err_msg (ctx);

    // Test get_status with NULL status pointer
    errno = 0;
    rc = reapi_cli_get_status (ctx, "/tiny0/node0", NULL);
    ok (rc == -1 && errno == EINVAL,
        "reapi_cli_get_status returns -1 with errno=EINVAL for NULL status pointer");

    // Test get_rank_status with NULL status pointer
    errno = 0;
    rc = reapi_cli_get_rank_status (ctx, "0", NULL);
    ok (rc == -1 && errno == EINVAL,
        "reapi_cli_get_rank_status returns -1 with errno=EINVAL for NULL status pointer");

    // Test set_rank_status with empty string
    errno = 0;
    rc = reapi_cli_set_rank_status (ctx, "", RESOURCE_DOWN);
    ok (rc == -1 && errno == EINVAL,
        "reapi_cli_set_rank_status returns -1 with errno=EINVAL for empty string");

    // Test get_rank_status with invalid idset
    errno = 0;
    rc = reapi_cli_get_rank_status (ctx, "not-a-rank", &status);
    ok (rc == -1 && errno == EINVAL,
        "reapi_cli_get_rank_status returns -1 with errno=EINVAL for invalid idset");

    // Test get_rank_status with multiple ranks (requires exactly one rank)
    errno = 0;
    rc = reapi_cli_get_rank_status (ctx, "0-1", &status);
    ok (rc == -1 && errno == EINVAL,
        "reapi_cli_get_rank_status returns -1 with errno=EINVAL for multiple ranks");

    // Test set_rank_status with NULL context
    errno = 0;
    rc = reapi_cli_set_rank_status (NULL, "0", RESOURCE_DOWN);
    ok (rc == -1 && errno == EINVAL,
        "reapi_cli_set_rank_status returns -1 with errno=EINVAL for NULL context");

    // Test get_rank_status with NULL context
    errno = 0;
    rc = reapi_cli_get_rank_status (NULL, "0", &status);
    ok (rc == -1 && errno == EINVAL,
        "reapi_cli_get_rank_status returns -1 with errno=EINVAL for NULL context");

    // Test set_status with NULL path
    errno = 0;
    rc = reapi_cli_set_status (ctx, NULL, RESOURCE_DOWN);
    ok (rc == -1 && errno == EINVAL,
        "reapi_cli_set_status returns -1 with errno=EINVAL for NULL path");

    // Test get_status with NULL path
    errno = 0;
    rc = reapi_cli_get_status (ctx, NULL, &status);
    ok (rc == -1 && errno == EINVAL,
        "reapi_cli_get_status returns -1 with errno=EINVAL for NULL path");

    reapi_cli_destroy (ctx);
    return 0;
}

static int test_cancel_ex ()
{
    reapi_cli_ctx_t *ctx = reapi_cli_new ();
    if (!ctx)
        BAIL_OUT ("reapi_cli_new failed");

    int rc = reapi_cli_initialize (ctx, tiny_jgf, tiny_params);
    if (rc < 0)
        BAIL_OUT ("reapi_cli_initialize failed: %s", reapi_cli_get_err_msg (ctx));

    uint64_t jobid = 1;
    bool reserved = false;
    char *R = NULL;
    int64_t at = 0;
    double ov = 0.0;

    // Allocate a job first
    rc = reapi_cli_match_allocate (ctx, false, simple_jobspec, &jobid, &reserved, &R, &at, &ov);
    if (rc < 0)
        BAIL_OUT ("reapi_cli_match_allocate failed: %s", reapi_cli_get_err_msg (ctx));

    // Parse R to remove scheduling section and create partial R
    // This tests rv1exec format auto-detection (production use case)
    json_error_t error;
    json_t *r_obj = json_loads (R, 0, &error);
    if (!r_obj)
        BAIL_OUT ("couldn't parse R as JSON");

    // Remove scheduling section to get rv1exec format
    json_object_del (r_obj, "scheduling");

    // For partial cancel, modify rank to cancel only rank 0
    // (assuming the allocation has multiple ranks or we'll get full_removal=true)
    json_t *exec = json_object_get (r_obj, "execution");
    json_t *r_lite = json_object_get (exec, "R_lite");
    json_t *entry = json_array_get (r_lite, 0);
    json_object_set_new (entry, "rank", json_string ("0"));

    char *partial_r = json_dumps (r_obj, JSON_COMPACT);
    json_decref (r_obj);
    if (!partial_r)
        BAIL_OUT ("couldn't serialize partial R");

    // Test reapi_cli_partial_cancel with partial R (auto-detects rv1exec format)
    bool full_removal = false;
    errno = 0;
    rc = reapi_cli_partial_cancel (ctx, jobid, partial_r, false, &full_removal);
    ok (rc == 0, "reapi_cli_partial_cancel with partial R succeeds");
    // Note: full_removal depends on whether the allocation had multiple ranks
    diag ("partial_cancel full_removal=%s", full_removal ? "true" : "false");

    free (partial_r);
    free (R);

    // Test with nonexistent job and noent_ok=false
    const char *dummy_r = "{\"version\":1,\"execution\":{\"R_lite\":[{\"rank\":\"0\"}]}}";
    errno = 0;
    rc = reapi_cli_partial_cancel (ctx, 99999, dummy_r, false, &full_removal);
    ok (rc == -1 && errno == ENOENT,
        "reapi_cli_partial_cancel returns -1 with errno=ENOENT for nonexistent job");

    // Test with nonexistent job and noent_ok=true
    errno = 0;
    rc = reapi_cli_partial_cancel (ctx, 99999, dummy_r, true, &full_removal);
    ok (rc == 0, "reapi_cli_partial_cancel succeeds with noent_ok=true for nonexistent job");

    // Test with NULL context
    errno = 0;
    rc = reapi_cli_partial_cancel (NULL, 1, dummy_r, false, &full_removal);
    ok (rc == -1 && errno == EINVAL,
        "reapi_cli_partial_cancel returns -1 with errno=EINVAL for NULL context");

    // Test with NULL R
    errno = 0;
    rc = reapi_cli_partial_cancel (ctx, 1, NULL, false, &full_removal);
    ok (rc == -1 && errno == EINVAL,
        "reapi_cli_partial_cancel returns -1 with errno=EINVAL for NULL R");

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

    // Test find with an unknown format (must be rejected, not silently
    // defaulted to the simple writer)
    R_found = NULL;
    errno = 0;
    rc = reapi_cli_find (ctx, "sched-now=allocated", "bogus_format", &R_found);
    ok (rc == -1 && errno == EINVAL,
        "reapi_cli_find returns -1 with errno=EINVAL for unknown format");

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
    test_status_by_rank ();
    test_status_by_path ();
    test_null_parameters ();
    test_cancel_ex ();
    test_find ();

    done_testing ();
    return EXIT_SUCCESS;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

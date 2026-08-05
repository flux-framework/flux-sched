#!/usr/bin/env python3
###############################################################
# Copyright 2026 Lawrence Livermore National Security, LLC
# (c.f. AUTHORS, NOTICE.LLNS, COPYING)
#
# This file is part of the Flux resource manager framework.
# For details, see https://github.com/flux-framework.
#
# SPDX-License-Identifier: LGPL-3.0
###############################################################

"""CFFI build script for _reapi_cli extension module."""

from cffi import FFI

ffi = FFI()

# Set source - this tells CFFI to compile a real extension module
# that links against libreapi_cli.so
ffi.set_source(
    "_fluxion._reapi_cli",
    """
    #include <flux/core.h>
    #include <reapi_cli.h>
    """,
    libraries=["reapi_cli", "flux-core"],
)

# C declarations from reapi_cli.h
cdefs = """
    typedef enum match_op_t {
        MATCH_UNKNOWN,
        MATCH_ALLOCATE,
        MATCH_ALLOCATE_W_SATISFIABILITY,
        MATCH_ALLOCATE_ORELSE_RESERVE,
        MATCH_SATISFIABILITY
    } match_op_t;

    typedef enum resource_status_t {
        RESOURCE_UP = 0,
        RESOURCE_DOWN = 1
    } resource_status_t;

    typedef struct reapi_cli_ctx reapi_cli_ctx_t;

    reapi_cli_ctx_t *reapi_cli_new(void);
    void reapi_cli_destroy(reapi_cli_ctx_t *ctx);
    int reapi_cli_initialize(reapi_cli_ctx_t *ctx, const char *rgraph, const char *options);
    reapi_cli_ctx_t *reapi_cli_clone(reapi_cli_ctx_t *ctx);

    int reapi_cli_match_with_jobid(reapi_cli_ctx_t *ctx,
                                   match_op_t match_op,
                                   const char *jobspec,
                                   uint64_t jobid,
                                   bool *reserved,
                                   char **R,
                                   int64_t *at,
                                   double *ov);


    int reapi_cli_update_allocate(reapi_cli_ctx_t *ctx,
                                  const uint64_t jobid,
                                  const char *R,
                                  int64_t *at,
                                  double *ov,
                                  const char **R_out);

    int reapi_cli_cancel(reapi_cli_ctx_t *ctx,
                        const uint64_t jobid,
                        bool noent_ok);

    int reapi_cli_partial_cancel(reapi_cli_ctx_t *ctx,
                                 const uint64_t jobid,
                                 const char *R,
                                 bool noent_ok,
                                 bool *full_removal);

    int reapi_cli_info(reapi_cli_ctx_t *ctx,
                       const uint64_t jobid,
                       char **mode,
                       bool *reserved,
                       int64_t *at,
                       double *ov);

    int reapi_cli_find(reapi_cli_ctx_t *ctx,
                       const char *criteria,
                       const char *format,
                       char **R);

    const char *reapi_cli_get_err_msg(reapi_cli_ctx_t *ctx);
    void reapi_cli_clear_err_msg(reapi_cli_ctx_t *ctx);

    int reapi_cli_set_status(reapi_cli_ctx_t *ctx,
                             const char *resource_path,
                             resource_status_t status);

    int reapi_cli_set_rank_status(reapi_cli_ctx_t *ctx,
                                  const char *ranks,
                                  resource_status_t status);

    int reapi_cli_get_status(reapi_cli_ctx_t *ctx,
                             const char *resource_path,
                             resource_status_t *status);

    int reapi_cli_get_rank_status(reapi_cli_ctx_t *ctx,
                                  const char *rank,
                                  resource_status_t *status);

    void free(void *ptr);
"""

ffi.cdef(cdefs)

if __name__ == "__main__":
    ffi.emit_c_code("_reapi_cli.c")

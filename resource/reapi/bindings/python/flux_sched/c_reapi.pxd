from libc.stdint cimport int64_t, uint64_t, uintptr_t
from libcpp cimport bool

cdef extern from "flux/core.h":
    pass


cdef extern from "resource/policies/base/match_op.h":
    ctypedef enum match_op_t:
        MATCH_ALLOCATE
        MATCH_ALLOCATE_ORELSE_RESERVE
        MATCH_SATISFIABILITY
        MATCH_ALLOCATE_W_SATISFIABILITY

cdef extern from "c/reapi_cli.h":
    ctypedef struct reapi_cli_ctx_t:
        pass

    reapi_cli_ctx_t *reapi_cli_new()
    void reapi_cli_destroy(reapi_cli_ctx_t *ctx)
    int reapi_cli_initialize(reapi_cli_ctx_t *ctx, const char *rgraph, const char *options)

    int reapi_cli_match(reapi_cli_ctx_t *ctx, match_op_t match_op, const char *jobspec,
                        uint64_t *jobid, bool *reserved, char **R, int64_t *at, double *ov)

    int reapi_cli_update_allocate(reapi_cli_ctx_t *ctx, uint64_t jobid, const char *R,
                                  int64_t *at, double *ov, const char **R_out)

    int reapi_cli_cancel(reapi_cli_ctx_t *ctx, uint64_t jobid, bool noent_ok)
    int reapi_cli_partial_cancel(reapi_cli_ctx_t *ctx,
                                 uint64_t jobid,
                                 const char *R,
                                 bool noent_ok,
                                 bool *full_removal)

    int reapi_cli_info(reapi_cli_ctx_t *ctx, uint64_t jobid, char **mode,
                       bool *reserved, int64_t *at, double *ov)

    int reapi_cli_stat(reapi_cli_ctx_t *ctx, int64_t *V, int64_t *E, int64_t *J,
                       double *load, double *min, double *max, double *avg)

    const char *reapi_cli_get_err_msg(reapi_cli_ctx_t *ctx)
    void reapi_cli_clear_err_msg(reapi_cli_ctx_t *ctx)

cdef extern from "c/reapi_module.h":
    ctypedef struct reapi_module_ctx_t:
        pass

    reapi_module_ctx_t *reapi_module_new()
    void reapi_module_destroy(reapi_module_ctx_t *ctx)
    int reapi_module_set_handle(reapi_module_ctx_t *ctx, void *handle)

    int reapi_module_match(reapi_module_ctx_t *ctx, match_op_t match_op, const char *jobspec,
                           uint64_t jobid, bool *reserved, char **R, int64_t *at, double *ov)

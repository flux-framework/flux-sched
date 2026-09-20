# resource/reapi/bindings/python/flux_sched/reapi_cli.pyx

from libc.stdlib cimport free
from libc.stdint cimport int64_t, uint64_t
from libcpp cimport bool

# FIX: Use absolute import
from flux_sched.c_reapi cimport *

class ReapiError(Exception):
    pass

cdef class ReapiCli:
    cdef reapi_cli_ctx_t *_ctx

    def __cinit__(self):
        self._ctx = reapi_cli_new()
        if self._ctx is NULL:
            raise MemoryError("Failed to allocate reapi_cli context")

    def __dealloc__(self):
        if self._ctx is not NULL:
            reapi_cli_destroy(self._ctx)

    def initialize(self, str rgraph, str options="{}"):
        cdef bytes rgraph_b = rgraph.encode('utf-8')
        cdef bytes options_b = options.encode('utf-8')

        # 2. Create C pointers to the bytes' internal buffers
        cdef const char *rgraph_c = rgraph_b
        cdef const char *options_c = options_b

        rc = reapi_cli_initialize(self._ctx, rgraph_c, options_c)
        if rc != 0:
            self._raise_error()

    def match(self, str jobspec, bint orelse_reserve=False):
        """
        Returns: (jobid, reserved, R, at, overhead)
        """
        cdef:
            match_op_t op = MATCH_ALLOCATE_ORELSE_RESERVE if orelse_reserve else MATCH_ALLOCATE
            bytes jobspec_b = jobspec.encode('utf-8')
            uint64_t jobid = 0
            bool reserved = False
            char *R = NULL
            int64_t at = 0
            double ov = 0.0
            int rc

        rc = reapi_cli_match(self._ctx, op, jobspec_b, &jobid, &reserved, &R, &at, &ov)

        if rc != 0:
            self._raise_error()

        try:
            # Convert C string to Python string
            R_str = R.decode('utf-8') if R is not NULL else ""
            return (jobid, reserved, R_str, at, ov)
        finally:
            # Important: Free the memory allocated by strdup in C++
            if R is not NULL:
                free(R)

    def info(self, uint64_t jobid):
        cdef:
            char *mode = NULL
            bool reserved = False
            int64_t at = 0
            double ov = 0.0
            int rc

        rc = reapi_cli_info(self._ctx, jobid, &mode, &reserved, &at, &ov)
        if rc != 0:
            self._raise_error()

        try:
            mode_str = mode.decode('utf-8') if mode is not NULL else ""
            return (mode_str, reserved, at, ov)
        finally:
            if mode is not NULL:
                free(mode)

    cdef _raise_error(self):
        cdef const char *msg_c = reapi_cli_get_err_msg(self._ctx)
        cdef str msg = "Unknown Error"

        # Check msg_c, copy to python str, then free msg_c
        if msg_c is not NULL:
            try:
                msg = msg_c.decode('utf-8')
            finally:
                # The C interface returns a strdup'd string that we own
                # We cast away const to free it
                free(<void*>msg_c)

        reapi_cli_clear_err_msg(self._ctx)
        raise ReapiError(msg)

# ... (inside class ReapiCli) ...

    def cancel(self, uint64_t jobid, bool noent_ok=False):
        """
        Cancel the allocation or reservation for the given jobid.
        """
        cdef int rc
        rc = reapi_cli_cancel(self._ctx, jobid, noent_ok)
        if rc != 0:
            self._raise_error()

    def partial_cancel(self, uint64_t jobid, str R, bool noent_ok=False):
        """
        Partially cancel resources for the given jobid.

        Args:
            jobid: The job ID.
            R: The resource set (string/json) to release.
            noent_ok: If True, ignore error if jobid doesn't exist.

        Returns:
            bool: True if the job was fully removed (became empty), False otherwise.
        """
        cdef:
            bytes R_b = R.encode('utf-8')
            const char *R_c = R_b
            bool full_removal = False
            int rc

        rc = reapi_cli_partial_cancel(self._ctx, jobid, R_c, noent_ok, &full_removal)

        if rc != 0:
            self._raise_error()

        return full_removal

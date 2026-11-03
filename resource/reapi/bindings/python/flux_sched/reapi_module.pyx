# resource/reapi/bindings/python/flux_sched/reapi_module.pyx

from libc.stdlib cimport free
from libc.stdint cimport int64_t, uint64_t, uintptr_t
from libcpp cimport bool
from flux_sched.c_reapi cimport *

cdef class ReapiModule:
    cdef reapi_module_ctx_t *_ctx

    def __cinit__(self):
        self._ctx = reapi_module_new()
        if self._ctx is NULL:
            raise MemoryError("Failed to allocate reapi_module context")

    def __dealloc__(self):
        if self._ctx is not NULL:
            reapi_module_destroy(self._ctx)

    def set_handle(self, flux_handle):
        """
        Accepts a python flux handle (flux.Flux) OR a raw integer address.
        """
        cdef void *ptr
        cdef uintptr_t val
        cdef object ffi = None

        # 1. Fast Path: Integer (Does not require flux-python)
        if isinstance(flux_handle, int):
            val = <uintptr_t>flux_handle
            ptr = <void *>val
            reapi_module_set_handle(self._ctx, ptr)
            return

        # 2. Check for flux-python availability
        try:
            import flux
        except ImportError:
            raise ImportError(
                "The 'flux' package is required to use set_handle() with a Flux object. "
                "Please install with: pip install flux-sched[reapi-module]"
            )

        # 3. Locate CFFI 'ffi' instance (Logic from previous step)
        # Attempt A: flux.core.inner
        if ffi is None:
            try:
                from flux.core.inner import ffi as _f
                if hasattr(_f, 'cast'): ffi = _f
            except ImportError: pass

        # Attempt B: flux._core
        if ffi is None:
            try:
                from flux._core import ffi as _f
                if hasattr(_f, 'cast'): ffi = _f
            except ImportError: pass

        # Attempt C: Handle's own FFI
        if ffi is None and hasattr(flux_handle, "_ffi"):
            if hasattr(flux_handle._ffi, 'cast'): ffi = flux_handle._ffi

        # 4. Perform Cast
        if ffi is not None:
            try:
                if hasattr(flux_handle, "_handle"):
                    val = <uintptr_t>int(ffi.cast("uintptr_t", flux_handle._handle))
                else:
                    val = <uintptr_t>int(ffi.cast("uintptr_t", flux_handle))

                ptr = <void *>val
                reapi_module_set_handle(self._ctx, ptr)
                return
            except Exception as e:
                raise ValueError(f"Failed to cast handle: {e}")

        raise ValueError("Could not locate valid 'ffi' object. Try passing int(handle) directly.")

    # ... match/cancel methods remain the same ...

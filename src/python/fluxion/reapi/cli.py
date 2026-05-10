###############################################################
# Copyright 2026 Lawrence Livermore National Security, LLC
# (c.f. AUTHORS, NOTICE.LLNS, COPYING)
#
# This file is part of the Flux resource manager framework.
# For details, see https://github.com/flux-framework.
#
# SPDX-License-Identifier: LGPL-3.0
###############################################################

"""Python bindings for Fluxion's standalone Resource API (REAPI).

This module provides Python access to Fluxion's graph-based resource matching
algorithm. The primary interface is :class:`Reapi`, which manages a resource
graph and provides methods for matching, allocating, and freeing resources.
"""

import errno as _errno_mod
import json

from flux.constants import FLUX_NODEID_ANY

# Import from compiled extension module (API mode)
from _fluxion._reapi_cli import ffi, lib as _lib


class ReapiError(Exception):
    """Base exception for REAPI errors.

    Raised when a REAPI operation fails for reasons other than resource
    availability or request feasibility.
    """

    pass


class ReapiInsufficientResources(ReapiError):
    """Resources are temporarily unavailable.

    Raised by :meth:`Reapi.match_allocate` when a request cannot currently be
    satisfied because resources are in use by other jobs.
    """

    pass


class ReapiInfeasibleRequest(ReapiError):
    """Request is unsatisfiable.

    Raised by :meth:`Reapi.match_satisfiability` when a request cannot be
    satisfied even if all resources were free.  :meth:`Reapi.match_allocate`
    always raises :class:`ReapiInsufficientResources` on failure; use
    :meth:`Reapi.match_satisfiability` to distinguish structural infeasibility
    from temporary unavailability.
    """

    pass


class Reapi:
    """Fluxion resource graph matching interface.

    A :class:`Reapi` manages a resource graph and provides methods for
    matching job specifications against available resources. Each instance
    maintains its own independent graph state and allocation tracking.
    The instance must be initialized with a resource graph before use.

    Formats
    -------

    Two independent format axes control serialization:

    **load_format** (passed to :meth:`initialize`) selects the reader used
    to parse the resource graph string.  Valid values:

    - ``"grug"``: GraphML with generation rules (default)
    - ``"jgf"``: bare JGF ``{"graph": {"nodes": [...], "edges": [...]}}``
      (RFC 40); note this is the bare graph object, not an RV1 envelope
    - ``"jgf_shorthand"``: JGF shorthand notation
    - ``"rv1exec"``: RFC 20 envelope ``{"version":1, "execution": {"R_lite":
      [...]}}``; ``scheduling`` key is tolerated but ignored
    - ``"hwloc"``: hwloc XML (single node)

    **match_format** (passed to :meth:`initialize`) selects the writer used
    to serialize R strings returned by :meth:`match_allocate`.  Valid values:

    - ``"jgf"``: bare JGF object (default)
    - ``"jgf_shorthand"``: JGF shorthand notation
    - ``"rv1"``: RFC 20 envelope with execution and scheduling sections
    - ``"rv1_nosched"``: RFC 20 envelope with execution section only
    - ``"rv1_shorthand"``: RFC 20 shorthand notation
    - ``"rlite"``: compact format
    - ``"simple"``: human-readable text
    - ``"pretty_simple"``: human-readable text, indented

    For partial cancel, :meth:`cancel` parses the R string using the rv1exec
    reader by default, which accepts any RFC 20 envelope (ignoring the
    scheduling key).  This means partial cancel works with ``"rv1"``,
    ``"rv1_nosched"``, and ``"rv1exec"`` match formats by default.  Pass
    ``format`` to :meth:`cancel` to override the reader.  Partial cancel is
    not supported for ``"jgf"``, ``"rlite"``, ``"simple"``, or
    ``"pretty_simple"`` match formats.

    Lifecycle
    ---------

    ::

        ctx = Reapi()
        ctx.initialize(graph_json, load_format="jgf")

        result = ctx.match_allocate(1, jobspec_json)

        ctx.cancel(1)  # releases all resources for job 1

    Attributes
    ----------
    _ctx : ffi.CData
        Opaque pointer to underlying C context (internal)
    _initialized : bool
        Whether initialize() has been called (internal)
    """

    def __init__(self):
        """Create a new REAPI instance.

        The instance is initially uninitialized. Call :meth:`initialize` with a
        resource graph before performing any matching operations.

        Raises:
            ReapiError: If the underlying interface cannot be created.
        """
        self._ctx = _lib.reapi_cli_new()
        if self._ctx == ffi.NULL:
            raise ReapiError("Failed to create reapi_cli context")
        self._initialized = False

    def __del__(self):
        if hasattr(self, "_ctx") and self._ctx != ffi.NULL:
            _lib.reapi_cli_destroy(self._ctx)
            self._ctx = ffi.NULL

    def clone(self):
        """Return an independent deep copy of this context.

        The clone shares no state with the original: allocations, cancellations,
        and status changes made to either context do not affect the other.
        Intended for forward simulation — run scheduling decisions against the
        clone without disturbing the live context.

        Returns:
            Reapi: A new, fully independent :class:`Reapi` instance.

        Raises:
            ReapiError: If not initialized or the clone cannot be created.
        """
        if not self._initialized:
            raise ReapiError("Not initialized")
        new_obj = Reapi.__new__(Reapi)
        new_obj._ctx = _lib.reapi_cli_clone(self._ctx)
        if new_obj._ctx == ffi.NULL:
            raise ReapiError("Failed to clone reapi_cli context")
        new_obj._initialized = True
        return new_obj

    def _clear_err_message(self):
        _lib.reapi_cli_clear_err_msg(self._ctx)

    def _get_err_message(self):
        msg_ptr = _lib.reapi_cli_get_err_msg(self._ctx)
        if msg_ptr != ffi.NULL:
            msg = ffi.string(msg_ptr).decode("utf-8")
            _lib.free(msg_ptr)
            return msg
        return ""

    def _check_error(self, rc):
        """Raise an appropriate exception if rc < 0, embedding the C error message."""
        if rc < 0:
            err_no = ffi.errno
            err_msg = self._get_err_message()

            if err_no == _errno_mod.EBUSY:
                raise ReapiInsufficientResources(f"Resources unavailable: {err_msg}")
            elif err_no == _errno_mod.ENODEV:
                raise ReapiInfeasibleRequest(f"Infeasible request: {err_msg}")
            else:
                raise ReapiError(f"REAPI error (errno={err_no}): {err_msg}")

    def initialize(
        self, rgraph, load_format="grug", match_format="jgf", match_policy=None
    ):
        """Initialize with a resource graph.

        Must be called before any match or allocation operations.
        See class docstring for valid ``load_format`` and ``match_format`` values.

        Args:
            rgraph (dict or str): Resource graph. Dicts are JSON-encoded
                automatically. Format determined by ``load_format``.
            load_format (str, optional): Reader format for ``rgraph``
                (default ``"grug"``).
            match_format (str, optional): Writer format for R strings returned
                by :meth:`match_allocate` (default ``"jgf"``).
            match_policy (str, optional): Matcher policy (default ``"first"``).
                Valid values: ``"first"``, ``"firstnodex"``, ``"high"``,
                ``"low"``, ``"lonode"``, ``"hinode"``, ``"lonodex"``,
                ``"hinodex"``.

        Raises:
            ReapiError: If initialization fails (invalid format, parse error,
                etc.)

        Examples:
            ::

                jgf = {"graph": {"nodes": [...], "edges": [...]}}
                ctx.initialize(jgf, load_format="jgf", match_format="rv1_nosched")
        """
        self._clear_err_message()

        # Handle rgraph - accept dict or string
        if isinstance(rgraph, dict):
            rgraph_str = json.dumps(rgraph)
        else:
            rgraph_str = rgraph
        rgraph_bytes = rgraph_str.encode("utf-8")

        opts = {"load_format": load_format, "match_format": match_format}
        if match_policy is not None:
            opts["matcher_policy"] = match_policy
        options_str = json.dumps(opts)
        options_bytes = options_str.encode("utf-8")

        rc = _lib.reapi_cli_initialize(self._ctx, rgraph_bytes, options_bytes)
        self._check_error(rc)
        self._initialized = True

    def match_allocate(self, jobid, jobspec, orelse_reserve=False):
        """Match a jobspec against the resource graph and allocate resources.

        Attempts to find resources that satisfy the given jobspec and allocates them
        for exclusive use.

        Args:
            jobid (int): Job ID assigned by the caller. Must be unique across all
                active allocations. Use the same value with :meth:`cancel`.
            jobspec (dict or str): Flux jobspec (version 1). If dict, will be
                JSON-encoded automatically. If str, assumed to be already JSON-encoded.
                Must include a ``resources`` array describing required resources and a
                ``tasks`` array describing task placement.
            orelse_reserve (bool, optional): If True, make a reservation at a future
                time if resources are not currently available. If False (default),
                only attempt immediate allocation.

        Returns:
            dict: Dictionary with the following keys:

                - ``reserved`` (bool): True if reserved rather than immediately
                  allocated.
                - ``R`` (dict): Allocated resources in the ``match_format``
                  specified at initialization.  A subset can be passed to
                  :meth:`cancel` for partial release.
                - ``at`` (int): 0 for immediate allocation; future timestamp
                  for reservations.
                - ``overhead`` (float): Time in seconds taken by the match
                  operation.

        Raises:
            InsufficientResourcesError: Resources are unavailable (includes
                structurally infeasible requests — use :meth:`match_satisfiability`
                to distinguish).
            ReapiError: If not initialized or a general error occurs.

        Examples:
            ::

                try:
                    result = ctx.match_allocate(jobid, jobspec)
                    R = result["R"]
                    # ... use allocated resources ...
                    ctx.cancel(jobid)
                except InsufficientResourcesError:
                    print("Resources temporarily unavailable, try again later")
        """
        if not self._initialized:
            raise ReapiError("Not initialized")
        self._clear_err_message()

        if isinstance(jobspec, dict):
            jobspec_str = json.dumps(jobspec)
        else:
            jobspec_str = jobspec
        jobspec_bytes = jobspec_str.encode("utf-8")

        match_op = (
            _lib.MATCH_ALLOCATE_ORELSE_RESERVE
            if orelse_reserve
            else _lib.MATCH_ALLOCATE
        )
        reserved_out = ffi.new("bool *")
        R_out = ffi.new("char **")
        at_out = ffi.new("int64_t *")
        ov_out = ffi.new("double *")

        rc = _lib.reapi_cli_match_with_jobid(
            self._ctx,
            match_op,
            jobspec_bytes,
            jobid,
            reserved_out,
            R_out,
            at_out,
            ov_out,
        )
        self._check_error(rc)

        result = {
            "reserved": reserved_out[0],
            "at": at_out[0],
            "overhead": ov_out[0],
        }
        if R_out[0] != ffi.NULL:
            result["R"] = json.loads(ffi.string(R_out[0]).decode("utf-8"))
            _lib.free(R_out[0])
        return result

    def match_satisfiability(self, jobspec):
        """Check if a jobspec could be satisfied with available resources.

        Performs a satisfiability check to determine if the resource graph contains
        sufficient resources (both types and quantities) to satisfy the request if
        all resources were free. This is useful for validating jobspecs before queueing.

        This operation does not allocate resources or modify graph state.

        Args:
            jobspec (dict or str): Flux jobspec to check. If dict, will be
                JSON-encoded automatically. If str, assumed to be already JSON-encoded.

        Returns:
            dict: Dictionary with key:

                - ``overhead`` (float): Time in seconds taken by the check.

        Raises:
            InfeasibleRequestError: Request is unsatisfiable.
            ReapiError: If not initialized or a general error occurs.

        Examples:
            ::

                try:
                    result = ctx.match_satisfiability(jobspec)
                    print("Request is satisfiable")
                except InfeasibleRequestError:
                    print("Request is unsatisfiable")
        """
        if not self._initialized:
            raise ReapiError("Not initialized")
        self._clear_err_message()

        if isinstance(jobspec, dict):
            jobspec_str = json.dumps(jobspec)
        else:
            jobspec_str = jobspec
        jobspec_bytes = jobspec_str.encode("utf-8")

        reserved_out = ffi.new("bool *")
        R_out = ffi.new("char **")
        at_out = ffi.new("int64_t *")
        ov_out = ffi.new("double *")

        rc = _lib.reapi_cli_match_with_jobid(
            self._ctx,
            _lib.MATCH_SATISFIABILITY,
            jobspec_bytes,
            0,
            reserved_out,
            R_out,
            at_out,
            ov_out,
        )
        self._check_error(rc)
        if R_out[0] != ffi.NULL:
            _lib.free(R_out[0])
        return {"overhead": ov_out[0]}

    def update_allocate(self, jobid, R):
        """Re-register an existing allocation from a serialized R string.

        Used to replay allocations when a scheduler is reloaded while jobs
        are running.  The resource graph is updated to reflect the existing
        allocation without performing a new match.

        Timing (start time, duration) is extracted automatically from the R
        string.  For RFC 20 envelopes (``rv1``, ``rv1_nosched``, ``rv1exec``),
        ``execution.starttime`` and ``execution.expiration`` are used.  The
        reader format is also derived from the R content: if a ``scheduling``
        JGF section is present the JGF reader is used; otherwise ``rv1exec``.
        For bare JGF (``match_format="jgf"``), start time defaults to 0 and
        duration is unlimited.

        Args:
            jobid (int): Job ID to register.  Must not already be registered.
            R (dict or str): Resource set previously allocated to this job.
                If dict, will be JSON-encoded automatically.

        Returns:
            dict: Dictionary with keys:

                - ``at`` (int): Scheduled start time parsed from R (0 for
                  immediate / bare JGF).
                - ``overhead`` (float): Time taken by the update operation.
                - ``R`` (dict): Updated resource set in the ``match_format``
                  specified at initialization.

        Raises:
            ReapiError: If not initialized, the jobid already exists
                (errno ``EEXIST``), the R string is invalid, or the
                operation fails.

        Examples:
            ::

                # Replay a running job whose R was saved from match_allocate
                result = ctx.update_allocate(jobid, saved_R)
                print(f"Re-registered job {jobid} starting at {result['at']}")
        """
        if not self._initialized:
            raise ReapiError("Not initialized")
        self._clear_err_message()

        if isinstance(R, dict):
            R_str = json.dumps(R)
        else:
            R_str = R
        R_bytes = R_str.encode("utf-8")

        at_out = ffi.new("int64_t *")
        ov_out = ffi.new("double *")
        R_out = ffi.new("char **")

        rc = _lib.reapi_cli_update_allocate(
            self._ctx,
            jobid,
            R_bytes,
            at_out,
            ov_out,
            R_out,
        )
        self._check_error(rc)

        result = {"at": at_out[0], "overhead": ov_out[0]}
        if R_out[0] != ffi.NULL:
            result["R"] = json.loads(ffi.string(R_out[0]).decode("utf-8"))
            _lib.free(R_out[0])
        return result

    def cancel(self, jobid, R=None, format=None, noent_ok=False):
        """Cancel (free) a job allocation.

        Releases resources from an allocation identified by jobid.  Pass R to
        do a partial release; omit it to release all resources for the job.
        See class docstring for which match formats support partial cancel.

        Args:
            jobid (int): Job ID previously passed to :meth:`match_allocate`.
            R (None, dict, or str): Resource subset to free, or None to free
                all resources for the job.
            format (str, optional): Reader format for parsing R (default:
                use the initialize load_format).  Only meaningful when R is
                provided.
            noent_ok (bool, optional): If True, silently succeed if the job
                doesn't exist (default False).

        Returns:
            bool: True if all resources were freed (job is gone), False if
                some resources remain allocated.

        Raises:
            ReapiError: If not initialized, the job doesn't exist
                (and noent_ok is False), or the operation fails.

        Examples:
            ::

                jobid = 1
                result = ctx.match_allocate(jobid, two_node_jobspec)

                # Release a subset (R_subset is a one-node portion as dict)
                full_removal = ctx.cancel(jobid, R_subset, "rv1exec")

                if not full_removal:
                    ctx.cancel(jobid)  # release remainder
        """
        if not self._initialized:
            raise ReapiError("Not initialized")
        self._clear_err_message()

        R_cdata = (
            ffi.NULL
            if R is None
            else (
                json.dumps(R).encode("utf-8")
                if isinstance(R, dict)
                else R.encode("utf-8")
            )
        )
        format_cdata = ffi.NULL if format is None else format.encode("utf-8")

        full_removal_out = ffi.new("bool *")
        rc = _lib.reapi_cli_cancel_ex(
            self._ctx, jobid, R_cdata, format_cdata, noent_ok, full_removal_out
        )
        self._check_error(rc)
        return full_removal_out[0]

    def set_status(self, *, path=None, rank=None, up):
        """Set resource status to up or down.

        Marks a resource or all resources at a rank as either available (up)
        or unavailable (down). Exactly one of ``path`` or ``rank`` must be specified.

        Args:
            path (str, optional): Resource path (e.g., ``"/cluster0/node0"``).
            rank (int, optional): Rank number. Use ``FLUX_NODEID_ANY`` to mark all ranks.
            up (bool): ``True`` to mark available, ``False`` to mark unavailable.

        Raises:
            ReapiError: If not initialized, the resource/rank is not found,
                or parameters are invalid.
            ValueError: If both or neither of ``path`` and ``rank`` are provided.

        Examples:
            ::

                # Mark specific resource path down
                ctx.set_status(path="/cluster0/node0", up=False)

                # Mark all resources on rank 0 as down
                ctx.set_status(rank=0, up=False)

                # Mark all ranks down
                ctx.set_status(rank=FLUX_NODEID_ANY, up=False)

                # Bring rank 0 back up
                ctx.set_status(rank=0, up=True)
        """
        if not self._initialized:
            raise ReapiError("Not initialized")
        self._clear_err_message()

        if (path is None and rank is None) or (path is not None and rank is not None):
            raise ValueError("Exactly one of 'path' or 'rank' must be specified")

        status_str = "UP" if up else "DOWN"
        status_bytes = status_str.encode("utf-8")

        if path is not None:
            # Path-based
            path_bytes = path.encode("utf-8")
            rc = _lib.reapi_cli_set_status(self._ctx, path_bytes, status_bytes)
        else:
            # Rank-based
            rc = _lib.reapi_cli_set_status_by_rank(self._ctx, rank, status_bytes)

        self._check_error(rc)

    def get_status(self, *, path=None, rank=None):
        """Get resource status.

        Query whether a resource or rank is marked as available (up) or
        unavailable (down). Exactly one of ``path`` or ``rank`` must be specified.

        Args:
            path (str, optional): Resource path (e.g., ``"/cluster0/node0"``).
            rank (int, optional): Rank number.

        Returns:
            bool: ``True`` if resource is up (available), ``False`` if down (unavailable).

        Raises:
            ReapiError: If not initialized, the resource/rank is not found,
                or parameters are invalid (e.g., ``FLUX_NODEID_ANY`` for rank query).
            ValueError: If both or neither of ``path`` and ``rank`` are provided.

        Examples:
            ::

                # Query by path
                is_up = ctx.get_status(path="/cluster0/node0")

                # Query by rank
                is_up = ctx.get_status(rank=0)
        """
        if not self._initialized:
            raise ReapiError("Not initialized")
        self._clear_err_message()

        if (path is None and rank is None) or (path is not None and rank is not None):
            raise ValueError("Exactly one of 'path' or 'rank' must be specified")

        status_out = ffi.new("const char **")

        if path is not None:
            # Path-based
            path_bytes = path.encode("utf-8")
            rc = _lib.reapi_cli_get_status(self._ctx, path_bytes, status_out)
        else:
            # Rank-based
            rc = _lib.reapi_cli_get_status_by_rank(self._ctx, rank, status_out)

        self._check_error(rc)
        status_str = ffi.string(status_out[0]).decode("utf-8")
        return status_str == "UP"

    def info(self, jobid):
        """Get information about an allocated or reserved job.

        Retrieves the current state and timing information for a job that has
        been allocated or reserved in this context.

        Args:
            jobid (int): Job ID to query. Must be a jobid previously passed
                to :meth:`match_allocate`.

        Returns:
            dict: Dictionary with the following keys:

                - ``mode`` (str): Job state ("ALLOCATED", "RESERVED", etc.)
                - ``reserved`` (bool): True if this is a reservation rather than allocation
                - ``at`` (int): Allocation time. 0 for immediate allocation, future timestamp
                  for reservations.
                - ``overhead`` (float): Time in seconds taken by the original
                  match operation that created this allocation.
                - ``R`` (dict): Allocated resource set in the match_format specified
                  at initialization.  **Note:** after a partial :meth:`cancel`
                  this field still reflects the original full allocation; it is
                  not updated to reflect the released subset.

        Raises:
            ReapiError: If not initialized, the job doesn't exist,
                or the operation fails.

        Examples:
            ::

                jobid = 1
                result = ctx.match_allocate(jobid, jobspec)

                # Query job info
                info = ctx.info(jobid)
                print(f"Job {jobid} mode: {info['mode']}, R: {info['R']}")
        """
        if not self._initialized:
            raise ReapiError("Not initialized")
        self._clear_err_message()

        mode_out = ffi.new("const char **")
        reserved_out = ffi.new("bool *")
        at_out = ffi.new("int64_t *")
        ov_out = ffi.new("double *")
        R_out = ffi.new("const char **")

        rc = _lib.reapi_cli_info_ex(
            self._ctx, jobid, mode_out, reserved_out, at_out, ov_out, R_out
        )
        self._check_error(rc)

        result = {
            "mode": ffi.string(mode_out[0]).decode("utf-8")
            if mode_out[0] != ffi.NULL
            else "",
            "reserved": reserved_out[0],
            "at": at_out[0],
            "overhead": ov_out[0],
            "R": json.loads(ffi.string(R_out[0]).decode("utf-8"))
            if R_out[0] != ffi.NULL and ffi.string(R_out[0])
            else None,
        }

        return result

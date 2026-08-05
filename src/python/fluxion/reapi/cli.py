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

# Import from compiled extension module (API mode)
from _fluxion._reapi_cli import ffi, lib as _lib

# Export resource status constants
RESOURCE_UP = _lib.RESOURCE_UP
RESOURCE_DOWN = _lib.RESOURCE_DOWN


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
        # Module globals (_lib, ffi) may be torn down to None during
        # interpreter shutdown; guard against that to avoid noise on stderr.
        if _lib is None or ffi is None:
            return
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
            # Read errno before _get_err_message(): that call re-enters the C
            # layer and would clobber ffi.errno.  Order is load-bearing.
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
        # Re-initializing would leak the prior graph in the C layer
        # (reapi_cli_initialize overwrites ctx->rqt without freeing it).
        if self._initialized:
            raise ReapiError("Already initialized")
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
                - ``R`` (dict or None): Allocated resources in the
                  ``match_format`` specified at initialization, or ``None`` if
                  no resource set was returned.  A subset can be passed to
                  :meth:`cancel` for partial release.
                - ``at`` (int): 0 for immediate allocation; future timestamp
                  for reservations.
                - ``overhead`` (float): Time in seconds taken by the match
                  operation.

        Raises:
            ReapiInsufficientResources: Resources are unavailable (includes
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
                except ReapiInsufficientResources:
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
            "R": None,
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
            ReapiInfeasibleRequest: Request is unsatisfiable.
            ReapiError: If not initialized or a general error occurs.

        Examples:
            ::

                try:
                    result = ctx.match_satisfiability(jobspec)
                    print("Request is satisfiable")
                except ReapiInfeasibleRequest:
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

    def cancel(self, jobid, R=None, noent_ok=False):
        """Cancel (free) a job allocation.

        Releases resources from an allocation identified by jobid.  Pass R to
        do a partial release; omit it to release all resources for the job.
        The format is auto-detected from R per RFC 20/40.

        Args:
            jobid (int): Job ID previously passed to :meth:`match_allocate`.
            R (None, dict, or str): Resource subset to free, or None to free
                all resources for the job. Format is auto-detected.
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
                full_removal = ctx.cancel(jobid, R_subset)

                if not full_removal:
                    ctx.cancel(jobid)  # release remainder
        """
        if not self._initialized:
            raise ReapiError("Not initialized")
        self._clear_err_message()

        if R is None:
            # Full cancel - no R provided
            rc = _lib.reapi_cli_cancel(self._ctx, jobid, noent_ok)
            self._check_error(rc)
            return True
        else:
            # Partial cancel - R provided
            R_cdata = (
                json.dumps(R).encode("utf-8")
                if isinstance(R, dict)
                else R.encode("utf-8")
            )
            full_removal_out = ffi.new("bool *")
            rc = _lib.reapi_cli_partial_cancel(
                self._ctx, jobid, R_cdata, noent_ok, full_removal_out
            )
            self._check_error(rc)
            return full_removal_out[0]

    def set_status(self, path, status):
        """Set resource status by path.

        Marks a resource at the specified path with the given status.

        Args:
            path (str): Resource path (e.g., ``"/cluster0/node0"``).
            status (int): Resource status. Use ``RESOURCE_UP`` or ``RESOURCE_DOWN``
                from the module (e.g., ``from fluxion.reapi.cli import RESOURCE_UP``).

        Raises:
            ReapiError: If not initialized, the resource is not found,
                or parameters are invalid.

        Examples:
            ::

                from fluxion.reapi.cli import Reapi, RESOURCE_UP, RESOURCE_DOWN

                # Mark specific resource path down
                ctx.set_status("/cluster0/node0", RESOURCE_DOWN)

                # Bring it back up
                ctx.set_status("/cluster0/node0", RESOURCE_UP)
        """
        if not self._initialized:
            raise ReapiError("Not initialized")
        self._clear_err_message()

        path_bytes = path.encode("utf-8")
        rc = _lib.reapi_cli_set_status(self._ctx, path_bytes, status)
        self._check_error(rc)

    def get_status(self, path):
        """Get resource status by path.

        Query the status of a resource at the specified path.

        Args:
            path (str): Resource path (e.g., ``"/cluster0/node0"``).

        Returns:
            int: Resource status (``RESOURCE_UP`` or ``RESOURCE_DOWN``).

        Raises:
            ReapiError: If not initialized, the resource is not found,
                or parameters are invalid.

        Examples:
            ::

                from fluxion.reapi.cli import Reapi, RESOURCE_UP

                # Query by path
                status = ctx.get_status("/cluster0/node0")
                if status == RESOURCE_UP:
                    print("Resource is up")
        """
        if not self._initialized:
            raise ReapiError("Not initialized")
        self._clear_err_message()

        status_out = ffi.new("resource_status_t *")
        path_bytes = path.encode("utf-8")
        rc = _lib.reapi_cli_get_status(self._ctx, path_bytes, status_out)
        self._check_error(rc)
        return status_out[0]

    def set_rank_status(self, ranks, status):
        """Set resource status by rank.

        Marks all resources at specified ranks with the given status.

        Args:
            ranks (str or int): RFC 22 idset string (e.g., "0", "0-3", "0,2,4"),
                the special value "all", or an integer rank. Use "all" to mark all ranks.
            status (int): Resource status. Use ``RESOURCE_UP`` or ``RESOURCE_DOWN``
                from the module.

        Raises:
            ReapiError: If not initialized, a rank is not found,
                or parameters are invalid.

        Examples:
            ::

                from fluxion.reapi.cli import Reapi, RESOURCE_UP, RESOURCE_DOWN

                # Mark rank 0 down
                ctx.set_rank_status(0, RESOURCE_DOWN)

                # Mark ranks 0-3 down
                ctx.set_rank_status("0-3", RESOURCE_DOWN)

                # Mark all ranks down
                ctx.set_rank_status("all", RESOURCE_DOWN)

                # Bring rank 0 back up
                ctx.set_rank_status(0, RESOURCE_UP)
        """
        if not self._initialized:
            raise ReapiError("Not initialized")
        self._clear_err_message()

        # Convert int to string if needed
        if isinstance(ranks, int):
            ranks = str(ranks)
        ranks_bytes = ranks.encode("utf-8")
        rc = _lib.reapi_cli_set_rank_status(self._ctx, ranks_bytes, status)
        self._check_error(rc)

    def get_rank_status(self, rank):
        """Get resource status by rank.

        Query the status of resources at a specified rank.

        Args:
            rank (str or int): Single rank as string or integer. Ranges
                and "all" are not permitted.

        Returns:
            int: Resource status (``RESOURCE_UP`` or ``RESOURCE_DOWN``).

        Raises:
            ReapiError: If not initialized, the rank is not found,
                or parameters are invalid (e.g., "all" or ranges).

        Examples:
            ::

                from fluxion.reapi.cli import Reapi, RESOURCE_UP

                # Query by rank (int or string)
                status = ctx.get_rank_status(0)
                if status == RESOURCE_UP:
                    print("Rank is up")
        """
        if not self._initialized:
            raise ReapiError("Not initialized")
        self._clear_err_message()

        status_out = ffi.new("resource_status_t *")

        # Convert int to string if needed
        if isinstance(rank, int):
            rank = str(rank)
        rank_bytes = rank.encode("utf-8")
        rc = _lib.reapi_cli_get_rank_status(self._ctx, rank_bytes, status_out)
        self._check_error(rc)
        return status_out[0]

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

        Raises:
            ReapiError: If not initialized, the job doesn't exist,
                or the operation fails.

        Examples:
            ::

                jobid = 1
                result = ctx.match_allocate(jobid, jobspec)

                # Query job info
                info = ctx.info(jobid)
                print(f"Job {jobid} mode: {info['mode']}")
        """
        if not self._initialized:
            raise ReapiError("Not initialized")
        self._clear_err_message()

        mode_out = ffi.new("char **")
        reserved_out = ffi.new("bool *")
        at_out = ffi.new("int64_t *")
        ov_out = ffi.new("double *")

        rc = _lib.reapi_cli_info(
            self._ctx, jobid, mode_out, reserved_out, at_out, ov_out
        )
        self._check_error(rc)

        result = {
            "mode": ffi.string(mode_out[0]).decode("utf-8")
            if mode_out[0] != ffi.NULL
            else "",
            "reserved": reserved_out[0],
            "at": at_out[0],
            "overhead": ov_out[0],
        }

        _lib.free(mode_out[0])

        return result

    def find(self, criteria, format=None):
        """Find resources matching the specified criteria.

        Search the resource graph for resources matching the given criteria
        and return them in R format.

        Args:
            criteria (str): Search criteria string. Examples:
                - ``"sched-now=allocated"`` - Find all allocated resources
                - ``"status=down"`` - Find all down resources
                - ``"status=up"`` - Find all up resources
            format (str, optional): Output format string (e.g., "rv1_nosched", "jgf").
                If None, uses the context's configured match_format from initialization.

        Returns:
            dict or None: Dictionary containing matching resources in R format,
                or None if no resources matched the criteria.

        Raises:
            ReapiError: If not initialized or the operation fails.

        Examples:
            ::

                # Find all allocated resources
                R_allocated = ctx.find("sched-now=allocated")
                if R_allocated:
                    print(f"Allocated: {R_allocated}")

                # Find all down resources in rv1_nosched format
                R_down = ctx.find("status=down", format="rv1_nosched")
        """
        if not self._initialized:
            raise ReapiError("Not initialized")
        self._clear_err_message()

        criteria_bytes = criteria.encode("utf-8")
        format_bytes = format.encode("utf-8") if format else ffi.NULL
        R_out = ffi.new("char **")

        rc = _lib.reapi_cli_find(self._ctx, criteria_bytes, format_bytes, R_out)
        self._check_error(rc)

        if R_out[0] != ffi.NULL:
            result = json.loads(ffi.string(R_out[0]).decode("utf-8"))
            _lib.free(R_out[0])
            return result
        else:
            return None

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
from typing import NamedTuple, Optional, Union

# Import from compiled extension module (API mode)
from _fluxion._reapi_cli import ffi, lib as _lib

# Export resource status constants
RESOURCE_UP = _lib.RESOURCE_UP
RESOURCE_DOWN = _lib.RESOURCE_DOWN

# Match formats that emit text rather than JSON.  Resource sets in these
# formats are returned to the caller as str; all others are decoded.
_TEXT_FORMATS = frozenset(["simple", "pretty_simple"])

# Type of a resource set as returned by this module: decoded JSON (a dict,
# or a list for the "rlite" format) or text for the non-JSON formats.
ResourceSet = Union[dict, list, str]


class ReapiError(Exception):
    """Base exception for REAPI errors.

    Raised when a REAPI operation fails for reasons other than resource
    availability or request feasibility.

    Attributes:
        errno (int): The ``errno`` value reported by the underlying library.
    """

    def __init__(self, message, errno=0):
        super().__init__(message)
        self.errno = errno


class ReapiInsufficientResources(ReapiError):
    """Resources are temporarily unavailable.

    Raised by :meth:`Reapi.match_allocate` when a request cannot currently be
    satisfied because resources are in use by other jobs.
    """


class ReapiInfeasibleRequest(ReapiError):
    """Request is unsatisfiable.

    Raised when a request cannot be satisfied even if all resources were
    free.  See :meth:`Reapi.match_satisfiability` and the
    ``check_satisfiability`` parameter of :meth:`Reapi.match_allocate`.
    """


class MatchResult(NamedTuple):
    """Result of a match operation.

    Returned by :meth:`Reapi.match_allocate`,
    :meth:`Reapi.match_without_allocating`, :meth:`Reapi.match_satisfiability`
    and :meth:`Reapi.update_allocate`.  Every match operation returns the
    same fields; those that do not apply to a given operation take their
    default values.
    """

    #: Matched resource set in the ``match_format`` of the :class:`Reapi`
    #: instance, or ``None`` if the operation produced no resource set.
    R: Optional[ResourceSet] = None
    #: True if the resources were reserved for a future time rather than
    #: allocated now.
    reserved: bool = False
    #: Start time of the allocation or reservation (0 for an immediate
    #: allocation).
    at: int = 0
    #: Time in seconds taken by the operation.
    overhead: float = 0.0


class JobInfo(NamedTuple):
    """Information about a job known to a :class:`Reapi` instance.

    Returned by :meth:`Reapi.info`.
    """

    #: Job state, e.g. ``"ALLOCATED"``, ``"RESERVED"``, or ``"CANCELED"``.
    mode: str
    #: True if the job holds a reservation rather than an allocation.
    reserved: bool
    #: Start time of the allocation or reservation (0 for an immediate
    #: allocation).
    at: int
    #: Time in seconds taken by the match operation that created the job.
    overhead: float


class Reapi:
    """Fluxion resource graph matching interface.

    A :class:`Reapi` is created with a resource graph and provides methods
    for matching job specifications against it.  Each instance maintains its
    own independent graph state and allocation tracking.

    Instances hold resources in the underlying C library.  Call
    :meth:`close` when done, or use the instance as a context manager.

    Formats
    -------

    Two independent format axes control serialization:

    **load_format** selects the reader used to parse the resource graph
    passed to the constructor.  Valid values:

    - ``"jgf"``: bare JGF ``{"graph": {"nodes": [...], "edges": [...]}}``
      (RFC 40); note this is the bare graph object, not an RV1 envelope
      (default)
    - ``"jgf_shorthand"``: JGF shorthand notation
    - ``"rv1exec"``: RFC 20 envelope ``{"version":1, "execution": {"R_lite":
      [...]}}``; ``scheduling`` key is tolerated but ignored
    - ``"grug"``: GraphML with generation rules
    - ``"hwloc"``: hwloc XML (single node)

    **match_format** selects the writer used to serialize the resource sets
    returned by match operations and :meth:`find`.  Valid values:

    - ``"rv1_nosched"``: RFC 20 envelope with execution section only
      (default)
    - ``"rv1"``: RFC 20 envelope with execution and scheduling sections
    - ``"rv1_shorthand"``: RFC 20 shorthand notation
    - ``"jgf"``: bare JGF object
    - ``"jgf_shorthand"``: JGF shorthand notation
    - ``"rlite"``: the bare RFC 20 ``R_lite`` array
    - ``"simple"``: human-readable text
    - ``"pretty_simple"``: human-readable text, indented

    Resource sets in the JSON formats are decoded before being returned, so
    ``R`` is a dict (or a list for ``"rlite"``).  The text formats are
    returned as str.

    **Partial cancel** (:meth:`cancel` with ``R``) and
    :meth:`update_allocate` parse the resource set they are given per RFC 20:
    an envelope without a ``scheduling`` key is read from
    ``execution.R_lite``; an envelope with a ``scheduling`` key is read by
    the reader named in ``scheduling.writer`` (RFC 40).  Partial cancel
    requires an RFC 20 envelope and is only supported without the
    ``scheduling`` key: use the ``"rv1_nosched"`` match format, or remove
    the ``scheduling`` key from an ``"rv1"`` resource set before passing it.

    Lifecycle
    ---------

    ::

        with Reapi(jgf_graph) as ctx:
            result = ctx.match_allocate(1, jobspec)
            # ... use result.R ...
            ctx.cancel(1)  # releases all resources for job 1
    """

    def __init__(
        self,
        rgraph,
        load_format="jgf",
        match_format="rv1_nosched",
        matcher_policy=None,
        matcher_name=None,
        traverser_policy=None,
        prune_filters=None,
        load_allowlist=None,
        reserve_vtx_vec=None,
    ):
        """Create a REAPI instance from a resource graph.

        The keyword arguments correspond one to one with the initialization
        options of the ``reapi_cli`` C library.  An option left at ``None``
        takes the library default.  See the class docstring for valid
        ``load_format`` and ``match_format`` values.

        Args:
            rgraph (dict or str): Resource graph in the format given by
                ``load_format``.  Dicts are JSON-encoded automatically.
            load_format (str, optional): Reader format for ``rgraph``
                (default ``"jgf"``).
            match_format (str, optional): Writer format for returned
                resource sets (default ``"rv1_nosched"``).
            matcher_policy (str, optional): Match policy: ``"first"``
                (default), ``"firstnodex"``, ``"high"``, ``"low"``,
                ``"lonode"``, ``"hinode"``, ``"lonodex"``, ``"hinodex"``,
                or a custom policy string.
            matcher_name (str, optional): Subsystems to match against,
                e.g. ``"CA"`` (containment, the default), ``"IBA"``,
                ``"C+IBA"``.
            traverser_policy (str, optional): ``"simple"`` (default) or
                ``"flexible"``.
            prune_filters (str, optional): Pruning filter specification
                (default ``"ALL:core,ALL:node"``).
            load_allowlist (str, optional): Comma-separated resource types to
                load; the rest are dropped.  Not supported by all readers.
            reserve_vtx_vec (int, optional): Number of graph vertices to
                reserve space for up front.

        Raises:
            ReapiError: If an option is invalid or the graph cannot be
                loaded.

        Examples:
            ::

                jgf = {"graph": {"nodes": [...], "edges": [...]}}
                ctx = Reapi(jgf, match_format="rv1")
        """
        self._ctx = ffi.NULL
        self._match_format = match_format
        ctx = _lib.reapi_cli_new()
        if ctx == ffi.NULL:
            raise ReapiError("Failed to create reapi_cli context", ffi.errno)
        self._ctx = ctx

        options = {
            "load_format": load_format,
            "match_format": match_format,
            "matcher_policy": matcher_policy,
            "matcher_name": matcher_name,
            "traverser_policy": traverser_policy,
            "prune_filters": prune_filters,
            "load_allowlist": load_allowlist,
            "reserve_vtx_vec": reserve_vtx_vec,
        }
        options = {k: v for k, v in options.items() if v is not None}
        try:
            rc = _lib.reapi_cli_initialize(
                self._ctx, self._encode(rgraph), self._encode(options)
            )
            self._check_error(rc)
        except Exception:
            self.close()
            raise

    def close(self):
        """Release the underlying C context.

        Subsequent method calls raise :class:`ReapiError`.  Calling
        :meth:`close` more than once is harmless.
        """
        if self._ctx != ffi.NULL:
            _lib.reapi_cli_destroy(self._ctx)
            self._ctx = ffi.NULL

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    def __del__(self):
        # Module globals (_lib, ffi) may be torn down to None during
        # interpreter shutdown; guard against that to avoid noise on stderr.
        if _lib is None or ffi is None or not hasattr(self, "_ctx"):
            return
        self.close()

    def clone(self):
        """Return an independent deep copy of this instance.

        The clone shares no state with the original: allocations, cancellations,
        and status changes made to either do not affect the other.
        Intended for forward simulation: run scheduling decisions against the
        clone without disturbing the live instance.

        Returns:
            Reapi: A new, fully independent :class:`Reapi` instance.

        Raises:
            ReapiError: If closed or the clone cannot be created.
        """
        self._prepare()
        ctx = _lib.reapi_cli_clone(self._ctx)
        if ctx == ffi.NULL:
            self._check_error(-1)
        new = Reapi.__new__(Reapi)
        new._ctx = ctx
        new._match_format = self._match_format
        return new

    # Internal helpers

    @staticmethod
    def _encode(obj):
        """Return obj as UTF-8 bytes, JSON-encoding a dict or list."""
        if isinstance(obj, (dict, list)):
            obj = json.dumps(obj)
        return obj.encode("utf-8")

    def _prepare(self):
        """Check that this instance is usable and clear any stale error."""
        if self._ctx == ffi.NULL:
            raise ReapiError("Reapi instance is closed")
        _lib.reapi_cli_clear_err_msg(self._ctx)

    def _get_err_message(self):
        msg_ptr = _lib.reapi_cli_get_err_msg(self._ctx)
        if msg_ptr == ffi.NULL:
            return ""
        msg = ffi.string(msg_ptr).decode("utf-8")
        _lib.free(msg_ptr)
        return msg

    def _check_error(self, rc):
        """Raise an appropriate exception if rc < 0, embedding the C error message."""
        if rc >= 0:
            return
        # Read errno before _get_err_message(): that call re-enters the C
        # layer and would clobber ffi.errno.  Order is load-bearing.
        err_no = ffi.errno
        err_msg = self._get_err_message().rstrip()
        if err_no == _errno_mod.EBUSY:
            raise ReapiInsufficientResources(
                f"Resources unavailable: {err_msg}", err_no
            )
        if err_no == _errno_mod.ENODEV:
            raise ReapiInfeasibleRequest(f"Infeasible request: {err_msg}", err_no)
        raise ReapiError(
            f"REAPI error ({_errno_mod.errorcode.get(err_no, err_no)}): {err_msg}",
            err_no,
        )

    def _take_string(self, cptr):
        """Return the Python str for a malloc'd C string and free it."""
        if cptr == ffi.NULL:
            return None
        result = ffi.string(cptr).decode("utf-8")
        _lib.free(cptr)
        return result

    def _take_R(self, cptr, match_format=None):
        """Return a resource set from a malloc'd C string and free it.

        JSON formats are decoded; text formats are returned as str.  An
        empty string (no resources) is returned as None.
        """
        text = self._take_string(cptr)
        if not text:
            return None
        if (match_format or self._match_format) in _TEXT_FORMATS:
            return text
        return json.loads(text)

    def _match(self, match_op, jobspec, jobid):
        """Call reapi_cli_match_with_jobid() and return a MatchResult."""
        self._prepare()
        reserved_out = ffi.new("bool *")
        R_out = ffi.new("char **")
        at_out = ffi.new("int64_t *")
        ov_out = ffi.new("double *")
        rc = _lib.reapi_cli_match_with_jobid(
            self._ctx,
            match_op,
            self._encode(jobspec),
            jobid,
            reserved_out,
            R_out,
            at_out,
            ov_out,
        )
        self._check_error(rc)
        return MatchResult(
            R=self._take_R(R_out[0]),
            reserved=reserved_out[0],
            at=at_out[0],
            overhead=ov_out[0],
        )

    # Match operations

    def match_allocate(
        self, jobid, jobspec, orelse_reserve=False, check_satisfiability=False
    ):
        """Match a jobspec against the resource graph and allocate resources.

        Attempts to find resources that satisfy the given jobspec and allocates them
        for exclusive use.

        Args:
            jobid (int): Job ID assigned by the caller.  Must not already have
                an allocation or reservation in this instance.  Use the same
                value with :meth:`cancel` and :meth:`info`.
            jobspec (dict or str): Flux jobspec (version 1). If dict, will be
                JSON-encoded automatically. If str, assumed to be already JSON-encoded.
                Must include a ``resources`` array describing required resources and a
                ``tasks`` array describing task placement.
            orelse_reserve (bool, optional): If True, make a reservation at a future
                time if resources are not currently available. If False (default),
                only attempt immediate allocation.
            check_satisfiability (bool, optional): If True and resources are
                not currently available, determine whether the request could
                ever be satisfied and raise :class:`ReapiInfeasibleRequest`
                if not.  Mutually exclusive with ``orelse_reserve``.

        Returns:
            MatchResult: ``R`` is the allocated or reserved resource set
            (a subset can be passed to :meth:`cancel` for partial release),
            ``reserved`` is True for a reservation, and ``at`` is the start
            time (0 for an immediate allocation).

        Raises:
            ReapiInsufficientResources: Resources are unavailable.  Without
                ``check_satisfiability`` this includes structurally
                infeasible requests.
            ReapiInfeasibleRequest: Resources are unavailable and the
                request can never be satisfied (``check_satisfiability``
                only).
            ReapiError: ``jobid`` already exists (errno ``EEXIST``), the
                jobspec is invalid, or a general error occurs.
            ValueError: Both ``orelse_reserve`` and ``check_satisfiability``
                are set.

        Examples:
            ::

                try:
                    result = ctx.match_allocate(jobid, jobspec)
                    # ... use result.R ...
                    ctx.cancel(jobid)
                except ReapiInsufficientResources:
                    print("Resources temporarily unavailable, try again later")
        """
        if orelse_reserve and check_satisfiability:
            raise ValueError("orelse_reserve and check_satisfiability are exclusive")
        if orelse_reserve:
            match_op = _lib.MATCH_ALLOCATE_ORELSE_RESERVE
        elif check_satisfiability:
            match_op = _lib.MATCH_ALLOCATE_W_SATISFIABILITY
        else:
            match_op = _lib.MATCH_ALLOCATE
        return self._match(match_op, jobspec, jobid)

    def match_without_allocating(self, jobspec, future=False):
        """Match a jobspec without allocating the matched resources.

        Finds the resources that :meth:`match_allocate` would select, but
        leaves the graph unchanged and does not create a job.

        Args:
            jobspec (dict or str): Flux jobspec to match.
            future (bool, optional): If False (default), match only against
                resources available now, like :meth:`match_allocate`.  If
                True, match at the earliest time the request could be
                satisfied, like a reservation.

        Returns:
            MatchResult: ``R`` is the matched resource set and ``at`` is the
            time at which it is available (0 for now).  ``reserved`` is
            always False.

        Raises:
            ReapiInsufficientResources: Resources are unavailable (now, or
                ever if ``future`` is set).
            ReapiError: If the jobspec is invalid or a general error occurs.
        """
        if future:
            match_op = _lib.MATCH_WITHOUT_ALLOCATING_FUTURE
        else:
            match_op = _lib.MATCH_WITHOUT_ALLOCATING
        # No job is recorded, so the jobid is irrelevant; use the same
        # placeholder as reapi_cli_match().
        return self._match(match_op, jobspec, 2**64 - 1)

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
            MatchResult: Only ``overhead`` is meaningful; ``R`` is None.

        Raises:
            ReapiInfeasibleRequest: Request is unsatisfiable.
            ReapiError: If the jobspec is invalid or a general error occurs.

        Examples:
            ::

                try:
                    ctx.match_satisfiability(jobspec)
                    print("Request is satisfiable")
                except ReapiInfeasibleRequest:
                    print("Request is unsatisfiable")
        """
        return self._match(_lib.MATCH_SATISFIABILITY, jobspec, 0)._replace(R=None)

    def update_allocate(self, jobid, R):
        """Re-register an existing allocation from a resource set.

        Used to replay allocations when a scheduler is reloaded while jobs
        are running.  The resource graph is updated to reflect the existing
        allocation without performing a new match.

        The format of ``R`` is determined as described under *Partial
        cancel* in the class docstring.  For RFC 20 envelopes, the start
        time and duration are taken from ``execution.starttime`` and
        ``execution.expiration``.  For bare JGF (``match_format="jgf"``),
        start time defaults to 0 and duration is unlimited.

        Args:
            jobid (int): Job ID to register.  Must not already be registered.
            R (dict, list or str): Resource set previously allocated to this
                job.  A dict or list is JSON-encoded automatically.

        Returns:
            MatchResult: ``R`` is the registered resource set in this
            instance's ``match_format``, and ``at`` is the start time parsed
            from the input.

        Raises:
            ReapiError: If the jobid already exists (errno ``EEXIST``), the
                resource set is invalid, or the operation fails.

        Examples:
            ::

                # Replay a running job whose R was saved from match_allocate
                result = ctx.update_allocate(jobid, saved_R)
                print(f"Re-registered job {jobid} starting at {result.at}")
        """
        self._prepare()
        at_out = ffi.new("int64_t *")
        ov_out = ffi.new("double *")
        R_out = ffi.new("const char **")
        rc = _lib.reapi_cli_update_allocate(
            self._ctx, jobid, self._encode(R), at_out, ov_out, R_out
        )
        self._check_error(rc)
        return MatchResult(
            R=self._take_R(ffi.cast("char *", R_out[0])),
            at=at_out[0],
            overhead=ov_out[0],
        )

    def cancel(self, jobid, R=None, noent_ok=False):
        """Cancel (free) a job allocation.

        Releases resources from an allocation identified by jobid.  Pass R to
        do a partial release; omit it to release all resources for the job.
        See *Partial cancel* in the class docstring for the formats ``R``
        may take.

        Args:
            jobid (int): Job ID previously passed to :meth:`match_allocate`.
            R (dict, list or str, optional): Resource subset to free, or
                None to free all resources for the job.
            noent_ok (bool, optional): If True, silently succeed if the job
                doesn't exist (default False).

        Returns:
            bool: True if all resources were freed (job is gone), False if
                some resources remain allocated.

        Raises:
            ReapiError: If the job doesn't exist (and noent_ok is False),
                ``R`` cannot be parsed, or the operation fails.

        Examples:
            ::

                jobid = 1
                result = ctx.match_allocate(jobid, two_node_jobspec)

                # Release a subset (R_subset is a one-node portion as dict)
                full_removal = ctx.cancel(jobid, R_subset)

                if not full_removal:
                    ctx.cancel(jobid)  # release remainder
        """
        self._prepare()
        if R is None:
            rc = _lib.reapi_cli_cancel(self._ctx, jobid, noent_ok)
            self._check_error(rc)
            return True
        full_removal_out = ffi.new("bool *")
        rc = _lib.reapi_cli_partial_cancel(
            self._ctx, jobid, self._encode(R), noent_ok, full_removal_out
        )
        self._check_error(rc)
        return full_removal_out[0]

    def info(self, jobid):
        """Get information about an allocated or reserved job.

        Retrieves the current state and timing information for a job that has
        been allocated or reserved in this instance.

        Args:
            jobid (int): Job ID to query. Must be a jobid previously passed
                to :meth:`match_allocate` or :meth:`update_allocate`.

        Returns:
            JobInfo: The job's state and timing.

        Raises:
            ReapiError: If the job doesn't exist or the operation fails.

        Examples:
            ::

                jobid = 1
                result = ctx.match_allocate(jobid, jobspec)

                # Query job info
                info = ctx.info(jobid)
                print(f"Job {jobid} mode: {info.mode}")
        """
        self._prepare()
        mode_out = ffi.new("char **")
        reserved_out = ffi.new("bool *")
        at_out = ffi.new("int64_t *")
        ov_out = ffi.new("double *")
        rc = _lib.reapi_cli_info(
            self._ctx, jobid, mode_out, reserved_out, at_out, ov_out
        )
        self._check_error(rc)
        return JobInfo(
            mode=self._take_string(mode_out[0]) or "",
            reserved=reserved_out[0],
            at=at_out[0],
            overhead=ov_out[0],
        )

    def find(self, criteria, format=None):
        """Find resources matching the specified criteria.

        Search the resource graph for resources matching the given criteria
        and return them as a resource set.

        Args:
            criteria (str): Search criteria string. Examples:
                - ``"sched-now=allocated"`` - Find all allocated resources
                - ``"status=down"`` - Find all down resources
                - ``"status=up"`` - Find all up resources
            format (str, optional): Output format: any JSON ``match_format``
                value (see the class docstring).  If None, uses this
                instance's ``match_format``.

        Returns:
            dict, list or None: Matching resources, or None if no resources
            matched the criteria.

        Raises:
            ReapiError: If ``criteria`` or ``format`` is invalid or the
                operation fails.
            ValueError: If the effective format is not a JSON format.

        Examples:
            ::

                # Find all allocated resources
                R_allocated = ctx.find("sched-now=allocated")
                if R_allocated:
                    print(f"Allocated: {R_allocated}")

                # Find all down resources in rv1_nosched format
                R_down = ctx.find("status=down", format="rv1_nosched")
        """
        self._prepare()
        if (format or self._match_format) in _TEXT_FORMATS:
            raise ValueError("find requires a JSON match format")
        format_arg = self._encode(format) if format else ffi.NULL
        R_out = ffi.new("char **")
        rc = _lib.reapi_cli_find(self._ctx, self._encode(criteria), format_arg, R_out)
        self._check_error(rc)
        return self._take_R(R_out[0], format)

    # Graph modification

    def add_subgraph(self, subgraph):
        """Add resources to the resource graph.

        Grows the graph by attaching a JGF subgraph.  The subgraph must
        include the path from the cluster root to the new resources, using
        the same vertex ids and paths as the existing graph for vertices it
        has in common with it.

        Args:
            subgraph (dict or str): JGF subgraph to attach.  A dict is
                JSON-encoded automatically.

        Raises:
            ReapiError: If the subgraph is invalid or cannot be attached.
        """
        self._prepare()
        rc = _lib.reapi_cli_add_subgraph(self._ctx, self._encode(subgraph))
        self._check_error(rc)

    def remove_subgraph(self, path):
        """Remove resources from the resource graph.

        Shrinks the graph by detaching the subtree rooted at ``path``.

        Args:
            path (str): Path from the cluster root to the root of the
                subtree to remove, e.g. ``"/cluster0/rack0/node3"``.

        Raises:
            ReapiError: If ``path`` is not found or cannot be removed.
        """
        self._prepare()
        rc = _lib.reapi_cli_remove_subgraph(self._ctx, self._encode(path))
        self._check_error(rc)

    # Resource status

    def set_status(self, path, status):
        """Set resource status by path.

        Marks a resource at the specified path with the given status.

        Args:
            path (str): Resource path (e.g., ``"/cluster0/node0"``).
            status (int): Resource status. Use ``RESOURCE_UP`` or ``RESOURCE_DOWN``
                from the module (e.g., ``from fluxion.reapi import RESOURCE_UP``).

        Raises:
            ReapiError: If the resource is not found or parameters are
                invalid.

        Examples:
            ::

                from fluxion.reapi import Reapi, RESOURCE_UP, RESOURCE_DOWN

                # Mark specific resource path down
                ctx.set_status("/cluster0/node0", RESOURCE_DOWN)

                # Bring it back up
                ctx.set_status("/cluster0/node0", RESOURCE_UP)
        """
        self._prepare()
        rc = _lib.reapi_cli_set_status(self._ctx, self._encode(path), status)
        self._check_error(rc)

    def get_status(self, path):
        """Get resource status by path.

        Query the status of a resource at the specified path.

        Args:
            path (str): Resource path (e.g., ``"/cluster0/node0"``).

        Returns:
            int: Resource status (``RESOURCE_UP`` or ``RESOURCE_DOWN``).

        Raises:
            ReapiError: If the resource is not found or parameters are
                invalid.

        Examples:
            ::

                from fluxion.reapi import Reapi, RESOURCE_UP

                # Query by path
                status = ctx.get_status("/cluster0/node0")
                if status == RESOURCE_UP:
                    print("Resource is up")
        """
        self._prepare()
        status_out = ffi.new("resource_status_t *")
        rc = _lib.reapi_cli_get_status(self._ctx, self._encode(path), status_out)
        self._check_error(rc)
        return status_out[0]

    def set_rank_status(self, ranks, status):
        """Set resource status by rank.

        Marks all resources at specified ranks with the given status.
        Ranks not present in the graph are silently ignored.

        Args:
            ranks (str or int): RFC 22 idset string (e.g., "0", "0-3", "0,2,4"),
                the special value "all", or an integer rank. Use "all" to mark all ranks.
            status (int): Resource status. Use ``RESOURCE_UP`` or ``RESOURCE_DOWN``
                from the module.

        Raises:
            ReapiError: If parameters are invalid.

        Examples:
            ::

                from fluxion.reapi import Reapi, RESOURCE_UP, RESOURCE_DOWN

                # Mark rank 0 down
                ctx.set_rank_status(0, RESOURCE_DOWN)

                # Mark ranks 0-3 down
                ctx.set_rank_status("0-3", RESOURCE_DOWN)

                # Mark all ranks down
                ctx.set_rank_status("all", RESOURCE_DOWN)

                # Bring rank 0 back up
                ctx.set_rank_status(0, RESOURCE_UP)
        """
        self._prepare()
        rc = _lib.reapi_cli_set_rank_status(self._ctx, self._encode(str(ranks)), status)
        self._check_error(rc)

    def get_rank_status(self, rank):
        """Get resource status by rank.

        Query the status of resources at a specified rank.

        Args:
            rank (str or int): A single rank as string or integer.  An
                idset or "all" is accepted only if it resolves to exactly
                one rank in the graph.

        Returns:
            int: Resource status (``RESOURCE_UP`` or ``RESOURCE_DOWN``).

        Raises:
            ReapiError: If the rank is not found or ``rank`` does not
                resolve to exactly one rank.

        Examples:
            ::

                from fluxion.reapi import Reapi, RESOURCE_UP

                # Query by rank (int or string)
                status = ctx.get_rank_status(0)
                if status == RESOURCE_UP:
                    print("Rank is up")
        """
        self._prepare()
        status_out = ffi.new("resource_status_t *")
        rc = _lib.reapi_cli_get_rank_status(
            self._ctx, self._encode(str(rank)), status_out
        )
        self._check_error(rc)
        return status_out[0]

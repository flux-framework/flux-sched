###############################################################
# Copyright 2026 Lawrence Livermore National Security, LLC
# (c.f. AUTHORS, NOTICE.LLNS, COPYING)
#
# This file is part of the Flux resource manager framework.
# For details, see https://github.com/flux-framework.
#
# SPDX-License-Identifier: LGPL-3.0
###############################################################

"""Fluxion resource pool implementation.

Provides graph-based resource matching using Fluxion's REAPI with JGF
(JSON Graph Format) resource graphs.
"""

import json
import syslog
from collections.abc import Mapping
from typing import Optional

from flux.resource.ResourcePoolImplementation import (
    InfeasibleRequest,
    InsufficientResources,
    ResourcePoolImplementation,
)
from flux.resource.Rv1Set import Rv1Set

from fluxion.reapi.cli import (
    Reapi,
    ReapiError,
    ReapiInfeasibleRequest,
    ReapiInsufficientResources,
    RESOURCE_UP,
    RESOURCE_DOWN,
)


class FluxionRequest:
    """Parsed resource request: the jobspec plus its extracted duration."""

    def __init__(self, jobspec, duration):
        self.jobspec = jobspec
        self.duration = duration


class FluxionJGFPool(Rv1Set, ResourcePoolImplementation):
    """Fluxion graph-based resource pool implementation.

    This pool owns its own resource graph (via reapi_cli) and operates
    standalone without requiring sched-fluxion broker modules.

    The resource graph must be provided as JGF in the R.scheduling key.
    Loaded by schedulers via R.scheduling.writer='fluxion:jgf'.
    """

    version = 1  # Uses Rv1 R format with scheduling.writer extension

    # Empty resource set for returning when no resources match
    EMPTY_R = {
        "version": 1,
        "execution": {
            "R_lite": [],
            "starttime": 0,
            "expiration": 0,
            "nodelist": [],
        },
    }

    def __init__(self, R=None, log=None, **kwargs):
        """Initialize Fluxion resource pool.

        Args:
            R: Resource set as JSON string or dict. Must contain JGF graph
               in R["scheduling"]["graph"].
            log: Logger callable with (level, msg) signature
            **kwargs: Additional options:
                - match_format: Output format for R ("rv1", "jgf", etc.)
                - matcher_policy: Matching policy for Fluxion
                - Other options passed to Reapi.initialize()

        Raises:
            TypeError: If R has wrong type
            OSError: If R.scheduling.graph is missing or Reapi initialization fails
        """
        # Set up logging (no-op by default)
        if log is not None:
            self.log = log

        # Parse R input to validate JGF presence before calling parent
        if isinstance(R, str):
            R_dict = json.loads(R)
        elif isinstance(R, Mapping):
            R_dict = dict(R)
        elif R is None:
            raise TypeError("R cannot be None for Fluxion FluxionJGFPool")
        else:
            raise TypeError(f"R must be a dict or JSON string, got {type(R)}")

        # Require JGF in scheduling key
        scheduling = R_dict.get("scheduling", {})
        if not scheduling or "graph" not in scheduling:
            raise OSError(
                "FluxionJGFPool requires JGF in R.scheduling.graph"
            )

        jgf_dict = scheduling

        # Initialize Rv1Set parent (parses R_lite, sets up _ranks, nodelist, etc.)
        # keep_scheduling=True preserves R.scheduling for propagation to allocations
        super().__init__(R, keep_scheduling=True)

        # Load the JGF graph into a REAPI context and finish common setup.
        # Default to rv1 match format to get Rv1 format with JGF in scheduling key.
        self._init_reapi(
            jgf_dict,
            load_format="jgf",
            match_format=kwargs.pop("match_format", "rv1"),
            **kwargs,
        )

    def _init_reapi(self, graph, load_format, match_format, **kwargs):
        """Create the REAPI context, load the graph, and set common state.

        Shared by FluxionJGFPool and FluxionRv1ExecPool.  The caller is
        responsible for parsing R and initializing the Rv1Set parent first;
        this handles the REAPI context and the per-pool bookkeeping that is
        identical across pool types.

        Raises:
            OSError: If REAPI initialization fails.
        """
        self._ctx = Reapi()
        try:
            self._ctx.initialize(
                graph,
                load_format=load_format,
                match_format=match_format,
                **kwargs,
            )
            self.log(syslog.LOG_DEBUG, "Initialized Fluxion resource pool")
        except ReapiError as e:
            raise OSError(f"Failed to initialize Fluxion graph: {e}")

        # Track job end times: jobid → expiration (float, seconds since epoch)
        self._end_times = {}

        # Expose self as impl for compatibility with code expecting
        # the wrapper pattern (pool.impl.attribute)
        self.impl = self

        # Pool options - empty set (no custom options yet)
        self.known_options = frozenset()

    def _bump(self):
        """Increment generation counter to signal pool state change."""
        self.generation += 1

    @staticmethod
    def _r_to_dict(R):
        """Coerce a resource set (ResourceSet wrapper, Rv1Set, or Mapping) to a
        plain dict.

        Raises:
            TypeError: If R is not a recognized resource-set type.  Returning
                an empty dict here instead would silently turn a mistyped R
                into a no-op cancel/update, so fail loudly.
        """
        if hasattr(R, "impl") and hasattr(R.impl, "to_dict"):
            return R.impl.to_dict()
        if isinstance(R, Rv1Set):
            return R.to_dict()
        if isinstance(R, Mapping):
            return dict(R)
        raise TypeError(f"Cannot convert R of type {type(R)} to dict")

    # ------------------------------------------------------------------
    # Availability management
    # ------------------------------------------------------------------

    def mark_up(self, ids: str) -> None:
        """Mark resources as up.

        Args:
            ids: RFC 22 idset string of ranks or "all"
        """
        # Update REAPI graph status (handles "all" and idset strings)
        try:
            self._ctx.set_rank_status(ids, RESOURCE_UP)
        except ReapiError as e:
            self.log(syslog.LOG_WARNING, f"Failed to mark ranks up ({ids}): {e}")

        self._bump()
        self.log(syslog.LOG_DEBUG, f"Marked up: {ids}")

    def mark_down(self, ids: str) -> None:
        """Mark resources as down.

        Args:
            ids: RFC 22 idset string of ranks or "all"
        """
        # Update REAPI graph status (handles "all" and idset strings)
        try:
            self._ctx.set_rank_status(ids, RESOURCE_DOWN)
        except ReapiError as e:
            self.log(syslog.LOG_WARNING, f"Failed to mark ranks down ({ids}): {e}")

        self._bump()
        self.log(syslog.LOG_DEBUG, f"Marked down: {ids}")

    def copy_down(self):
        """Return ResourceSet containing only down resources."""
        # Use find() to query the graph for all down resources
        try:
            R_down = self._ctx.find("status=down", format="rv1_nosched")
            if R_down is None:
                # No down resources - return empty Rv1Set
                return Rv1Set(self.EMPTY_R)
            return Rv1Set(R_down)
        except ReapiError as e:
            self.log(syslog.LOG_WARNING, f"Failed to query down resources: {e}")
            # Return empty on error
            return Rv1Set(self.EMPTY_R)

    # ------------------------------------------------------------------
    # Job lifecycle
    # ------------------------------------------------------------------

    def register_alloc(self, jobid: int, R) -> None:
        """Register an existing allocation (for reconnect).

        Args:
            jobid: Job ID
            R: Allocated resource set (ResourceSet or dict)
        """
        R_dict = self._r_to_dict(R)

        # Update fluxion's internal state with this allocation
        try:
            self._ctx.update_allocate(jobid, R_dict)
        except ReapiError as e:
            self.log(syslog.LOG_WARNING, f"Failed to register allocation for job {jobid}: {e}")

        # Track expiration time only (R is managed by reapi)
        expiration = R_dict.get("execution", {}).get("expiration", 0)
        self._end_times[jobid] = expiration
        self._bump()
        self.log(syslog.LOG_DEBUG, f"Registered allocation for job {jobid}")

    def free(self, jobid: int, R=None, final: bool = False) -> None:
        """Free job's allocated resources.

        Args:
            jobid: Job ID (scheduler jobid)
            R: Allocated resource set (optional, for partial free)
            final: If True, this is the final free for this job
        """
        # Cancel in Fluxion graph
        try:
            # If final=True, always do full cancel regardless of R
            if final or R is None:
                # Full cancel
                self._ctx.cancel(jobid, noent_ok=True)
            else:
                # Partial cancel - format is auto-detected from R
                R_dict = self._r_to_dict(R)
                self._ctx.cancel(jobid, R=R_dict, noent_ok=True)
        except ReapiError as e:
            self.log(
                syslog.LOG_WARNING,
                f"Cancel failed for job {jobid}: {e}",
            )

        # Remove from tracking (full cancel only)
        if final or R is None:
            if jobid in self._end_times:
                del self._end_times[jobid]

        self._bump()
        self.log(
            syslog.LOG_DEBUG,
            f"Freed resources for job {jobid}",
        )

    def update_expiration(self, jobid: int, expiration: float) -> None:
        """Update job end time.

        Args:
            jobid: Job ID
            expiration: New expiration time (seconds since epoch)
        """
        if jobid in self._end_times:
            self._end_times[jobid] = expiration
            self._bump()

    def job_end_times(self):
        """Return list of (jobid, end_time) pairs for tracked jobs."""
        result = []
        for jobid, expiration in self._end_times.items():
            if expiration > 0:
                result.append((jobid, expiration))
        return result

    # ------------------------------------------------------------------
    # Scheduling operations
    # ------------------------------------------------------------------

    def parse_resource_request(self, jobspec: dict):
        """Parse jobspec and return request object.

        For Fluxion, we pass the full jobspec through, so just validate
        and return it wrapped in a simple container.

        Args:
            jobspec: Jobspec dict

        Returns:
            Request object with jobspec and duration attribute
        """
        if not isinstance(jobspec, Mapping):
            raise TypeError("Jobspec must be a dict")

        # Extract duration for scheduler compatibility
        duration = (
            jobspec.get("attributes", {}).get("system", {}).get("duration", 0.0)
        )

        return FluxionRequest(jobspec, duration)

    def alloc(self, jobid: int, request):
        """Allocate resources for a job.

        Args:
            jobid: Job ID (scheduler jobid)
            request: Resource request from parse_resource_request()

        Returns:
            Rv1Set instance representing the allocated resources

        Raises:
            InsufficientResources: Not enough resources available now
            InfeasibleRequest: Request can never be satisfied
        """
        # Extract jobspec from request object
        jobspec = request.jobspec if hasattr(request, "jobspec") else request

        # Call Fluxion matcher
        try:
            result = self._ctx.match_allocate(jobid, jobspec)
        except ReapiInsufficientResources:
            self.log(syslog.LOG_DEBUG, f"Match failed for job {jobid}")
            # Check if request is feasible to distinguish temporary vs permanent failure
            try:
                self.check_feasibility(request)
                # Feasible but currently unavailable
                raise InsufficientResources("insufficient resources")
            except InfeasibleRequest:
                # Not feasible - re-raise as InfeasibleRequest
                raise
        except ReapiInfeasibleRequest as e:
            raise InfeasibleRequest(str(e))
        except ReapiError as e:
            raise OSError(f"Fluxion match failed: {e}")

        # Parse allocated R from REAPI
        R_allocated = result["R"]
        if not R_allocated:
            self.log(
                syslog.LOG_ERR,
                f"REAPI returned empty R for job {jobid}",
            )
            raise OSError(f"REAPI returned empty R")

        # Track expiration time only (R is managed by reapi)
        expiration = jobspec.get("attributes", {}).get("system", {}).get("duration", 0)
        self._end_times[jobid] = expiration

        self._bump()
        self.log(
            syslog.LOG_DEBUG,
            f"Allocated resources for job {jobid} (overhead: {result['overhead']:.3f}s)",
        )

        # Return Rv1Set with allocated R, preserving JGF in scheduling key
        return Rv1Set(R_allocated, keep_scheduling=True)

    def check_feasibility(self, request) -> None:
        """Check if request is feasible.

        Args:
            request: Resource request from parse_resource_request()

        Raises:
            InfeasibleRequest: If request cannot be satisfied
        """
        jobspec = request.jobspec if hasattr(request, "jobspec") else request

        try:
            self._ctx.match_satisfiability(jobspec)
        except ReapiInfeasibleRequest as e:
            raise InfeasibleRequest(str(e))
        except ReapiError as e:
            raise OSError(f"Feasibility check failed: {e}")

    # ------------------------------------------------------------------
    # Structural copies
    # ------------------------------------------------------------------

    def copy(self):
        """Create an independent deep copy for forward simulation.

        Returns a new pool with cloned Reapi context that shares no state
        with the original. Operations on the copy do not affect the original.
        """
        try:
            # Create new instance of same class
            new_pool = self.__class__.__new__(self.__class__)

            # Clone the Reapi context (deep copy including graph state)
            new_pool._ctx = self._ctx.clone()

            # Copy parent Rv1Set state
            from flux.resource.Rv1Set import Rv1Set
            Rv1Set.__init__(new_pool, self.to_dict(), keep_scheduling=False)

            # Copy end times tracking (shallow copy is sufficient for int→float dict)
            new_pool._end_times = dict(self._end_times)

            # Copy other attributes
            new_pool.impl = new_pool
            new_pool.known_options = self.known_options
            new_pool.log = self.log

            return new_pool
        except ReapiError as e:
            raise OSError(f"Failed to copy FluxionPool: {e}")

    def copy_allocated(self):
        """Return ResourceSet of allocated resources."""
        # Use find() to query the graph for all allocated resources in one go
        try:
            R_allocated = self._ctx.find("sched-now=allocated", format="rv1_nosched")
            if R_allocated is None:
                # No allocated resources - return empty Rv1Set
                return Rv1Set(self.EMPTY_R)
            return Rv1Set(R_allocated)
        except ReapiError as e:
            self.log(syslog.LOG_WARNING, f"Failed to query allocated resources: {e}")
            # Return empty on error
            return Rv1Set(self.EMPTY_R)

    def to_set(self):
        """Return ResourceSet snapshot of pool topology."""
        # Return a copy as an Rv1Set (stripping pool-specific state)
        return Rv1Set(self.to_dict())

    # ------------------------------------------------------------------
    # Serialization
    # ------------------------------------------------------------------

    def dumps(self) -> str:
        """Return human-readable summary."""
        return f"FluxionJGFPool(allocated_jobs={len(self._end_times)})"

    # Note: to_dict(), set_starttime(), set_expiration(), get_expiration()
    # are inherited from Rv1Set parent class

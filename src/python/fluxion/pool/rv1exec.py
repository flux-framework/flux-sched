###############################################################
# Copyright 2026 Lawrence Livermore National Security, LLC
# (c.f. AUTHORS, NOTICE.LLNS, COPYING)
#
# This file is part of the Flux resource manager framework.
# For details, see https://github.com/flux-framework.
#
# SPDX-License-Identifier: LGPL-3.0
###############################################################

"""Rv1Exec-based Fluxion resource pool implementation.

Provides Fluxion graph-based matching using R_lite directly
(RFC 20 execution section) without requiring pre-generated JGF.
"""

import json
import syslog
from collections.abc import Mapping

from fluxion.pool.jgf import FluxionJGFPool


class FluxionRv1ExecPool(FluxionJGFPool):
    """Fluxion pool that loads from R_lite (rv1exec format).

    This pool accepts standard R with R_lite in the execution section
    and does not require JGF in the scheduling key. The Reapi loads
    the resource graph directly from R_lite using load_format="rv1exec".

    Loaded by schedulers via R.scheduling.writer='fluxion:rv1exec' or
    explicitly via pool-class=fluxion:rv1exec.
    """

    def __init__(self, R=None, log=None, **kwargs):
        """Initialize Fluxion pool from R_lite.

        Args:
            R: Resource set as JSON string or dict. Must contain R_lite
               in R["execution"]["R_lite"].
            log: Logger callable with (level, msg) signature
            **kwargs: Additional options passed to Reapi.initialize()

        Raises:
            TypeError: If R has wrong type
            OSError: If Reapi initialization fails
        """
        # Set up logging (no-op by default)
        if log is not None:
            self.log = log

        # Parse R input
        if isinstance(R, str):
            R_dict = json.loads(R)
        elif isinstance(R, Mapping):
            R_dict = dict(R)
        elif R is None:
            raise TypeError("R cannot be None for Fluxion Rv1ExecPool")
        else:
            raise TypeError(f"R must be a dict or JSON string, got {type(R)}")

        # Initialize Rv1Set parent (parses R_lite, sets up _ranks, nodelist, etc.)
        # We call Rv1Set.__init__ directly, skipping FluxionJGFPool's JGF validation
        # keep_scheduling=False since rv1exec doesn't use/emit scheduling keys
        from flux.resource.Rv1Set import Rv1Set

        Rv1Set.__init__(self, R, keep_scheduling=False)

        # Load R_lite directly into a REAPI context via the rv1exec reader and
        # finish common setup.  Default to rv1_nosched match format.
        self._init_reapi(
            R_dict,
            load_format="rv1exec",
            match_format=kwargs.pop("match_format", "rv1_nosched"),
            **kwargs,
        )

    def dumps(self) -> str:
        """Return human-readable summary."""
        return f"FluxionRv1ExecPool(allocated_jobs={len(self._end_times)})"

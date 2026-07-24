###############################################################
# Copyright 2026 Lawrence Livermore National Security, LLC
# (c.f. AUTHORS, NOTICE.LLNS, COPYING)
#
# This file is part of the Flux resource manager framework.
# For details, see https://github.com/flux-framework.
#
# SPDX-License-Identifier: LGPL-3.0
###############################################################

"""Fluxion resource pool implementations for flux-core scheduler.

Provides graph-based resource matching using Fluxion's REAPI.
"""

from fluxion.pool.jgf import FluxionJGFPool
from fluxion.pool.rv1exec import FluxionRv1ExecPool

# Expose class for URI-based loading per RFC 40
# When R.scheduling.writer = "fluxion:jgf", scheduler loads fluxion.jgf
jgf = FluxionJGFPool

# When R.scheduling.writer = "fluxion:rv1exec", scheduler loads fluxion.rv1exec
rv1exec = FluxionRv1ExecPool

# Default pool class (RFC 40: "fluxion" means "fluxion:jgf")
pool_class = FluxionJGFPool

__all__ = ["FluxionJGFPool", "FluxionRv1ExecPool", "jgf", "rv1exec", "pool_class"]

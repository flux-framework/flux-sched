###############################################################
# Copyright 2026 Lawrence Livermore National Security, LLC
# (c.f. AUTHORS, NOTICE.LLNS, COPYING)
#
# This file is part of the Flux resource manager framework.
# For details, see https://github.com/flux-framework.
#
# SPDX-License-Identifier: LGPL-3.0
###############################################################

"""Fluxion: Advanced graph-based scheduler for Flux.

Exposes resource pool classes per RFC 20/40 for use with flux-core
Python scheduler framework.
"""

__all__ = ["jgf", "rv1exec", "pool_class"]


# Lazy import of pool classes to avoid requiring libreapi_cli.so
# for users who only need fluxion.resourcegraph utilities
def __getattr__(name):
    if name in ("jgf", "rv1exec", "pool_class"):
        from fluxion.pool import jgf, rv1exec, pool_class

        globals().update({"jgf": jgf, "rv1exec": rv1exec, "pool_class": pool_class})
        return globals()[name]
    raise AttributeError(f"module 'fluxion' has no attribute '{name}'")

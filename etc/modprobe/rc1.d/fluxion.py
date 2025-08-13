#############################################################
# Copyright 2025 Lawrence Livermore National Security, LLC
# (c.f. AUTHORS, NOTICE.LLNS, COPYING)
#
# This file is part of the Flux resource manager framework.
# For details, see https://github.com/flux-framework.
#
# SPDX-License-Identifier: LGPL-3.0
##############################################################

import os


def setup(context):
    if "FLUXION_RESOURCE_OPTIONS" in os.environ:
        context.setopt(
            "sched-fluxion-resource",
            os.getenv("FLUXION_RESOURCE_OPTIONS"),
            overwrite=True,
        )
        context.setopt(
            "sched-fluxion-feasibility",
            os.getenv("FLUXION_RESOURCE_OPTIONS"),
            overwrite=True,
        )
    if "FLUXION_QMANAGER_OPTIONS" in os.environ:
        context.setopt(
            "sched-fluxion-qmanager",
            os.getenv("FLUXION_QMANAGER_OPTIONS"),
            overwrite=True,
        )

#!/usr/bin/env python
##############################################################
# Copyright 2021 Lawrence Livermore National Security, LLC
# (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
#
# This file is part of the Flux resource manager framework.
# For details, see https://github.com/flux-framework.
#
# SPDX-License-Identifier: LGPL-3.0
##############################################################
import sys
from flux.core.inner import raw
from flux.constants import FLUX_CONF_AUTO

print(raw.flux_conf_builtin_get(sys.argv[1], FLUX_CONF_AUTO).decode("utf-8"))

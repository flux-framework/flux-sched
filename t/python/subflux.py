"""
Utility for re-running python tests under flux.

Copied from flux-core.
"""

###############################################################
# Copyright 2014 Lawrence Livermore National Security, LLC
# (c.f. AUTHORS, NOTICE.LLNS, COPYING)
#
# This file is part of the Flux resource manager framework.
# For details, see https://github.com/flux-framework.
#
# SPDX-License-Identifier: LGPL-3.0
###############################################################

import argparse
import os
import subprocess
import sys
import shutil


#  Ignore -v, --verbose and --root options so that python test scripts
#   can absorb the same options as sharness tests. Later, something could
#   be done with these options, but for now they are dropped silently.
parser = argparse.ArgumentParser()
parser.add_argument("--debug", "-d", action="store_true")
parser.add_argument("--root", metavar="PATH", type=str)
args, remainder = parser.parse_known_args()

sys.argv[1:] = remainder


def rerun_under_flux(size=1):
    try:
        if os.environ["IN_SUBFLUX"] == "1":
            return True
    except KeyError:
        pass

    child_env = dict(**os.environ)
    child_env["IN_SUBFLUX"] = "1"

    # ported from sharness.d/flux-sharness.sh
    command = [shutil.which("flux"), "start", "--test-size", str(size)]

    command.extend([sys.executable, sys.argv[0]])

    p = subprocess.Popen(
        command, env=child_env, bufsize=-1, stdout=sys.stdout, stderr=sys.stderr
    )
    p.wait()
    if p.returncode > 0:
        sys.exit(p.returncode)
    elif p.returncode < 0:
        sys.exit(128 + -p.returncode)
    return False

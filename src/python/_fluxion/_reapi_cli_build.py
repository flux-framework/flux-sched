#!/usr/bin/env python3
###############################################################
# Copyright 2026 Lawrence Livermore National Security, LLC
# (c.f. AUTHORS, NOTICE.LLNS, COPYING)
#
# This file is part of the Flux resource manager framework.
# For details, see https://github.com/flux-framework.
#
# SPDX-License-Identifier: LGPL-3.0
###############################################################

"""CFFI build script for the _fluxion._reapi_cli extension module.

Usage: _reapi_cli_build.py SOURCE_DIR OUTPUT

The C declarations handed to CFFI are derived from the reapi_cli headers
in SOURCE_DIR rather than maintained by hand, so they cannot drift from
the library they bind.
"""

import re
import sys

from cffi import FFI

# Headers whose declarations make up the binding, in dependency order.
HEADERS = [
    "resource/policies/base/match_op.h",
    "resource/reapi/bindings/c/resource_status.h",
    "resource/reapi/bindings/c/reapi_cli.h",
]


def strip_header(text):
    """Reduce a C header to the declarations CFFI's cdef() understands.

    Drop comments, preprocessor directives, and the extern "C" wrapper.
    The headers above are written so that what remains is plain C that
    cdef() accepts.
    """
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    text = re.sub(r"//[^\n]*", "", text)
    lines = []
    for line in text.splitlines():
        stripped = line.strip()
        if stripped.startswith("#") or stripped in ('extern "C" {', "}"):
            continue
        lines.append(line)
    return "\n".join(lines)


def build(source_dir):
    ffi = FFI()
    ffi.set_source(
        "_fluxion._reapi_cli",
        """
        #include <stdlib.h>
        #include <flux/core.h>
        #include <reapi_cli.h>
        """,
        libraries=["reapi_cli", "flux-core"],
    )
    cdefs = []
    for header in HEADERS:
        with open(f"{source_dir}/{header}") as f:
            cdefs.append(strip_header(f.read()))
    cdefs.append("void free(void *ptr);")
    ffi.cdef("\n".join(cdefs))
    return ffi


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(f"Usage: {sys.argv[0]} SOURCE_DIR OUTPUT")
    build(sys.argv[1]).emit_c_code(sys.argv[2])

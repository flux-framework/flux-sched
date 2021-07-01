#!/bin/false

##############################################################
# Copyright 2021 Lawrence Livermore National Security, LLC
# (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
#
# This file is part of the Flux resource manager framework.
# For details, see https://github.com/flux-framework.
#
# SPDX-License-Identifier: LGPL-3.0
##############################################################

#
#  Run script as `flux ion-resource` with properly configured
#   FLUX_EXEC_PATH or `flux python flux-ion-resource` if not to
#   avoid python version mismatch
#

import argparse
import sys
import json
import logging
import flux

from fluxion.resourcegraph.V1 import FluxionResourceGraphV1


#
# 'encode' subcommand routines
#
def encode_init(args, infile, outfile):
    if args.ifn:
        infile = open(args.ifn, "r")
    if args.ofn:
        outfile = open(args.ofn, "w")
    return infile, outfile


def encode_fini(args, infile, outfile):
    if args.ofn:
        outfile.close()
    if args.ifn:
        infile.close()


def encode(args):
    infile = sys.stdin
    outfile = sys.stdout
    try:
        infile, outfile = encode_init(args, infile, outfile)
        rv1 = json.loads(infile.read())
        graph = FluxionResourceGraphV1(rv1)
        rv1["scheduling"] = graph.to_JSON()
        print(json.dumps(rv1), file=outfile)
        encode_fini(args, infile, outfile)

    except:
        encode_fini(args, infile, outfile)
        raise


LOGGER = logging.getLogger("flux-ion-R")


@flux.util.CLIMain(LOGGER)
def main():
    parser = argparse.ArgumentParser(
        prog="flux-ion-R", formatter_class=flux.util.help_formatter()
    )
    subparsers = parser.add_subparsers(
        title="subcommands", description="", dest="subcommand"
    )
    subparsers.required = True

    encode_parser = subparsers.add_parser(
        "encode", formatter_class=flux.util.help_formatter()
    )
    encode_parser.set_defaults(func=encode)
    encode_parser.add_argument(
        "--output",
        dest="ofn",
        required=False,
        help="redirect stdout to FILENAME",
        metavar="FILENAME",
    )
    encode_parser.add_argument(
        "--input",
        dest="ifn",
        required=False,
        help="redirect stdin from FILENAME",
        metavar="FILENAME",
    )

    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()

#
# vi: ts=4 sw=4 expandtab
#

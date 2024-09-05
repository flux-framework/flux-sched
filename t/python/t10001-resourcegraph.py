#!/usr/bin/env python3

###############################################################
# Copyright 2023 Lawrence Livermore National Security, LLC
# (c.f. AUTHORS, NOTICE.LLNS, COPYING)
#
# This file is part of the Flux resource manager framework.
# For details, see https://github.com/flux-framework.
#
# SPDX-License-Identifier: LGPL-3.0

import unittest
import json
import sys
import pathlib

from pycotap import TAPTestRunner

# add fluxion to sys.path
sys.path.insert(0, str(pathlib.Path(__file__).absolute().parents[2] / "src" / "python"))

from fluxion.resourcegraph.V1 import (
    FluxionResourceGraphV1,
)

RV1 = {
    "version": 1,
    "execution": {
        "R_lite": [{"rank": "0", "children": {"core": "0-4"}}],
        "starttime": 0.0,
        "expiration": 0.0,
        "nodelist": ["compute01"],
    },
}

RV1_2 = {
    "version": 1,
    "execution": {
        "R_lite": [{"rank": "0-10", "children": {"gpu": "0-1", "core": "0-7"}}],
        "starttime": 0.0,
        "expiration": 0.0,
        "nodelist": ["compute[0-10]"],
    },
}

RV1_3 = {
    "version": 1,
    "execution": {
        "R_lite": [{"rank": "0-19", "children": {"gpu": "0-1", "core": "0-7"}}],
        "starttime": 0.0,
        "expiration": 0.0,
        "nodelist": ["compute[0-19]"],
        "properties": {"pdebug": "0-9", "pbatch": "10-19"},
    },
}


class TestResourceGraph(unittest.TestCase):
    """Test for the ResourceGraph class."""

    def _check_metadata(self, metadata):
        if metadata["type"] not in ("node", "core", "gpu", "cluster"):
            raise ValueError(metadata["type"])

    def test_basic(self):
        graph = FluxionResourceGraphV1(RV1)
        j = graph.to_JSON()
        json.dumps(j)  # make sure it doesn't throw an error
        self.assertEqual(len(j["graph"]["nodes"]), len(graph.get_nodes()))
        self.assertEqual(len(j["graph"]["edges"]), len(graph.get_edges()))
        for node in graph.get_nodes():
            self._check_metadata(node.get_metadata())

    def test_basic_2(self):
        graph = FluxionResourceGraphV1(RV1_2)
        j = graph.to_JSON()
        json.dumps(j)
        self.assertEqual(len(j["graph"]["nodes"]), len(graph.get_nodes()))
        self.assertEqual(len(j["graph"]["edges"]), len(graph.get_edges()))
        for node in graph.get_nodes():
            self._check_metadata(node.get_metadata())

    def test_basic_3(self):
        graph = FluxionResourceGraphV1(RV1_3)
        j = graph.to_JSON()
        self.assertEqual(len(j["graph"]["nodes"]), len(graph.get_nodes()))
        self.assertEqual(len(j["graph"]["edges"]), len(graph.get_edges()))
        for node in graph.get_nodes():
            metadata = node.get_metadata()
            if metadata["type"] != "node":
                self._check_metadata(node.get_metadata())
            else:
                self.assertEqual(len(metadata["properties"]), 1)
                if int(metadata["name"][len("compute") :]) < 10:
                    self.assertEqual(metadata["properties"]["pdebug"], "")
                else:
                    self.assertEqual(metadata["properties"]["pbatch"], "")


if __name__ == "__main__":
    from subflux import rerun_under_flux

    if rerun_under_flux(size=1):
        unittest.main(testRunner=TAPTestRunner())

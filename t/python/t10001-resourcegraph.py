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
    FluxionResourcePoolV1,
    FluxionResourceRelationshipV1,
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


class TestResourceGraph(unittest.TestCase):
    """Test for the ResourceGraph class."""

    def _check_metadata(self, metadata):
        if metadata["type"] in ("node", "core", "gpu", "cluster"):
            self.assertEqual(metadata["unit"], "")
            self.assertEqual(metadata["size"], 1)
            self.assertEqual(metadata["properties"], [])
        else:
            raise ValueError(metadata["type"])

    def test_basic(self):
        graph = FluxionResourceGraphV1(RV1)
        self.assertTrue(graph.is_directed())
        j = graph.to_JSON()
        json.dumps(j)  # make sure it doesn't throw an error
        self.assertTrue(j["graph"]["directed"])
        self.assertEqual(len(j["graph"]["nodes"]), len(graph.get_nodes()))
        self.assertEqual(len(j["graph"]["edges"]), len(graph.get_edges()))
        for node in graph.get_nodes():
            self._check_metadata(node.get_metadata())

    def test_basic_2(self):
        graph = FluxionResourceGraphV1(RV1_2)
        self.assertTrue(graph.is_directed())
        j = graph.to_JSON()
        json.dumps(j)
        self.assertTrue(j["graph"]["directed"])
        self.assertEqual(len(j["graph"]["nodes"]), len(graph.get_nodes()))
        self.assertEqual(len(j["graph"]["edges"]), len(graph.get_edges()))
        for node in graph.get_nodes():
            self._check_metadata(node.get_metadata())


unittest.main(testRunner=TAPTestRunner())

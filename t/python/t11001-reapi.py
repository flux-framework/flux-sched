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

"""Test the fluxion.reapi bindings."""

import copy
import errno
import json
import unittest
import subflux  # noqa: F401

from fluxion.reapi import (
    Reapi,
    ReapiError,
    ReapiInsufficientResources,
    ReapiInfeasibleRequest,
    MatchResult,
    JobInfo,
    RESOURCE_UP,
    RESOURCE_DOWN,
)

from pycotap import TAPTestRunner


def jgf_node(vid, name, rank, path):
    return {
        "id": vid,
        "metadata": {
            "type": "node",
            "basename": "node",
            "name": name,
            "size": 1,
            "unit": "",
            "rank": rank,
            "paths": {"containment": path},
        },
    }


def jgf_core(vid, name, rank, path):
    return {
        "id": vid,
        "metadata": {
            "type": "core",
            "basename": "core",
            "name": name,
            "size": 1,
            "unit": "",
            "id": 0,
            "rank": rank,
            "paths": {"containment": path},
        },
    }


JGF_CLUSTER = {
    "id": "0",
    "metadata": {
        "type": "cluster",
        "basename": "tiny",
        "name": "tiny0",
        "size": 1,
        "unit": "",
        "paths": {"containment": "/tiny0"},
    },
}

# One node with one core
TEST_JGF = {
    "graph": {
        "nodes": [
            JGF_CLUSTER,
            jgf_node("1", "node0", 0, "/tiny0/node0"),
            jgf_core("2", "core0", 0, "/tiny0/node0/core0"),
        ],
        "edges": [{"source": "0", "target": "1"}, {"source": "1", "target": "2"}],
    }
}

# A second node with one core, as a subgraph to attach to TEST_JGF
TEST_SUBGRAPH = {
    "graph": {
        "nodes": [
            JGF_CLUSTER,
            jgf_node("3", "node1", 1, "/tiny0/node1"),
            jgf_core("4", "core0", 1, "/tiny0/node1/core0"),
        ],
        "edges": [{"source": "0", "target": "3"}, {"source": "3", "target": "4"}],
    }
}

TEST_RV1 = {
    "version": 1,
    "execution": {
        "R_lite": [{"rank": "0", "children": {"core": "0"}}],
        "starttime": 0.0,
        "expiration": 0.0,
        "nodelist": ["node0"],
    },
}


def jobspec(nnodes=1):
    return {
        "version": 1,
        "resources": [
            {
                "type": "node",
                "count": nnodes,
                "with": [
                    {
                        "type": "slot",
                        "count": 1,
                        "label": "task",
                        "with": [{"type": "core", "count": 1}],
                    }
                ],
            }
        ],
        "tasks": [
            {"command": ["sleep", "0"], "slot": "task", "count": {"per_slot": 1}}
        ],
        "attributes": {"system": {"duration": 60.0}},
    }


TEST_JOBSPEC = jobspec(1)
INFEASIBLE_JOBSPEC = jobspec(100)


def without_scheduling(R):
    """Return an rv1 resource set with the scheduling key dropped."""
    return {k: v for k, v in R.items() if k != "scheduling"}


class TestReapiInit(unittest.TestCase):
    """Construction, options, and lifecycle."""

    def test_init_jgf(self):
        """Reapi can be created from a JGF graph with default options."""
        ctx = Reapi(TEST_JGF)
        self.assertIsInstance(ctx, Reapi)
        ctx.close()

    def test_init_string(self):
        """A pre-encoded JSON string is accepted as the graph."""
        ctx = Reapi(json.dumps(TEST_JGF))
        ctx.close()

    def test_init_rv1exec(self):
        """load_format=rv1exec reads an RFC 20 envelope."""
        with Reapi(TEST_RV1, load_format="rv1exec") as ctx:
            result = ctx.match_allocate(1, TEST_JOBSPEC)
            self.assertEqual(result.R["execution"]["nodelist"], ["node0"])

    def test_init_invalid_graph(self):
        """Construction fails with ReapiError on an unparsable graph."""
        with self.assertRaises(ReapiError):
            Reapi("not valid json")

    def test_init_invalid_options(self):
        """Each option is validated at construction."""
        for key in (
            "load_format",
            "match_format",
            "matcher_policy",
            "matcher_name",
            "traverser_policy",
            "prune_filters",
        ):
            with self.assertRaises(ReapiError, msg=key) as cm:
                Reapi(TEST_JGF, **{key: "bogus"})
            self.assertTrue(str(cm.exception), key)

    def test_init_all_options(self):
        """All options are forwarded and accepted with valid values."""
        with Reapi(
            TEST_JGF,
            load_format="jgf",
            match_format="rv1",
            matcher_policy="low",
            matcher_name="CA",
            traverser_policy="flexible",
            prune_filters="ALL:core",
            reserve_vtx_vec=16,
        ) as ctx:
            self.assertIn("scheduling", ctx.match_allocate(1, TEST_JOBSPEC).R)

    def test_match_policy_first(self):
        """matcher_policy=first works."""
        with Reapi(TEST_JGF, matcher_policy="first") as ctx:
            self.assertIsNotNone(ctx.match_allocate(1, TEST_JOBSPEC).R)

    def test_close_is_idempotent(self):
        """close() may be called more than once; methods then raise."""
        ctx = Reapi(TEST_JGF)
        ctx.close()
        ctx.close()
        with self.assertRaises(ReapiError):
            ctx.match_satisfiability(TEST_JOBSPEC)

    def test_context_manager(self):
        """The instance is closed on leaving a with block."""
        with Reapi(TEST_JGF) as ctx:
            ctx.match_satisfiability(TEST_JOBSPEC)
        with self.assertRaises(ReapiError):
            ctx.match_satisfiability(TEST_JOBSPEC)

    def test_del_without_close(self):
        """Dropping the last reference without close() is harmless."""
        ctx = Reapi(TEST_JGF)
        del ctx


class TestReapiMatch(unittest.TestCase):
    """Match operations."""

    def setUp(self):
        self.ctx = Reapi(TEST_JGF)

    def tearDown(self):
        self.ctx.close()

    def test_match_allocate(self):
        """match_allocate returns a MatchResult with every field populated."""
        result = self.ctx.match_allocate(1, TEST_JOBSPEC)
        self.assertIsInstance(result, MatchResult)
        self.assertEqual(result.R["execution"]["nodelist"], ["node0"])
        self.assertFalse(result.reserved)
        self.assertEqual(result.at, 0)
        self.assertGreaterEqual(result.overhead, 0.0)

    def test_match_allocate_string_jobspec(self):
        """A pre-encoded JSON string jobspec is accepted."""
        result = self.ctx.match_allocate(1, json.dumps(TEST_JOBSPEC))
        self.assertIsNotNone(result.R)

    def test_match_allocate_invalid_jobspec(self):
        """An unparsable jobspec raises ReapiError with errno EINVAL."""
        with self.assertRaises(ReapiError) as cm:
            self.ctx.match_allocate(1, "not valid json")
        self.assertEqual(cm.exception.errno, errno.EINVAL)

    def test_match_allocate_duplicate_jobid(self):
        """A second allocation under a live jobid raises EEXIST."""
        self.ctx.match_allocate(1, TEST_JOBSPEC)
        with self.assertRaises(ReapiError) as cm:
            self.ctx.match_allocate(1, TEST_JOBSPEC)
        self.assertEqual(cm.exception.errno, errno.EEXIST)
        # and the original allocation is intact
        self.assertEqual(self.ctx.info(1).mode, "ALLOCATED")
        self.ctx.cancel(1)

    def test_match_allocate_jobid_reuse_after_cancel(self):
        """A jobid may be reused once its allocation is canceled."""
        self.ctx.match_allocate(1, TEST_JOBSPEC)
        self.ctx.cancel(1)
        self.assertIsNotNone(self.ctx.match_allocate(1, TEST_JOBSPEC).R)

    def test_insufficient_resources(self):
        """A busy graph raises ReapiInsufficientResources (errno EBUSY)."""
        self.ctx.match_allocate(1, TEST_JOBSPEC)
        with self.assertRaises(ReapiInsufficientResources) as cm:
            self.ctx.match_allocate(2, TEST_JOBSPEC)
        self.assertEqual(cm.exception.errno, errno.EBUSY)

    def test_no_double_booking(self):
        """Allocated resources become available again after cancel."""
        self.ctx.match_allocate(1, TEST_JOBSPEC)
        with self.assertRaises(ReapiInsufficientResources):
            self.ctx.match_allocate(2, TEST_JOBSPEC)
        self.ctx.cancel(1)
        self.assertIsNotNone(self.ctx.match_allocate(3, TEST_JOBSPEC).R)

    def test_infeasible_without_check(self):
        """By default an infeasible request looks like a busy one."""
        with self.assertRaises(ReapiInsufficientResources):
            self.ctx.match_allocate(1, INFEASIBLE_JOBSPEC)

    def test_check_satisfiability(self):
        """check_satisfiability distinguishes infeasible from busy."""
        with self.assertRaises(ReapiInfeasibleRequest) as cm:
            self.ctx.match_allocate(1, INFEASIBLE_JOBSPEC, check_satisfiability=True)
        self.assertEqual(cm.exception.errno, errno.ENODEV)
        self.ctx.match_allocate(1, TEST_JOBSPEC)
        with self.assertRaises(ReapiInsufficientResources):
            self.ctx.match_allocate(2, TEST_JOBSPEC, check_satisfiability=True)

    def test_orelse_reserve(self):
        """orelse_reserve reserves at a future time when resources are busy."""
        self.ctx.match_allocate(1, TEST_JOBSPEC)
        result = self.ctx.match_allocate(2, TEST_JOBSPEC, orelse_reserve=True)
        self.assertTrue(result.reserved)
        self.assertGreater(result.at, 0)
        self.assertIsNotNone(result.R)
        info = self.ctx.info(2)
        self.assertEqual(info.mode, "RESERVED")
        self.assertTrue(info.reserved)
        self.assertEqual(info.at, result.at)

    def test_orelse_reserve_and_check_satisfiability_exclusive(self):
        """orelse_reserve and check_satisfiability cannot be combined."""
        with self.assertRaises(ValueError):
            self.ctx.match_allocate(
                1, TEST_JOBSPEC, orelse_reserve=True, check_satisfiability=True
            )

    def test_match_without_allocating(self):
        """match_without_allocating returns a match and leaves no trace."""
        result = self.ctx.match_without_allocating(TEST_JOBSPEC)
        self.assertEqual(result.R["execution"]["nodelist"], ["node0"])
        self.assertFalse(result.reserved)
        self.assertEqual(result.at, 0)
        self.assertIsNone(self.ctx.find("sched-now=allocated"))
        # the resources are still available
        self.assertIsNotNone(self.ctx.match_allocate(1, TEST_JOBSPEC).R)

    def test_match_without_allocating_busy(self):
        """match_without_allocating raises when nothing is available now."""
        self.ctx.match_allocate(1, TEST_JOBSPEC)
        with self.assertRaises(ReapiInsufficientResources):
            self.ctx.match_without_allocating(TEST_JOBSPEC)

    def test_match_without_allocating_future(self):
        """future=True finds the earliest time the request fits."""
        self.ctx.match_allocate(1, TEST_JOBSPEC)
        result = self.ctx.match_without_allocating(TEST_JOBSPEC, future=True)
        self.assertGreater(result.at, 0)
        self.assertEqual(result.R["execution"]["nodelist"], ["node0"])
        self.assertFalse(result.reserved)
        with self.assertRaises(ReapiError):
            self.ctx.info(2**64 - 1)

    def test_match_satisfiability(self):
        """match_satisfiability returns a MatchResult with R=None."""
        result = self.ctx.match_satisfiability(TEST_JOBSPEC)
        self.assertIsInstance(result, MatchResult)
        self.assertIsNone(result.R)
        self.assertGreaterEqual(result.overhead, 0.0)

    def test_match_satisfiability_infeasible(self):
        """match_satisfiability raises ReapiInfeasibleRequest."""
        with self.assertRaises(ReapiInfeasibleRequest):
            self.ctx.match_satisfiability(INFEASIBLE_JOBSPEC)

    def test_match_satisfiability_busy_ok(self):
        """match_satisfiability ignores current allocations."""
        self.ctx.match_allocate(1, TEST_JOBSPEC)
        self.ctx.match_satisfiability(TEST_JOBSPEC)

    def test_match_satisfiability_invalid_jobspec(self):
        """match_satisfiability raises ReapiError on a bad jobspec."""
        with self.assertRaises(ReapiError):
            self.ctx.match_satisfiability("not valid json")


class TestReapiFormats(unittest.TestCase):
    """match_format handling of returned resource sets."""

    def test_rv1_nosched_default(self):
        """The default match format is rv1_nosched (a dict without scheduling)."""
        with Reapi(TEST_JGF) as ctx:
            R = ctx.match_allocate(1, TEST_JOBSPEC).R
            self.assertIsInstance(R, dict)
            self.assertEqual(R["version"], 1)
            self.assertIn("execution", R)
            self.assertNotIn("scheduling", R)

    def test_rv1(self):
        """rv1 includes a scheduling section."""
        with Reapi(TEST_JGF, match_format="rv1") as ctx:
            R = ctx.match_allocate(1, TEST_JOBSPEC).R
            self.assertIn("scheduling", R)

    def test_rlite(self):
        """rlite is returned as a list."""
        with Reapi(TEST_JGF, match_format="rlite") as ctx:
            R = ctx.match_allocate(1, TEST_JOBSPEC).R
            self.assertIsInstance(R, list)
            self.assertEqual(R[0]["rank"], "0")

    def test_jgf(self):
        """jgf is returned as a dict with a graph key."""
        with Reapi(TEST_JGF, match_format="jgf") as ctx:
            R = ctx.match_allocate(1, TEST_JOBSPEC).R
            self.assertIn("graph", R)

    def test_text_formats(self):
        """simple and pretty_simple are returned as str."""
        for fmt in ("simple", "pretty_simple"):
            with Reapi(TEST_JGF, match_format=fmt) as ctx:
                R = ctx.match_allocate(1, TEST_JOBSPEC).R
                self.assertIsInstance(R, str, fmt)
                self.assertIn("node0", R)


class TestReapiCancel(unittest.TestCase):
    """Full and partial cancel."""

    def test_cancel(self):
        """cancel without R frees everything and returns True."""
        with Reapi(TEST_JGF) as ctx:
            ctx.match_allocate(1, TEST_JOBSPEC)
            self.assertTrue(ctx.cancel(1))
            self.assertIsNone(ctx.find("sched-now=allocated"))
            self.assertEqual(ctx.info(1).mode, "CANCELED")

    def test_cancel_nonexistent(self):
        """cancel of an unknown jobid raises ENOENT naming the jobid."""
        with Reapi(TEST_JGF) as ctx:
            with self.assertRaises(ReapiError) as cm:
                ctx.cancel(99999)
            self.assertEqual(cm.exception.errno, errno.ENOENT)
            self.assertIn("99999", str(cm.exception))

    def test_cancel_noent_ok(self):
        """noent_ok suppresses the error for an unknown jobid."""
        with Reapi(TEST_JGF) as ctx:
            self.assertTrue(ctx.cancel(99999, noent_ok=True))

    def test_cancel_reservation(self):
        """A reservation can be canceled."""
        with Reapi(TEST_JGF) as ctx:
            ctx.match_allocate(1, TEST_JOBSPEC)
            ctx.match_allocate(2, TEST_JOBSPEC, orelse_reserve=True)
            self.assertTrue(ctx.cancel(2))
            self.assertEqual(ctx.info(2).mode, "CANCELED")

    def test_partial_cancel_full_R(self):
        """Partial cancel with the whole rv1_nosched R is a full removal."""
        with Reapi(TEST_JGF) as ctx:
            R = ctx.match_allocate(1, TEST_JOBSPEC).R
            self.assertTrue(ctx.cancel(1, R=R))
            self.assertIsNone(ctx.find("sched-now=allocated"))
            self.assertIsNotNone(ctx.match_allocate(2, TEST_JOBSPEC).R)

    def test_partial_cancel_subset(self):
        """Partial cancel of one of two nodes returns False, then True."""
        with Reapi(TEST_JGF) as ctx:
            ctx.add_subgraph(TEST_SUBGRAPH)
            R = ctx.match_allocate(1, jobspec(2)).R
            self.assertEqual(R["execution"]["nodelist"], ["node[0-1]"])
            R_node1 = copy.deepcopy(R)
            R_node1["execution"]["R_lite"] = [{"rank": "1", "children": {"core": "0"}}]
            R_node1["execution"]["nodelist"] = ["node1"]
            self.assertFalse(ctx.cancel(1, R=R_node1))
            # node1 is free again, node0 is not
            allocated = ctx.find("sched-now=allocated")
            self.assertEqual(allocated["execution"]["nodelist"], ["node0"])
            R_node0 = copy.deepcopy(R)
            R_node0["execution"]["R_lite"] = [{"rank": "0", "children": {"core": "0"}}]
            R_node0["execution"]["nodelist"] = ["node0"]
            self.assertTrue(ctx.cancel(1, R=R_node0))
            self.assertIsNone(ctx.find("sched-now=allocated"))

    def test_partial_cancel_rv1_without_scheduling(self):
        """An rv1 R works for partial cancel once scheduling is dropped."""
        with Reapi(TEST_JGF, match_format="rv1") as ctx:
            R = ctx.match_allocate(1, TEST_JOBSPEC).R
            self.assertTrue(ctx.cancel(1, R=without_scheduling(R)))

    def test_partial_cancel_rv1_with_scheduling_unsupported(self):
        """An rv1 R with its scheduling section is rejected."""
        with Reapi(TEST_JGF, match_format="rv1") as ctx:
            R = ctx.match_allocate(1, TEST_JOBSPEC).R
            with self.assertRaises(ReapiError):
                ctx.cancel(1, R=R)
            # the allocation is intact
            self.assertEqual(ctx.info(1).mode, "ALLOCATED")

    def test_partial_cancel_rlite_unsupported(self):
        """An rlite R (bare list) is not an RFC 20 envelope and is rejected."""
        with Reapi(TEST_JGF, match_format="rlite") as ctx:
            R = ctx.match_allocate(1, TEST_JOBSPEC).R
            with self.assertRaises(ReapiError):
                ctx.cancel(1, R=R)

    def test_partial_cancel_invalid_R(self):
        """An unparsable R raises ReapiError."""
        with Reapi(TEST_JGF) as ctx:
            ctx.match_allocate(1, TEST_JOBSPEC)
            with self.assertRaises(ReapiError):
                ctx.cancel(1, R="not valid json")

    def test_partial_cancel_nonexistent(self):
        """Partial cancel of an unknown jobid raises unless noent_ok."""
        with Reapi(TEST_JGF) as ctx:
            with self.assertRaises(ReapiError):
                ctx.cancel(99999, R=TEST_RV1)
            ctx.cancel(99999, R=TEST_RV1, noent_ok=True)


class TestReapiUpdate(unittest.TestCase):
    """update_allocate."""

    def test_update_allocate(self):
        """update_allocate re-registers an allocation from its R."""
        with Reapi(TEST_JGF) as ctx:
            R = ctx.match_allocate(1, TEST_JOBSPEC).R
            ctx.cancel(1)
            result = ctx.update_allocate(2, R)
            self.assertIsInstance(result, MatchResult)
            self.assertEqual(result.at, 0)
            self.assertEqual(result.R["execution"]["nodelist"], ["node0"])
            self.assertEqual(ctx.info(2).mode, "ALLOCATED")
            with self.assertRaises(ReapiInsufficientResources):
                ctx.match_allocate(3, TEST_JOBSPEC)
            ctx.cancel(2)
            self.assertIsNotNone(ctx.match_allocate(4, TEST_JOBSPEC).R)

    def test_update_allocate_replay_after_cancel(self):
        """The same R can be replayed repeatedly after each cancel."""
        with Reapi(TEST_JGF) as ctx:
            R = ctx.match_allocate(1, TEST_JOBSPEC).R
            ctx.cancel(1)
            for i in range(3):
                ctx.update_allocate(10 + i, R)
                self.assertIsNotNone(ctx.find("sched-now=allocated"))
                ctx.cancel(10 + i)
                self.assertIsNone(ctx.find("sched-now=allocated"))

    def test_update_allocate_rv1(self):
        """An rv1 R (with scheduling) can be replayed."""
        with Reapi(TEST_JGF, match_format="rv1") as ctx:
            R = ctx.match_allocate(1, TEST_JOBSPEC).R
            ctx.cancel(1)
            self.assertIn("scheduling", ctx.update_allocate(2, R).R)

    def test_update_allocate_duplicate_jobid(self):
        """update_allocate of a live jobid raises EEXIST."""
        with Reapi(TEST_JGF) as ctx:
            R = ctx.match_allocate(1, TEST_JOBSPEC).R
            with self.assertRaises(ReapiError) as cm:
                ctx.update_allocate(1, R)
            self.assertEqual(cm.exception.errno, errno.EEXIST)

    def test_update_allocate_invalid_R(self):
        """update_allocate raises ReapiError on an unparsable R."""
        with Reapi(TEST_JGF) as ctx:
            with self.assertRaises(ReapiError):
                ctx.update_allocate(1, "not valid json")


class TestReapiInfoFind(unittest.TestCase):
    """info and find."""

    def test_info(self):
        """info returns a JobInfo for an allocated job."""
        with Reapi(TEST_JGF) as ctx:
            result = ctx.match_allocate(42, TEST_JOBSPEC)
            info = ctx.info(42)
            self.assertIsInstance(info, JobInfo)
            self.assertEqual(info.mode, "ALLOCATED")
            self.assertFalse(info.reserved)
            self.assertEqual(info.at, 0)
            self.assertEqual(info.overhead, result.overhead)

    def test_info_nonexistent(self):
        """info of an unknown jobid raises ReapiError."""
        with Reapi(TEST_JGF) as ctx:
            with self.assertRaises(ReapiError):
                ctx.info(99999)

    def test_find(self):
        """find returns None, then the allocated set, then None again."""
        with Reapi(TEST_JGF) as ctx:
            self.assertIsNone(ctx.find("sched-now=allocated"))
            ctx.match_allocate(1, TEST_JOBSPEC)
            R = ctx.find("sched-now=allocated")
            self.assertEqual(R["execution"]["nodelist"], ["node0"])
            ctx.cancel(1)
            self.assertIsNone(ctx.find("sched-now=allocated"))

    def test_find_status(self):
        """find status=up/down reflects set_status."""
        with Reapi(TEST_JGF) as ctx:
            self.assertIsNone(ctx.find("status=down"))
            ctx.set_rank_status(0, RESOURCE_DOWN)
            self.assertIsNotNone(ctx.find("status=down"))
            self.assertIsNone(ctx.find("status=up"))

    def test_find_format(self):
        """find format overrides the match format for the result."""
        with Reapi(TEST_JGF) as ctx:
            self.assertIn("graph", ctx.find("status=up", format="jgf"))
            self.assertIsInstance(ctx.find("status=up", format="rlite"), list)
            self.assertIn("execution", ctx.find("status=up", format="rv1"))

    def test_find_invalid(self):
        """find rejects unknown criteria and formats."""
        with Reapi(TEST_JGF) as ctx:
            with self.assertRaises(ReapiError):
                ctx.find("status=up", format="bogus")
            with self.assertRaises(ReapiError):
                ctx.find("bogus=criteria")

    def test_find_text_format(self):
        """find cannot return the text formats."""
        with Reapi(TEST_JGF, match_format="simple") as ctx:
            with self.assertRaises(ValueError):
                ctx.find("status=up")
            with self.assertRaises(ValueError):
                ctx.find("status=up", format="pretty_simple")
            # but an explicit JSON format works on a text-format instance
            self.assertIsNotNone(ctx.find("status=up", format="rv1_nosched"))


class TestReapiStatus(unittest.TestCase):
    """Resource up/down status."""

    def test_rank_status(self):
        """set_rank_status by int and 'all' is reflected by get_rank_status."""
        with Reapi(TEST_JGF) as ctx:
            self.assertEqual(ctx.get_rank_status(0), RESOURCE_UP)
            ctx.set_rank_status(0, RESOURCE_DOWN)
            self.assertEqual(ctx.get_rank_status(0), RESOURCE_DOWN)
            self.assertEqual(ctx.get_rank_status("0"), RESOURCE_DOWN)
            with self.assertRaises(ReapiInsufficientResources):
                ctx.match_allocate(1, TEST_JOBSPEC)
            ctx.set_rank_status("all", RESOURCE_UP)
            self.assertEqual(ctx.get_rank_status(0), RESOURCE_UP)
            self.assertIsNotNone(ctx.match_allocate(2, TEST_JOBSPEC).R)

    def test_rank_status_idset(self):
        """set_rank_status accepts an idset; unknown ranks are ignored."""
        with Reapi(TEST_JGF) as ctx:
            ctx.set_rank_status("0-3", RESOURCE_DOWN)
            self.assertEqual(ctx.get_rank_status(0), RESOURCE_DOWN)
            ctx.set_rank_status(999, RESOURCE_DOWN)

    def test_get_rank_status_invalid(self):
        """get_rank_status rejects unknown ranks and multi-rank idsets."""
        with Reapi(TEST_JGF) as ctx:
            ctx.add_subgraph(TEST_SUBGRAPH)
            for rank in (999, "0-1", "all"):
                with self.assertRaises(ReapiError, msg=rank):
                    ctx.get_rank_status(rank)

    def test_path_status(self):
        """set_status by path is reflected by get_status."""
        with Reapi(TEST_JGF) as ctx:
            self.assertEqual(ctx.get_status("/tiny0/node0"), RESOURCE_UP)
            ctx.set_status("/tiny0/node0", RESOURCE_DOWN)
            self.assertEqual(ctx.get_status("/tiny0/node0"), RESOURCE_DOWN)
            with self.assertRaises(ReapiInsufficientResources):
                ctx.match_allocate(1, TEST_JOBSPEC)
            ctx.set_status("/tiny0/node0", RESOURCE_UP)
            self.assertIsNotNone(ctx.match_allocate(2, TEST_JOBSPEC).R)

    def test_path_status_invalid(self):
        """Status by an unknown path raises ReapiError."""
        with Reapi(TEST_JGF) as ctx:
            with self.assertRaises(ReapiError):
                ctx.set_status("/nonexistent", RESOURCE_DOWN)
            with self.assertRaises(ReapiError):
                ctx.get_status("/nonexistent")


class TestReapiSubgraph(unittest.TestCase):
    """add_subgraph and remove_subgraph."""

    def test_add_subgraph(self):
        """A node added by add_subgraph can be allocated."""
        with Reapi(TEST_JGF) as ctx:
            with self.assertRaises(ReapiInsufficientResources):
                ctx.match_allocate(1, jobspec(2))
            ctx.add_subgraph(TEST_SUBGRAPH)
            self.assertEqual(
                ctx.find("status=up")["execution"]["nodelist"], ["node[0-1]"]
            )
            self.assertEqual(ctx.get_rank_status(1), RESOURCE_UP)
            R = ctx.match_allocate(1, jobspec(2)).R
            self.assertEqual(R["execution"]["nodelist"], ["node[0-1]"])

    def test_remove_subgraph(self):
        """A node removed by remove_subgraph is gone."""
        with Reapi(TEST_JGF) as ctx:
            ctx.add_subgraph(TEST_SUBGRAPH)
            ctx.remove_subgraph("/tiny0/node1")
            self.assertEqual(ctx.find("status=up")["execution"]["nodelist"], ["node0"])
            with self.assertRaises(ReapiError):
                ctx.get_status("/tiny0/node1")
            with self.assertRaises(ReapiInsufficientResources):
                ctx.match_allocate(1, jobspec(2))

    def test_remove_subgraph_nonexistent(self):
        """remove_subgraph of an unknown path raises ENOENT."""
        with Reapi(TEST_JGF) as ctx:
            with self.assertRaises(ReapiError) as cm:
                ctx.remove_subgraph("/tiny0/nonexistent")
            self.assertEqual(cm.exception.errno, errno.ENOENT)

    def test_add_subgraph_invalid(self):
        """add_subgraph of unparsable input raises ReapiError."""
        with Reapi(TEST_JGF) as ctx:
            with self.assertRaises(ReapiError):
                ctx.add_subgraph("not valid json")


class TestReapiClone(unittest.TestCase):
    """clone."""

    def test_clone_is_independent(self):
        """Changes to a clone do not affect the original and vice versa."""
        with Reapi(TEST_JGF) as ctx:
            ctx.match_allocate(1, TEST_JOBSPEC)
            with ctx.clone() as sim:
                sim.cancel(1)
                self.assertIsNotNone(sim.match_allocate(2, TEST_JOBSPEC).R)
                with self.assertRaises(ReapiInsufficientResources):
                    ctx.match_allocate(2, TEST_JOBSPEC)
                ctx.cancel(1)
                self.assertEqual(sim.info(2).mode, "ALLOCATED")

    def test_clone_inherits_state(self):
        """A clone sees the original's allocations and status."""
        with Reapi(TEST_JGF, match_format="rv1") as ctx:
            ctx.add_subgraph(TEST_SUBGRAPH)
            ctx.set_rank_status(1, RESOURCE_DOWN)
            ctx.match_allocate(1, TEST_JOBSPEC)  # lands on node0
            with ctx.clone() as sim:
                self.assertEqual(sim.info(1).mode, "ALLOCATED")
                self.assertEqual(sim.get_rank_status(1), RESOURCE_DOWN)
                with self.assertRaises(ReapiInsufficientResources):
                    sim.match_allocate(2, TEST_JOBSPEC)
                sim.set_rank_status(1, RESOURCE_UP)
                # the clone keeps the original's match_format
                self.assertIn("scheduling", sim.match_allocate(2, TEST_JOBSPEC).R)
                self.assertEqual(ctx.get_rank_status(1), RESOURCE_DOWN)

    def test_clone_closed(self):
        """clone of a closed instance raises ReapiError."""
        ctx = Reapi(TEST_JGF)
        ctx.close()
        with self.assertRaises(ReapiError):
            ctx.clone()


if __name__ == "__main__":
    unittest.main(testRunner=TAPTestRunner(), verbosity=2)

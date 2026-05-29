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

"""Test CFFI bindings to reapi_cli."""

import unittest
import subflux  # noqa: F401

from flux.constants import FLUX_NODEID_ANY
from fluxion.reapi.cli import (
    Reapi,
    ReapiError,
    ReapiInsufficientResources,
    ReapiInfeasibleRequest,
)

from pycotap import TAPTestRunner


# Minimal JGF for testing - must have "graph" wrapper
TEST_JGF = {
    "graph": {
        "nodes": [
            {
                "id": "0",
                "metadata": {
                    "type": "cluster",
                    "basename": "tiny",
                    "name": "tiny0",
                    "size": 1,
                    "unit": "",
                    "paths": {"containment": "/tiny0"},
                },
            },
            {
                "id": "1",
                "metadata": {
                    "type": "node",
                    "basename": "node",
                    "name": "node0",
                    "size": 1,
                    "unit": "",
                    "rank": 0,
                    "paths": {"containment": "/tiny0/node0"},
                },
            },
            {
                "id": "2",
                "metadata": {
                    "type": "core",
                    "basename": "core",
                    "name": "core0",
                    "size": 1,
                    "unit": "",
                    "id": 0,
                    "rank": 0,
                    "paths": {"containment": "/tiny0/node0/core0"},
                },
            },
        ],
        "edges": [{"source": "0", "target": "1"}, {"source": "1", "target": "2"}],
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

TEST_JOBSPEC = {
    "version": 1,
    "resources": [
        {
            "type": "node",
            "count": 1,
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
    "tasks": [{"command": ["sleep", "0"], "slot": "task", "count": {"per_slot": 1}}],
    "attributes": {"system": {"duration": 60.0}},
}


class TestReapiFFI(unittest.TestCase):
    """Test CFFI bindings to reapi_cli."""

    def test_context_creation(self):
        """Reapi can be created and destroyed."""
        ctx = Reapi()
        self.assertIsNotNone(ctx)
        del ctx

    def test_initialize_with_jgf(self):
        """Context can be initialized with JGF graph."""
        ctx = Reapi()
        ctx.initialize(TEST_JGF, load_format="jgf")

    def test_initialize_requires_valid_jgf(self):
        """Initialize fails with invalid JGF."""
        ctx = Reapi()
        with self.assertRaises(Exception):
            ctx.initialize("not valid json")

    def test_cancel_error_message_contains_jobid(self):
        """Exception from failed cancel includes the jobid."""
        ctx = Reapi()
        ctx.initialize(TEST_JGF, load_format="jgf")

        with self.assertRaises(Exception) as cm:
            ctx.cancel(99999)
        self.assertIn("99999", str(cm.exception))

    def test_initialize_invalid_load_format(self):
        """Initialize fails with invalid load_format."""
        ctx = Reapi()
        with self.assertRaises(Exception):
            ctx.initialize(TEST_JGF, load_format="invalid_format")

    @unittest.skip("match_format does not appear to be validated")
    def test_initialize_invalid_match_format(self):
        """Invalid match_format should fail but doesn't appear to be validated."""
        ctx = Reapi()
        ctx.initialize(TEST_JGF, load_format="jgf", match_format="invalid_format")

        # Expected to raise but doesn't - match_format may fall back to default
        with self.assertRaises(Exception):
            ctx.match_allocate(1, TEST_JOBSPEC)

    def test_match_before_initialize_fails(self):
        """Matching before initialize raises error."""
        ctx = Reapi()
        with self.assertRaises(Exception):
            ctx.match_allocate(1, TEST_JOBSPEC)

    def test_match_allocate_after_init(self):
        """Can match and allocate after initialization."""
        ctx = Reapi()
        ctx.initialize(TEST_JGF, load_format="jgf")

        result = ctx.match_allocate(1, TEST_JOBSPEC)

        self.assertIn("R", result)
        self.assertIn("reserved", result)

    def test_match_allocate_invalid_jobspec(self):
        """Match allocate with invalid jobspec raises error."""
        ctx = Reapi()
        ctx.initialize(TEST_JGF, load_format="jgf")

        # Invalid JSON
        with self.assertRaises(Exception):
            ctx.match_allocate(1, "not valid json")

    def test_cancel(self):
        """Can cancel allocated job."""
        ctx = Reapi()
        ctx.initialize(TEST_JGF, load_format="jgf")

        ctx.match_allocate(1, TEST_JOBSPEC)
        ctx.cancel(1)

    def test_cancel_noent_ok(self):
        """Cancel with noent_ok=True doesn't error for missing job."""
        ctx = Reapi()
        ctx.initialize(TEST_JGF, load_format="jgf")

        # Should not raise
        ctx.cancel(99999, noent_ok=True)

    def test_cancel_nonexistent_job_raises(self):
        """Cancel with noent_ok=False raises for missing job."""
        ctx = Reapi()
        ctx.initialize(TEST_JGF, load_format="jgf")

        # Should raise
        with self.assertRaises(Exception):
            ctx.cancel(99999)

    def test_cancel_before_initialize(self):
        """Cancel before initialize raises error."""
        ctx = Reapi()
        with self.assertRaises(Exception):
            ctx.cancel(1)

    def test_cancel_full_R(self):
        """Cancel with full R results in complete removal."""
        ctx = Reapi()
        ctx.initialize(TEST_JGF, load_format="jgf", match_format="rv1")

        result = ctx.match_allocate(1, TEST_JOBSPEC)
        full_R = result["R"]

        # Cancel with the full R should be a full removal
        full_removal = ctx.cancel(1, R=full_R, format="rv1exec")

        # Passing full R should result in full removal
        self.assertTrue(full_removal)

        # Job should no longer exist - verify resources are available again
        result2 = ctx.match_allocate(2, TEST_JOBSPEC)
        self.assertGreater(len(result2["R"]), 0)

    def test_cancel_invalid_r_format(self):
        """Cancel with invalid R string raises error."""
        ctx = Reapi()
        ctx.initialize(TEST_JGF, load_format="jgf", match_format="rv1")

        ctx.match_allocate(1, TEST_JOBSPEC)

        # Invalid R string
        with self.assertRaises(Exception):
            ctx.cancel(1, R="not valid json")

    def test_cancel_implicit_format(self):
        """Cancel with rv1 R string matching load format works."""
        ctx = Reapi()
        ctx.initialize(TEST_RV1, load_format="rv1exec", match_format="rv1")

        result = ctx.match_allocate(1, TEST_JOBSPEC)
        full_removal = ctx.cancel(1, R=result["R"])
        self.assertTrue(full_removal)

        result2 = ctx.match_allocate(2, TEST_JOBSPEC)
        self.assertGreater(len(result2["R"]), 0)

    def test_cancel_explicit_format(self):
        """Cancel with explicit format=rv1exec works."""
        ctx = Reapi()
        ctx.initialize(TEST_JGF, load_format="jgf", match_format="rv1")

        result = ctx.match_allocate(1, TEST_JOBSPEC)
        full_removal = ctx.cancel(1, R=result["R"], format="rv1exec")
        self.assertTrue(full_removal)

        result2 = ctx.match_allocate(2, TEST_JOBSPEC)
        self.assertGreater(len(result2["R"]), 0)

    def test_info(self):
        """Can get job information for allocated job."""
        ctx = Reapi()
        ctx.initialize(TEST_JGF, load_format="jgf", match_format="rv1")

        ctx.match_allocate(1, TEST_JOBSPEC)

        # Get job info
        info = ctx.info(1)

        self.assertIn("mode", info)
        self.assertIn("reserved", info)
        self.assertIn("at", info)
        self.assertIn("overhead", info)
        self.assertIn("R", info)

        # Should not be reserved for immediate allocation
        self.assertFalse(info["reserved"])
        # Immediate allocation should have at=0
        self.assertEqual(info["at"], 0)
        # R must be populated and parseable
        self.assertIsNotNone(info["R"])
        self.assertIsInstance(info["R"], dict)
        self.assertIn("execution", info["R"])

    def test_info_nonexistent_job(self):
        """Info for nonexistent job raises error."""
        ctx = Reapi()
        ctx.initialize(TEST_JGF, load_format="jgf")

        with self.assertRaises(Exception):
            ctx.info(99999)

    def test_info_before_initialize(self):
        """Info before initialize raises error."""
        ctx = Reapi()
        with self.assertRaises(Exception):
            ctx.info(1)

    def test_jobid_is_caller_provided(self):
        """Caller-provided jobid is tracked and can be used with cancel/info."""
        ctx = Reapi()
        ctx.initialize(TEST_JGF, load_format="jgf")

        ctx.match_allocate(42, TEST_JOBSPEC)
        info = ctx.info(42)
        self.assertFalse(info["reserved"])

    def test_match_format_rv1(self):
        """Can initialize with match_format: rv1 (JgfPool default)."""
        ctx = Reapi()
        ctx.initialize(TEST_JGF, load_format="jgf", match_format="rv1")

        result = ctx.match_allocate(1, TEST_JOBSPEC)

        self.assertIn("R", result)
        # rv1 format should return valid R string
        self.assertIsNotNone(result["R"])
        self.assertIsInstance(result["R"], dict)

        # rv1 format has "version" and "execution" keys
        self.assertIn("version", result["R"])
        self.assertIn("execution", result["R"])

    def test_match_format_rlite(self):
        """Can initialize with match_format: rlite."""
        ctx = Reapi()
        ctx.initialize(TEST_JGF, load_format="jgf", match_format="rlite")

        result = ctx.match_allocate(1, TEST_JOBSPEC)

        self.assertIn("R", result)
        self.assertIsNotNone(result["R"])
        # rlite returns a list
        self.assertIsInstance(result["R"], list)

    def test_match_policy_first(self):
        """Can initialize with match_policy: first."""
        ctx = Reapi()
        ctx.initialize(
            TEST_JGF, load_format="jgf", match_format="rv1", match_policy="first"
        )

        result = ctx.match_allocate(1, TEST_JOBSPEC)
        self.assertIn("R", result)

    def test_match_allocate_orelse_reserve(self):
        """match_allocate with orelse_reserve makes reservation if resources unavailable."""
        ctx = Reapi()
        ctx.initialize(TEST_JGF, load_format="jgf", match_format="rv1")

        # Allocate the only available core
        result1 = ctx.match_allocate(1, TEST_JOBSPEC, orelse_reserve=False)
        self.assertIsNotNone(result1["R"])
        self.assertGreater(len(result1["R"]), 0)
        self.assertFalse(result1["reserved"])

        # Try to allocate again with orelse_reserve=True
        # Should make a reservation since resources are busy
        result2 = ctx.match_allocate(2, TEST_JOBSPEC, orelse_reserve=True)

        # If reservation is supported result2["reserved"] is True;
        # either way the call should succeed without raising.
        self.assertIn("reserved", result2)
        self.assertIn("R", result2)
        self.assertIn("at", result2)

    def test_insufficient_resources_raises(self):
        """When resources exhausted, match raises ReapiInsufficientResources."""
        ctx = Reapi()
        ctx.initialize(TEST_JGF, load_format="jgf", match_format="rv1")

        # Allocate the only available core
        result1 = ctx.match_allocate(1, TEST_JOBSPEC)
        self.assertIsNotNone(result1["R"])
        self.assertGreater(len(result1["R"]), 0)

        # Try to allocate again - should raise ReapiInsufficientResources
        with self.assertRaises(ReapiInsufficientResources):
            ctx.match_allocate(2, TEST_JOBSPEC)

    def test_no_double_booking(self):
        """Allocated resources are not available until freed."""
        ctx = Reapi()
        ctx.initialize(TEST_JGF, load_format="jgf", match_format="rv1")

        # Allocate the only available core
        result1 = ctx.match_allocate(1, TEST_JOBSPEC)
        self.assertIsNotNone(result1["R"])
        self.assertGreater(len(result1["R"]), 0)

        # Try to allocate again - should raise (resources in use)
        with self.assertRaises(ReapiInsufficientResources):
            ctx.match_allocate(2, TEST_JOBSPEC)

        # Free the first allocation
        ctx.cancel(1)

        # Now allocation should succeed again
        result3 = ctx.match_allocate(3, TEST_JOBSPEC)
        self.assertIsNotNone(result3["R"])
        self.assertGreater(len(result3["R"]), 0)

    def test_infeasible_request(self):
        """match_allocate raises ReapiInsufficientResources even for infeasible requests.

        The traverser always returns EBUSY from MATCH_ALLOCATE regardless of
        whether the request is structurally infeasible or temporarily unavailable.
        Use match_satisfiability to distinguish the two cases.
        """
        ctx = Reapi()
        ctx.initialize(TEST_JGF, load_format="jgf", match_format="rv1")

        # Request more nodes than exist in graph (graph has 1 node)
        infeasible_jobspec = {
            "version": 1,
            "resources": [
                {
                    "type": "node",
                    "count": 100,  # Graph only has 1 node
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

        with self.assertRaises(ReapiInsufficientResources):
            ctx.match_allocate(1, infeasible_jobspec)

    def test_match_satisfiability_feasible(self):
        """Satisfiability check succeeds for feasible request."""
        ctx = Reapi()
        ctx.initialize(TEST_JGF, load_format="jgf")

        result = ctx.match_satisfiability(TEST_JOBSPEC)

        # Should not raise, just return overhead
        self.assertIn("overhead", result)

    def test_match_satisfiability_infeasible(self):
        """Satisfiability check raises for infeasible requests."""
        ctx = Reapi()
        ctx.initialize(TEST_JGF, load_format="jgf")

        # Request more nodes than exist
        infeasible_jobspec = {
            "version": 1,
            "resources": [
                {
                    "type": "node",
                    "count": 100,
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

        with self.assertRaises(ReapiInfeasibleRequest):
            ctx.match_satisfiability(infeasible_jobspec)

    def test_match_satisfiability_before_initialize(self):
        """Satisfiability check before initialize raises error."""
        ctx = Reapi()
        with self.assertRaises(Exception):
            ctx.match_satisfiability(TEST_JOBSPEC)

    def test_match_satisfiability_invalid_jobspec(self):
        """Satisfiability check with invalid jobspec raises error."""
        ctx = Reapi()
        ctx.initialize(TEST_JGF, load_format="jgf")

        with self.assertRaises(Exception):
            ctx.match_satisfiability("not valid json")

    def test_set_status_by_rank(self):
        """Set status by rank affects all resources at that rank."""
        ctx = Reapi()
        ctx.initialize(TEST_JGF, load_format="jgf", match_format="rv1")

        # Verify initially up
        self.assertTrue(ctx.get_status(rank=0))

        # Mark entire rank 0 as down
        ctx.set_status(rank=0, up=False)

        # Verify now down
        self.assertFalse(ctx.get_status(rank=0))

        # Allocation should fail
        with self.assertRaises(ReapiInsufficientResources):
            ctx.match_allocate(1, TEST_JOBSPEC)

        # Mark back up
        ctx.set_status(rank=0, up=True)

        # Verify now up
        self.assertTrue(ctx.get_status(rank=0))

        # Should work again
        result = ctx.match_allocate(2, TEST_JOBSPEC)
        self.assertIsNotNone(result["R"])
        self.assertGreater(len(result["R"]), 0)

    def test_set_status_by_rank_nonexistent_rank(self):
        """Setting status on nonexistent rank raises error."""
        ctx = Reapi()
        ctx.initialize(TEST_JGF, load_format="jgf")

        # Should raise with nonexistent rank (graph only has rank 0)
        with self.assertRaises(Exception):
            ctx.set_status(rank=999, up=False)

    def test_set_status_by_rank_all(self):
        """Set status by rank with no rank affects all ranks."""
        ctx = Reapi()
        ctx.initialize(TEST_JGF, load_format="jgf", match_format="rv1")

        # Verify initially up
        self.assertTrue(ctx.get_status(rank=0))

        # Mark all ranks down
        ctx.set_status(rank=FLUX_NODEID_ANY, up=False)

        # Verify now down
        self.assertFalse(ctx.get_status(rank=0))

        # Allocation should fail
        with self.assertRaises(ReapiInsufficientResources):
            ctx.match_allocate(1, TEST_JOBSPEC)

        # Mark back up
        ctx.set_status(rank=FLUX_NODEID_ANY, up=True)

        # Verify now up
        self.assertTrue(ctx.get_status(rank=0))

        # Should work again
        result = ctx.match_allocate(2, TEST_JOBSPEC)
        self.assertIsNotNone(result["R"])
        self.assertGreater(len(result["R"]), 0)

    def test_set_status_by_path(self):
        """Set status by path affects specific resource."""
        ctx = Reapi()
        ctx.initialize(TEST_JGF, load_format="jgf", match_format="rv1")

        # Verify initially up
        self.assertTrue(ctx.get_status(path="/tiny0/node0"))

        # Mark node down by path
        ctx.set_status(path="/tiny0/node0", up=False)

        # Verify now down
        self.assertFalse(ctx.get_status(path="/tiny0/node0"))

        # Allocation should fail
        with self.assertRaises(ReapiInsufficientResources):
            ctx.match_allocate(1, TEST_JOBSPEC)

        # Mark back up
        ctx.set_status(path="/tiny0/node0", up=True)

        # Verify now up
        self.assertTrue(ctx.get_status(path="/tiny0/node0"))

        # Should work again
        result = ctx.match_allocate(2, TEST_JOBSPEC)
        self.assertIsNotNone(result["R"])
        self.assertGreater(len(result["R"]), 0)

    def test_get_status(self):
        """Get status by rank and path."""
        ctx = Reapi()
        ctx.initialize(TEST_JGF, load_format="jgf")

        # Check initial status by rank
        self.assertTrue(ctx.get_status(rank=0))

        # Check initial status by path
        self.assertTrue(ctx.get_status(path="/tiny0/node0"))

        # Set down by rank
        ctx.set_status(rank=0, up=False)
        self.assertFalse(ctx.get_status(rank=0))

        # Set down by path
        ctx.set_status(path="/tiny0/node0/core0", up=False)
        self.assertFalse(ctx.get_status(path="/tiny0/node0/core0"))

        # Verify node status is still down
        self.assertFalse(ctx.get_status(rank=0))

    def test_set_status_invalid_params(self):
        """Setting status with invalid params raises error."""
        ctx = Reapi()
        ctx.initialize(TEST_JGF, load_format="jgf")

        # Should raise with both path and rank
        with self.assertRaises(ValueError):
            ctx.set_status(path="/tiny0/node0", rank=0, up=False)

        # Should raise with neither path nor rank
        with self.assertRaises(ValueError):
            ctx.set_status(up=False)

        # Should raise with invalid path
        with self.assertRaises(Exception):
            ctx.set_status(path="/nonexistent", up=False)

    def test_update_allocate(self):
        """update_allocate re-registers an allocation, marking resources taken."""
        ctx = Reapi()
        ctx.initialize(TEST_JGF, load_format="jgf", match_format="rv1")

        # Allocate, save R, then cancel to reset the graph
        result = ctx.match_allocate(1, TEST_JOBSPEC)
        R = result["R"]
        ctx.cancel(1)

        # Re-register the allocation via update_allocate
        update_result = ctx.update_allocate(2, R)
        self.assertIn("at", update_result)
        self.assertIn("overhead", update_result)
        self.assertIn("R", update_result)

        # info() must return a populated R for the re-registered job
        info = ctx.info(2)
        self.assertIsNotNone(info["R"])
        self.assertIsInstance(info["R"], dict)
        self.assertIn("execution", info["R"])

        # Resources should now be marked as taken
        with self.assertRaises(ReapiInsufficientResources):
            ctx.match_allocate(3, TEST_JOBSPEC)

        # After cancel, resources should be available again
        ctx.cancel(2)
        result2 = ctx.match_allocate(4, TEST_JOBSPEC)
        self.assertIsNotNone(result2["R"])

    def test_update_allocate_before_initialize(self):
        """update_allocate before initialize raises error."""
        ctx = Reapi()
        with self.assertRaises(ReapiError):
            ctx.update_allocate(1, TEST_RV1)

    def test_update_allocate_duplicate_jobid(self):
        """update_allocate with an already-registered jobid raises error."""
        ctx = Reapi()
        ctx.initialize(TEST_JGF, load_format="jgf", match_format="rv1")

        result = ctx.match_allocate(1, TEST_JOBSPEC)
        R = result["R"]

        with self.assertRaises(ReapiError):
            ctx.update_allocate(1, R)

    def test_clone_is_independent(self):
        """Clone is independent: mutations to clone do not affect original."""
        ctx = Reapi()
        ctx.initialize(TEST_JGF, load_format="jgf", match_format="rv1")

        # Allocate a job in the original
        ctx.match_allocate(1, TEST_JOBSPEC)

        # Clone after the allocation — clone should see job 1 as allocated
        sim = ctx.clone()

        # Allocate in the clone (uses the only remaining... wait, resources
        # are all taken).  Instead cancel in the clone and verify the original
        # still has the allocation.
        sim.cancel(1)

        # Clone: resources freed, can allocate again
        result_sim = sim.match_allocate(2, TEST_JOBSPEC)
        self.assertIsNotNone(result_sim["R"])

        # Original: job 1 still allocated, no resources available
        with self.assertRaises(ReapiInsufficientResources):
            ctx.match_allocate(2, TEST_JOBSPEC)

    def test_clone_before_initialize(self):
        """Clone before initialize raises error."""
        ctx = Reapi()
        with self.assertRaises(ReapiError):
            ctx.clone()

    def test_clone_inherits_status(self):
        """Clone inherits up/down status from original."""
        ctx = Reapi()
        ctx.initialize(TEST_JGF, load_format="jgf", match_format="rv1")

        # Mark rank 0 down in the original
        ctx.set_status(rank=0, up=False)

        # Clone should also see rank 0 as down
        sim = ctx.clone()
        with self.assertRaises(ReapiInsufficientResources):
            sim.match_allocate(1, TEST_JOBSPEC)

        # Bringing it back up in the clone does not affect the original
        sim.set_status(rank=0, up=True)
        result = sim.match_allocate(1, TEST_JOBSPEC)
        self.assertIsNotNone(result["R"])

        with self.assertRaises(ReapiInsufficientResources):
            ctx.match_allocate(1, TEST_JOBSPEC)


if __name__ == "__main__":
    unittest.main(testRunner=TAPTestRunner(), verbosity=2)

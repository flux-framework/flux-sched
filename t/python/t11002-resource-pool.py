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

"""Test FluxionJGFPool basic functionality (creation, serialization, etc)."""

import json
import unittest

try:
    from fluxion.pool import FluxionJGFPool
    from fluxion.pool.rv1exec import FluxionRv1ExecPool
    from fluxion.resourcegraph.V1 import FluxionResourceGraphV1

    FLUXION_AVAILABLE = True
except (ImportError, OSError) as e:
    FLUXION_AVAILABLE = False
    SKIP_REASON = str(e)

from pycotap import TAPTestRunner


def make_test_R_with_jgf():
    """Create test R with JGF in scheduling key."""
    # Start with basic R
    R_basic = {
        "version": 1,
        "execution": {
            "R_lite": [{"rank": "0", "children": {"core": "0-3"}}],
            "starttime": 0,
            "expiration": 0,
            "nodelist": ["node0"],
        },
    }

    # Convert to JGF
    if FLUXION_AVAILABLE:
        try:
            converter = FluxionResourceGraphV1(R_basic)
            jgf = converter.to_JSON()
            R_basic["scheduling"] = jgf

            # Validate JGF was actually created
            if not jgf or "graph" not in jgf:
                raise ValueError(f"FluxionResourceGraphV1 did not produce valid JGF: {jgf}")
            if "nodes" not in jgf["graph"]:
                raise ValueError(f"JGF missing nodes: {jgf}")

            print(f"Created test R with {len(jgf['graph']['nodes'])} JGF nodes")
        except Exception as e:
            print(f"ERROR creating test JGF: {e}")
            import traceback
            traceback.print_exc()
            raise

    return R_basic


# Test R - simple single-node setup with JGF
TEST_R_BASIC = make_test_R_with_jgf() if FLUXION_AVAILABLE else {}

# Test jobspec - request 1 core (with slot wrapper like t11001)
TEST_JOBSPEC_1CORE = {
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

# Test jobspec - request 2 cores (with slot wrapper)
TEST_JOBSPEC_2CORES = {
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
                    "with": [{"type": "core", "count": 2}],
                }
            ],
        }
    ],
    "tasks": [{"command": ["sleep", "0"], "slot": "task", "count": {"per_slot": 1}}],
    "attributes": {"system": {"duration": 60.0}},
}

# Test jobspec - infeasible request (too many cores, with slot wrapper)
TEST_JOBSPEC_INFEASIBLE = {
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
                    "with": [{"type": "core", "count": 100}],
                }
            ],
        }
    ],
    "tasks": [{"command": ["sleep", "0"], "slot": "task", "count": {"per_slot": 1}}],
    "attributes": {"system": {"duration": 60.0}},
}

# Test jobspec - 2 nodes with 1 core each
TEST_JOBSPEC_2NODES = {
    "version": 1,
    "resources": [
        {
            "type": "node",
            "count": 2,
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

# Test R - 2 nodes with 2 cores each (for partial release tests)
TEST_R_MULTINODE = {
    "version": 1,
    "execution": {
        "R_lite": [{"rank": "0-1", "children": {"core": "0-1"}}],
        "starttime": 0,
        "expiration": 0,
        "nodelist": ["node0", "node1"],
    },
}

# Create JGF version for JGF pool tests
if FLUXION_AVAILABLE:
    try:
        converter = FluxionResourceGraphV1(TEST_R_MULTINODE)
        jgf = converter.to_JSON()
        TEST_R_MULTINODE_JGF = dict(TEST_R_MULTINODE)
        TEST_R_MULTINODE_JGF["scheduling"] = jgf
    except Exception:
        TEST_R_MULTINODE_JGF = None
else:
    TEST_R_MULTINODE_JGF = None


@unittest.skipUnless(
    FLUXION_AVAILABLE,
    f"Fluxion not available: {SKIP_REASON if not FLUXION_AVAILABLE else ''}",
)
class TestFluxionJGFPoolBasic(unittest.TestCase):
    """Test basic FluxionJGFPool functionality."""

    def setUp(self):
        """Validate test data before each test."""
        # Verify TEST_R_BASIC has JGF
        self.assertIn("scheduling", TEST_R_BASIC, "TEST_R_BASIC missing scheduling key")
        self.assertIn("graph", TEST_R_BASIC["scheduling"], "TEST_R_BASIC missing JGF graph")
        self.assertIn("nodes", TEST_R_BASIC["scheduling"]["graph"], "JGF missing nodes")
        self.assertGreater(len(TEST_R_BASIC["scheduling"]["graph"]["nodes"]), 0, "JGF has no nodes")

    def test_pool_creation_from_dict(self):
        """FluxionJGFPool can be created from R dict."""
        pool = FluxionJGFPool(TEST_R_BASIC)
        self.assertIsNotNone(pool)
        self.assertEqual(pool.version, 1)

    def test_pool_creation_from_json(self):
        """FluxionJGFPool can be created from R JSON string."""
        r_str = json.dumps(TEST_R_BASIC)
        pool = FluxionJGFPool(r_str)
        self.assertIsNotNone(pool)

    def test_pool_generation_counter(self):
        """Generation counter increments on mutations."""
        pool = FluxionJGFPool(TEST_R_BASIC)
        initial_gen = pool.generation

        pool.mark_down("0")
        self.assertGreater(pool.generation, initial_gen)

    def test_pool_to_dict(self):
        """Pool can be serialized to dict."""
        pool = FluxionJGFPool(TEST_R_BASIC)
        r_dict = pool.to_dict()
        self.assertIsInstance(r_dict, dict)
        self.assertIn("execution", r_dict)
        self.assertEqual(r_dict["version"], 1)

    def test_pool_dumps(self):
        """Pool has human-readable string representation."""
        pool = FluxionJGFPool(TEST_R_BASIC)
        summary = pool.dumps()
        self.assertIsInstance(summary, str)
        self.assertIn("FluxionJGFPool", summary)

    def test_expiration_get_set(self):
        """Can get and set pool expiration."""
        pool = FluxionJGFPool(TEST_R_BASIC)

        pool.set_expiration(12345.0)
        self.assertEqual(pool.get_expiration(), 12345.0)

    def test_copy_works(self):
        """copy() creates independent pool."""
        pool = FluxionJGFPool(TEST_R_BASIC)
        copy_pool = pool.copy()
        self.assertIsNotNone(copy_pool)
        # Verify independence by allocating in copy
        jobid = 1
        request = pool.parse_resource_request(TEST_JOBSPEC_1CORE)
        copy_pool.alloc(jobid, request)
        # Original pool should still be empty
        self.assertEqual(len(pool._end_times), 0)
        # Copy should have the allocation
        self.assertEqual(len(copy_pool._end_times), 1)

    def test_parse_resource_request(self):
        """Can parse jobspec into request object."""
        pool = FluxionJGFPool(TEST_R_BASIC)
        request = pool.parse_resource_request(TEST_JOBSPEC_1CORE)

        self.assertIsNotNone(request)
        self.assertTrue(hasattr(request, "jobspec"))
        self.assertTrue(hasattr(request, "duration"))
        self.assertEqual(request.duration, 60.0)

    def test_check_feasibility_success(self):
        """check_feasibility succeeds for feasible request."""
        pool = FluxionJGFPool(TEST_R_BASIC)
        request = pool.parse_resource_request(TEST_JOBSPEC_1CORE)

        # Should not raise
        pool.check_feasibility(request)

    def test_check_feasibility_failure(self):
        """check_feasibility raises for infeasible request."""
        from flux.resource.ResourcePoolImplementation import InfeasibleRequest

        pool = FluxionJGFPool(TEST_R_BASIC)
        request = pool.parse_resource_request(TEST_JOBSPEC_INFEASIBLE)

        with self.assertRaises(InfeasibleRequest):
            pool.check_feasibility(request)

    def test_mark_down_up(self):
        """Can mark resources down and up."""
        pool = FluxionJGFPool(TEST_R_BASIC)

        # Mark down
        pool.mark_down("0")
        down_set = pool.copy_down()
        self.assertEqual(down_set.nnodes(), 1)

        # Mark up
        pool.mark_up("0")
        down_set = pool.copy_down()
        self.assertEqual(down_set.nnodes(), 0)

    def test_alloc_and_free(self):
        """Can allocate and free resources."""
        from flux.resource.ResourcePoolImplementation import InsufficientResources

        pool = FluxionJGFPool(TEST_R_BASIC)
        request = pool.parse_resource_request(TEST_JOBSPEC_1CORE)

        # Allocate for job 1
        try:
            R1 = pool.alloc(1, request)
        except Exception as e:
            # Print debug info on failure
            print(f"\nAllocation failed: {e}")
            print(f"Pool stats: {pool.dumps()}")
            print(f"Request: duration={request.duration}, jobspec keys={request.jobspec.keys()}")
            raise

        self.assertIsNotNone(R1)
        self.assertEqual(R1.nnodes(), 1)

        # Try to allocate 4 more cores (should fail - only have 4 total)
        request2 = pool.parse_resource_request(TEST_JOBSPEC_2CORES)
        # First allocation took 1 core, we have 3 left
        R2 = pool.alloc(2, request2)  # Should succeed, takes 2 cores, 1 left
        self.assertIsNotNone(R2)

        # Now try to allocate 2 more (should fail - only 1 left)
        with self.assertRaises(InsufficientResources):
            pool.alloc(3, request2)

        # Free first allocation
        pool.free(1)

        # Now we should be able to allocate again
        R3 = pool.alloc(3, request2)
        self.assertIsNotNone(R3)

    def test_copy_allocated(self):
        """copy_allocated returns allocated resources."""
        pool = FluxionJGFPool(TEST_R_BASIC)
        request = pool.parse_resource_request(TEST_JOBSPEC_1CORE)

        # No allocations yet
        allocated = pool.copy_allocated()
        self.assertEqual(allocated.nnodes(), 0)

        # Allocate
        pool.alloc(1, request)

        # Should have allocation now
        allocated = pool.copy_allocated()
        self.assertEqual(allocated.nnodes(), 1)

    def test_job_end_times(self):
        """job_end_times returns tracked allocations."""
        pool = FluxionJGFPool(TEST_R_BASIC)
        request = pool.parse_resource_request(TEST_JOBSPEC_1CORE)

        # No jobs yet
        end_times = pool.job_end_times()
        self.assertEqual(len(end_times), 0)

        # Allocate with duration
        pool.alloc(1, request)
        end_times = pool.job_end_times()
        self.assertEqual(len(end_times), 1)
        jobid, exp = end_times[0]
        self.assertEqual(jobid, 1)

    def test_update_expiration(self):
        """update_expiration modifies tracked job."""
        pool = FluxionJGFPool(TEST_R_BASIC)
        request = pool.parse_resource_request(TEST_JOBSPEC_1CORE)

        pool.alloc(1, request)

        # Update expiration
        pool.update_expiration(1, 999.0)

        # Verify
        end_times = pool.job_end_times()
        self.assertEqual(len(end_times), 1)
        jobid, exp = end_times[0]
        self.assertEqual(exp, 999.0)

    def test_sequential_alloc_free(self):
        """Sequential allocations work after free (regression test)."""
        pool = FluxionJGFPool(TEST_R_BASIC)
        request = pool.parse_resource_request(TEST_JOBSPEC_1CORE)

        # Job 1: alloc then free
        R1 = pool.alloc(1, request)
        self.assertIsNotNone(R1)
        pool.free(1)

        # Job 2: should succeed
        R2 = pool.alloc(2, request)
        self.assertIsNotNone(R2)
        pool.free(2)

        # Job 3: should also succeed
        R3 = pool.alloc(3, request)
        self.assertIsNotNone(R3)

    def test_concurrent_alloc_free(self):
        """Concurrent allocations and frees work (regression test)."""
        pool = FluxionJGFPool(TEST_R_BASIC)
        request = pool.parse_resource_request(TEST_JOBSPEC_1CORE)

        # Allocate all 4 cores
        pool.alloc(1, request)
        pool.alloc(2, request)
        pool.alloc(3, request)
        pool.alloc(4, request)

        # Free one
        pool.free(1)

        # Should be able to allocate again
        R5 = pool.alloc(5, request)
        self.assertIsNotNone(R5)

    def test_rv1exec_sequential_alloc_free(self):
        """FluxionRv1ExecPool sequential alloc/free works."""
        pool = FluxionRv1ExecPool(TEST_R_MULTINODE)
        request = pool.parse_resource_request(TEST_JOBSPEC_1CORE)

        # Job 1: alloc then free
        R1 = pool.alloc(1, request)
        self.assertIsNotNone(R1)
        pool.free(1)

        # Job 2: should succeed
        R2 = pool.alloc(2, request)
        self.assertIsNotNone(R2)

    def test_rv1exec_partial_release(self):
        """FluxionRv1ExecPool partial release at node granularity."""
        from flux.resource.Rv1Set import Rv1Set

        pool = FluxionRv1ExecPool(TEST_R_MULTINODE)
        request = pool.parse_resource_request(TEST_JOBSPEC_2NODES)

        # Allocate 2 nodes
        R_full = pool.alloc(1, request)
        self.assertEqual(R_full.nnodes(), 2)

        # Create R for partial release (1 node)
        R_partial = {
            "version": 1,
            "execution": {
                "R_lite": [{"rank": "0", "children": {"core": "0-1"}}],
                "starttime": 0,
                "expiration": 0,
                "nodelist": ["node0"],
            },
        }

        # Partial free - release rank 0, keep rank 1
        pool.free(1, R=Rv1Set(R_partial), final=False)

        # Job should still be tracked
        self.assertEqual(len(pool._end_times), 1)

        # Final free - release remaining resources
        pool.free(1, final=True)

        # Job should be removed now
        self.assertEqual(len(pool._end_times), 0)

        # Should be able to allocate again
        R_new = pool.alloc(2, request)
        self.assertIsNotNone(R_new)

    def test_jgf_partial_release(self):
        """FluxionJGFPool partial release at node granularity."""
        from flux.resource.Rv1Set import Rv1Set

        if TEST_R_MULTINODE_JGF is None:
            self.skipTest("Could not create multinode JGF")

        pool = FluxionJGFPool(TEST_R_MULTINODE_JGF)
        request = pool.parse_resource_request(TEST_JOBSPEC_2NODES)

        # Allocate 2 nodes
        R_full = pool.alloc(1, request)
        self.assertEqual(R_full.nnodes(), 2)

        # Create R for partial release (1 node)
        R_partial = {
            "version": 1,
            "execution": {
                "R_lite": [{"rank": "0", "children": {"core": "0-1"}}],
                "starttime": 0,
                "expiration": 0,
                "nodelist": ["node0"],
            },
        }

        # Partial free - release rank 0, keep rank 1
        pool.free(1, R=Rv1Set(R_partial), final=False)

        # Job should still be tracked
        self.assertEqual(len(pool._end_times), 1)

        # Final free - release remaining resources
        pool.free(1, final=True)

        # Job should be removed now
        self.assertEqual(len(pool._end_times), 0)

        # Should be able to allocate again
        R_new = pool.alloc(2, request)
        self.assertIsNotNone(R_new)


if __name__ == "__main__":
    unittest.main(testRunner=TAPTestRunner())

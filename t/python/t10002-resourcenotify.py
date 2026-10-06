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

import flux
import flux.testing.fake_resources
import unittest
import os
import subprocess

from pycotap import TAPTestRunner


class TestResourceNotify(unittest.TestCase):
    """Test the sched-fluxion-resource.notify RPC."""

    notify_request_no = 0

    # Activated with `reactor_run()` after receiving a response
    def notify_up_down_cb(self, fut, handle):
        self.notify_request_no += 1
        print(f"notification number: {self.notify_request_no}")

        result = fut.get()

        if self.notify_request_no == 1:
            self.assertTrue("resources" in result)
            self.assertTrue("up" in result)
            self.assertFalse("down" in result)  # All nodes start up
            self.assertFalse("shrink" in result)
            self.assertTrue("expiration" in result)
            print(f"got UP: {result['up']}")
        elif self.notify_request_no == 2:
            self.assertFalse("resources" in result)
            self.assertFalse("up" in result)
            self.assertTrue("down" in result)
            self.assertFalse("shrink" in result)
            self.assertFalse("expiration" in result)
            self.assertEqual(result["down"], "1")
            print(f"got DOWN: {result['down']}")
        elif self.notify_request_no == 3:
            self.assertFalse("resources" in result)
            self.assertTrue("up" in result)
            self.assertFalse("down" in result)
            self.assertFalse("shrink" in result)
            self.assertFalse("expiration" in result)
            self.assertEqual(result["up"], "1")
            print(f"got UP: {result['up']}")
        else:
            self.assertTrue(False)

        fut.reset()
        handle.reactor_stop()

    def test_notify_up_down(self):
        handle = flux.Flux()
        self.notify_request_no = 0

        # Initialize resource update notifications
        handle.rpc(
            "sched-fluxion-resource.notify",
            {
                "requested": {
                    "resources": True,
                    "up": True,
                    "down": True,
                    "shrink": True,
                    "expiration": True,
                }
            },
            flags=flux.constants.FLUX_RPC_STREAMING,
        ).then(self.notify_up_down_cb, handle)
        handle.reactor_run()  # 1: Receive initial resources, up (all) hosts, and expiration

        # Force a host down
        payload = {"resource_path": "/cluster0/fake1", "status": "down"}
        handle.rpc("sched-fluxion-resource.set_status", payload).get()
        handle.reactor_run()  # 2: Receive newly down rank 1

        # Force a host down
        payload = {"resource_path": "/cluster0/fake1", "status": "up"}
        handle.rpc("sched-fluxion-resource.set_status", payload).get()
        handle.reactor_run()  # 3: Receive newly up rank 1

        self.assertEqual(self.notify_request_no, 3)

    def test_notify_no_resource(self):
        handle = flux.Flux()

        # Initialize resource update notifications
        initial_resources = handle.rpc(
            "sched-fluxion-resource.notify",
            {
                "requested": {
                    "resources": False,
                    "up": True,
                    "down": True,
                    "shrink": True,
                    "expiration": True,
                }
            },
            flags=flux.constants.FLUX_RPC_STREAMING,
        ).get()

        # Make sure we don't transmit the resources if we don't ask for them
        self.assertFalse("resources" in initial_resources)
        self.assertTrue("up" in initial_resources)
        self.assertFalse("down" in initial_resources)
        self.assertFalse("shrink" in initial_resources)
        self.assertTrue("expiration" in initial_resources)


if __name__ == "__main__":
    from subflux import rerun_under_flux

    os.environ["FLUX_SCHED_MODULE"] = "none"
    if rerun_under_flux("--conf=fake-resources.nnodes=2"):
        subprocess.run(["flux", "module", "load", "sched-fluxion-resource"])
        subprocess.run(["flux", "module", "load", "sched-fluxion-qmanager"])
        subprocess.run(["flux", "module", "load", "sched-fluxion-feasibility"])
        unittest.main(testRunner=TAPTestRunner())

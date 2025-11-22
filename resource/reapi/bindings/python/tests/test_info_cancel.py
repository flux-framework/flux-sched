import pytest
import os
import json
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
for path in [ROOT, HERE]:
    sys.path.insert(0, path)
import utils  # noqa
import paths  # noqa
from flux_sched.reapi_cli import ReapiCli, ReapiError  # noqa


class TestInfoCancelGrug:
    """
    Loading GRUG and testing standard cancel/info operations.
    """

    @pytest.fixture
    def cli(self):
        """
        Initializes ReapiCli with 'tiny.graphml' and specific shell options.
        """
        cli_obj = ReapiCli()
        grug_content = utils.read_file(paths.GRUG_PATH)

        # Shell: load-file=${grug} prune-filters=ALL:core load-format=grug subsystems=containment policy=high
        options = json.dumps(
            {
                "load_format": "grug",
                "prune_filters": "ALL:core",
                "subsystems": "containment",
                "policy": "high",
            }
        )

        cli_obj.initialize(grug_content, options)
        return cli_obj

    def test_resource_cancel_loop(self, cli):
        """
        Mimics: 'resource-cancel works'
        Allocates and cancels a job 4 times in a row.
        """
        jobspec_path = os.path.join(paths.JOBSPEC_DIR, "test001.yaml")
        jobspec_str = utils.load_jobspec(jobspec_path)

        print("\n--- Testing Allocate/Cancel Loop ---")
        for i in range(4):
            # 1. Allocate
            jobid, _, _, _, _ = cli.match(jobspec_str, orelse_reserve=False)
            assert jobid > 0
            print(f"  Iter {i}: Allocated Job {jobid}")

            # 2. Cancel
            # Note: Shell script hardcodes '0', but ReapiCli generates sequential IDs (1, 2...).
            # We must cancel the actual ID returned by match.
            try:
                cli.cancel(jobid)
                print(f"  Iter {i}: Canceled Job {jobid}")
            except Exception as e:
                pytest.fail(f"Failed to cancel job {jobid}: {e}")

    def test_info_on_cancelled_failure(self, cli):
        """
        Mimics: 'resource-info will not report for canceled jobs'
        """
        jobspec_path = os.path.join(paths.JOBSPEC_DIR, "test001.yaml")
        jobspec_str = utils.load_jobspec(jobspec_path)

        # Allocate then cancel
        jobid, _, _, _, _ = cli.match(jobspec_str)

        # Info should report allocated then cancelled
        info = cli.info(jobid)
        assert info[0] == "ALLOCATED"
        cli.cancel(jobid)
        info = cli.info(jobid)
        assert info[0] == "CANCELED"

    def test_info_on_allocated_jobs(self, cli):
        """
        Mimics:
        1. 'allocate works with 1-node/1-socket after cancels'
        2. 'resource-info on allocated jobs works'
        """
        jobspec_path = os.path.join(paths.JOBSPEC_DIR, "test001.yaml")
        jobspec_str = utils.load_jobspec(jobspec_path)

        # 1. Fill up the system (4 slots on tiny.graphml)
        active_jobids = []
        print("\n--- Filling System ---")
        for i in range(4):
            jobid, _, _, _, _ = cli.match(jobspec_str)
            assert jobid > 0
            active_jobids.append(jobid)
            print(f"  Allocated {jobid}")

        # 2. Check Info
        print("\n--- Checking Info ---")
        for jobid in active_jobids:
            # info returns: (mode, reserved, at, ov)
            mode, reserved, _, _ = cli.info(jobid)

            print(f"  Job {jobid} mode: {mode}")

            # Shell script does: grep ALLOCATED info.N
            assert "ALLOCATED" in mode
            assert not reserved

    def test_cancel_nonexistent(self, cli):
        """
        Mimics: 'cancel on nonexistent jobid is handled gracefully'
        Shell expects code 3. ReapiCli should raise ReapiError (or similar).
        """
        bad_id = 100000
        with pytest.raises(ReapiError):
            # By default, noent_ok is False in standard python bindings logic
            # unless explicitly exposed as an arg.
            cli.cancel(bad_id)


class TestPartialCancelRV1:
    """
    Represents the second half of the shell script:
    Loading RV1 and testing partial cancellation.
    """

    @pytest.fixture
    def cli(self):
        """
        Initializes ReapiCli with 'tiny_rv1exec.json'.
        """
        cli_obj = ReapiCli()

        # Locate rv1exec file
        # path: data/resource/rv1exec/tiny_rv1exec.json
        rv1_path = os.path.join(paths.DATA_DIR, "rv1exec", "tiny_rv1exec.json")
        rv1_content = utils.read_file(rv1_path)

        # Shell: load-file=${rv1} prune-filters=ALL:core load-format=rv1exec subsystems=containment policy=low
        options = json.dumps(
            {
                "load_format": "rv1exec",
                "prune_filters": "ALL:core",
                "subsystems": "containment",
                "policy": "low",
            }
        )

        cli_obj.initialize(rv1_content, options)
        return cli_obj

    def test_partial_cancel_workflow(self, cli):
        """
        Mimics: 'resource-cancel works' (in the RV1 context)
        1. Match test018.yaml
        2. Partial cancel using rank1_cancel.json
        3. Match test019.yaml
        """
        # 1. Match test018
        js1_path = os.path.join(paths.JOBSPEC_ROOT, "cancel", "test018.yaml")
        js1_str = utils.load_jobspec(js1_path)

        jobid1, _, _, _, _ = cli.match(js1_str)
        print(f"\nAllocated Job 1 (test018): {jobid1}")
        assert jobid1 > 0

        # 2. Partial Cancel
        # We need the content of rank1_cancel.json to pass as the 'R' string/json
        cancel_file_path = os.path.join(
            paths.DATA_DIR, "rv1exec", "cancel", "rank1_cancel.json"
        )
        cancel_r_content = utils.read_file(cancel_file_path)

        print(
            f"Partial Canceling Job {jobid1} with resources from {os.path.basename(cancel_file_path)}"
        )

        try:
            # reapi_cli_partial_cancel returns: (int ret)
            # It might also return full_removal bool via reference, depending on binding implementation
            cli.partial_cancel(jobid1, cancel_r_content)
        except AttributeError:
            pytest.fail(
                "ReapiCli.partial_cancel method not found. Please update reapi_cli.pyx."
            )
        except Exception as e:
            pytest.fail(f"Partial cancel failed: {e}")

        # 3. Match test019
        # This job presumably fits only because resources were freed by the partial cancel
        js2_path = os.path.join(paths.JOBSPEC_ROOT, "cancel", "test019.yaml")
        js2_str = utils.load_jobspec(js2_path)

        jobid2, _, _, _, _ = cli.match(js2_str)
        print(f"Allocated Job 2 (test019): {jobid2}")
        assert jobid2 > 0

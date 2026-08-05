import pytest
import os
import json
import yaml
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
for path in [ROOT, HERE]:
    sys.path.insert(0, path)
import utils  # noqa
import paths  # noqa
from flux_sched.reapi_cli import ReapiCli, ReapiError  # noqa


class TestMatchAllocate:
    @pytest.fixture
    def cli(self):
        """
        Initializes ReapiCli with the 'tiny.graphml' GRUG file
        and the options specified in the shell script.
        """
        cli_obj = ReapiCli()

        # 1. Read the GRUG file content
        grug_content = utils.read_file(paths.GRUG_PATH)

        # 2. Mimic shell options:
        # load-file=${grug} (handled via first arg)
        # prune-filters=ALL:core
        # load-format=grug
        # subsystems=containment
        # policy=high
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

    def test_allocation_saturation(self, cli):
        """
        Mimics:
        1. 'match-allocate works with a 1-node, 1-socket jobspec' (4 times)
        2. 'match-allocate fails when all resources are allocated' (4 times)
        """
        jobspec_path = os.path.join(paths.JOBSPEC_DIR, "test001.yaml")
        jobspec_str = utils.load_jobspec(jobspec_path)

        print("\n--- Phase 1: Allocating 4 jobs (Success expected) ---")
        for i in range(1, 5):
            # Should succeed
            jobid, reserved, R, _, _ = cli.match(jobspec_str, orelse_reserve=False)
            print(f"  Job {i}: Allocated JobID {jobid}")
            assert jobid > 0
            assert reserved is False

        print("\n--- Phase 2: Allocating 4 more jobs (Failure expected) ---")
        # The machine is tiny; 4 jobs should not go in after the first 4
        for i in range(5, 9):
            _, reserved, allocated, _, _ = cli.match(jobspec_str, orelse_reserve=False)
            assert not allocated and not reserved
            print(f"  Job {i}: Match Allocate failed as expected")

    def test_malformed_jobspec(self, cli):
        """
        Mimics: 'handling of a malformed jobspec works'
        Input: bad.yaml
        """
        path = os.path.join(paths.JOBSPEC_DIR, "bad.yaml")

        # We pass the raw content because yaml.safe_load might fail if it's truly malformed YAML,
        # but here we want to test how the C++ binding handles the bad input.
        content = utils.read_file(path)

        # If valid YAML but bad schema, pass as JSON. If invalid YAML, pass raw.
        try:
            js = yaml.safe_load(content)
            content = json.dumps(js)
        except Exception:
            pass  # Pass raw content if it's not valid YAML

        with pytest.raises(ReapiError):
            cli.match(content)

    def test_invalid_resource_type(self, cli):
        """
        Mimics: 'handling of an invalid resource type works'
        Input: bad_res_type.yaml
        """
        path = os.path.join(paths.JOBSPEC_DIR, "bad_res_type.yaml")
        jobspec_str = utils.load_jobspec(path)

        with pytest.raises(ReapiError) as excinfo:
            cli.match(jobspec_str)
        print(f"Caught expected error: {excinfo.value}")

    def test_non_existent_jobspec_equivalent(self, cli):
        """
        Mimics: 'detecting of a non-existent jobspec file works'

        Note: The C++ binding accepts a string content, not a filename.
        So we can't test 'file not found'. Instead, we test passing
        garbage non-JSON content, which is effectively what happens if
        one were to pass a filename string instead of file content.
        """
        garbage_input = "this is not a json string"

        with pytest.raises(ReapiError):
            cli.match(garbage_input)

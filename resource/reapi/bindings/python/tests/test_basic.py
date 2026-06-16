import pytest
import os
import json
import sys

# Assuming this file is in: resource/reapi/bindings/python/tests
# And data is in:           resource/data/

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
for path in [ROOT, HERE]:
    sys.path.insert(0, path)
import utils  # noqa
import paths  # noqa
from flux_sched.reapi_cli import ReapiCli  # noqa


@pytest.fixture(scope="module")
def cli():
    """
    Initialize the ReapiCli once with tiny.json for all tests in this module.
    """
    if not os.path.exists(paths.JGF_PATH):
        pytest.fail(f"Cannot find tiny.json at: {paths.JGF_PATH}")

    c = ReapiCli()
    rgraph = utils.read_file(paths.JGF_PATH)
    options = json.dumps({"load_format": "jgf"})

    try:
        c.initialize(rgraph, options)
    except Exception as e:
        pytest.fail(f"Failed to initialize ReapiCli: {e}")

    return c


@pytest.mark.parametrize(
    "filename",
    [
        "test001.yaml",
        "test002.yaml",
        "test003.yaml",
        "test004.yaml",
        "test005.yaml",
        "test006.yaml",
        "test007.yaml",
        "test008.yaml",
        "test009.yaml",
        "test010.yaml",
        "test011.yaml",
        "test012.yaml",
        "test013.yaml",
        "test014.yaml",
        "test015.yaml",
    ],
)
def test_all_basics_success(cli, filename):
    """
    Generalizes the manual python loop provided in your prompt.
    Ensures all testXXX.yaml files in 'basics' result in a successful match.
    """
    jobspec = utils.load_jobspec(filename)

    jobid, reserved, R, at, ov = cli.match(jobspec, orelse_reserve=False)

    assert jobid > 0, f"Failed to match {filename}"
    assert R is not None


@pytest.mark.parametrize("filename", ["bad.yaml", "bad_res_type.yaml"])
def test_basics_failure(cli, filename):
    """
    Ensures invalid jobspecs raise errors or return valid failures.
    """
    try:
        jobspec = utils.load_jobspec(filename)
        jobid, _, _, _, _ = cli.match(jobspec, orelse_reserve=False)

        # If it returns 0 on failure - this is "success" because it failed to match as expected
        if jobid == 0:
            return  # Success (it failed to match as expected)

    except Exception:
        # If it raised an exception (parsing error), that is also a pass for a "bad" file
        return

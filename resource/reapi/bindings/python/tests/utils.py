import os
import yaml
import sys
import json

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)


def read_file(path):
    """
    Read path from filename
    """
    with open(path, "r") as f:
        return f.read()


def load_jobspec(filename):
    """
    Load a YAML jobspec from file, jump as json.

    We don't technically need to json load, but this
    helps to ensure it is valid json first.
    """
    import paths

    path = os.path.join(paths.JOBSPEC_DIR, filename)
    with open(path, "r") as f:
        data = yaml.safe_load(f)
    return json.dumps(data)

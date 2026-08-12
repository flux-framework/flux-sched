import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

ROOT_DIR = HERE
for _ in range(5):
    ROOT_DIR = os.path.dirname(ROOT_DIR)

DATA_DIR = os.path.join(ROOT_DIR, "t", "data", "resource")
JGF_PATH = os.path.join(DATA_DIR, "jgfs", "tiny.json")
GRUG_PATH = os.path.join(DATA_DIR, "grugs", "tiny.graphml")
JOBSPEC_ROOT = os.path.join(DATA_DIR, "jobspecs")
JOBSPEC_DIR = os.path.join(JOBSPEC_ROOT, "basics")
JOBSPEC_DURATION = os.path.join(DATA_DIR, "jobspecs", "duration")

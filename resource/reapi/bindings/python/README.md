# flux-sched Python Bindings

> Need a scheduler? I got you covered, in Python! 📅

![PyPI - Version](https://img.shields.io/pypi/v/flux-sched)

Python bindings for the **Flux Framework** scheduler (`flux-sched` or `fluxion`). These bindings provide a high-performance interface to Flux's graph-based resource model, allowing users to simulate scheduling decisions offline or interact with a live Flux instance.

## Installation

### From Source

To build and install from within the `flux-sched` source tree:

```bash
cd resource/reapi/bindings/python

# Build the "old school" way
python3 setup.py sdist bdist_wheel

# Install build...
sudo python3 -m pip install build --break-system-packages

# Try for isolated environment
pyproject-build

# This is more robust to work (detect libs, etc.)
python3 -m build --sdist --wheel --no-isolation
sudo pip install ./dist/flux_sched-0.0.0*.whl --break-system-packages
```

### Requirements

*   Python 3.8+
*   `flux-python` (recommended, make sure to match your Flux install version)
*   Flux Framework core libraries (`libflux-core`, `libflux-sched`) installed in system paths.

---

## Testing

To build or test Python, you will additionally need Cython installed and `pytest`. Then you can build flux-sched as normal.
Ensuring that the `flux_sched` module is on the `PYTHONPATH` (or installed) you can then do:

```bash
pytest -xs ./resource/reapi/bindings/python/tests/test*.py
```

To incrementally make changes and test, just cd to the Python directory and run `make`.

```bash
cd ./resource/reapi/bindings/python
make
pytest -xs ./tests/test_basic.py
```

## Usage ReapiCli

The `ReapiCli` class mimics the behavior of the internal scheduler logic in a standalone C++ object.

### 1. Initialization (Loading JGF)

To use the scheduler, you must initialize it with a Resource Graph. The following example uses **JGF (JSON Graph Format)** with the required metadata fields (`uniq_id`, `paths`, `basename`, etc.) derived from a standard Flux resource discovery.

```python
import json
from flux_sched.reapi_cli import ReapiCli, ReapiError

# 1. Create the Client
cli = ReapiCli()

# 2. Define the Resource Graph (JGF)
# This represents a minimal 1-Node system (Cluster -> Rack -> Node -> Socket -> Core)
# Note: Metadata fields like 'paths', 'uniq_id', and 'basename' are critical.
rgraph = json.dumps({
    "graph": {
        "nodes": [
            {
                "id": "0",
                "metadata": {
                    "type": "cluster", "basename": "tiny", "name": "tiny0", "id": 0,
                    "uniq_id": 0, "rank": -1, "exclusive": False, "size": 1,
                    "paths": {"containment": "/tiny0"}
                }
            },
            {
                "id": "1",
                "metadata": {
                    "type": "rack", "basename": "rack", "name": "rack0", "id": 0,
                    "uniq_id": 1, "rank": -1, "exclusive": False, "size": 1,
                    "paths": {"containment": "/tiny0/rack0"}
                }
            },
            {
                "id": "2",
                "metadata": {
                    "type": "node", "basename": "node", "name": "node0", "id": 0,
                    "uniq_id": 2, "rank": -1, "exclusive": False, "size": 1,
                    "paths": {"containment": "/tiny0/rack0/node0"}
                }
            },
            {
                "id": "3",
                "metadata": {
                    "type": "core", "basename": "core", "name": "core0", "id": 0,
                    "uniq_id": 3, "rank": -1, "exclusive": False, "size": 1,
                    "paths": {"containment": "/tiny0/rack0/node0/core0"}
                }
            }
        ],
        "edges": [
            {"source": "0", "target": "1", "metadata": {"subsystem": "containment"}},
            {"source": "1", "target": "2", "metadata": {"subsystem": "containment"}},
            {"source": "2", "target": "3", "metadata": {"subsystem": "containment"}}
        ]
    }
})

# 3. Define Loader Options
# "load-format": "json" tells the C++ reader to use the JGF parser.
# "prune-filters": "ALL:core" ensures we track resources down to the core level.
options = json.dumps({
    "load_format": "jgf",
    "prune_filters": "ALL:core",
    "subsystems": "containment",
    "policy": "high"
})

# 4. Initialize
cli.initialize(rgraph, options)
```

### 2. Matching & Allocation

Submit a Jobspec (resource request) to be matched against the loaded graph.

```python
# Request 1 Node
jobspec = json.dumps({
  "version": 9999,
  "resources": [
    {
      "type": "slot",
      "count": 1,
      "label": "default",
      "with": [
        {
          "type": "core",
          "count": 1
        }
     ]
    }
  ],
  "attributes": {
    "system": {
      "duration": 3600
    }
  },
  "tasks": [
    {
      "command": [
        "app"
      ],
      "slot": "default",
      "count": {
        "per_slot": 1
      }
    }
  ]
})

try:
    # match() returns:
    #   jobid (int):    The ID assigned to the job (0 if failure)
    #   reserved (bool): True if future reservation, False if immediate allocation
    #   R (str):        The assigned resources (R-spec string)
    #   at (int):       Time of assignment
    #   ov (float):     Performance overhead (seconds)
    jobid, reserved, R, at, ov = cli.match(jobspec, orelse_reserve=False)
    print(R)
```

### 3. Querying Job Info

Check the status of an allocated job.

```python
mode, is_reserved, at, ov = cli.info(jobid)    
print(f"Job {jobid} is currently: {mode}") 
```

### 4. Cancellation

Free resources associated with a job ID, making them available for new matches.

```python
cli.cancel(jobid)
print(f"Job {jobid} canceled. Resources released.")    
info = cli.info(jobid)
print(info)
```

### 5. Partial Cancellation

Release a *subset* of resources from an active allocation (e.g., for dynamic workflows or shrinking jobs).

```python
# 'R_subset' is a string (R-spec or RV1) defining the specific resources to drop.
# In this example, we assume we have an R string representing specific cores/nodes.
R_subset = '{"rank": 0, "children": ...}' 
is_fully_removed = cli.partial_cancel(jobid, R_subset)    
if is_fully_removed:
    print(f"Job {jobid} is now empty and fully removed.")
else:
    print(f"Job {jobid} was shrunk but remains active.")
```

## ReapiModule

The `ReapiModule` wraps a live Flux handle. It requires the `flux` python package.

```python
import flux
from flux_sched.reapi_module import ReapiModule

# 1. Connect to local Flux instance
# Make sure you flux start before running this example!
h = flux.Flux()

# 2. Initialize Wrapper
mod = ReapiModule()
mod.set_handle(h)
```
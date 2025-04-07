#!/bin/false
# Usage: flux python y2j <in.yaml >out.json

import sys
import yaml
import json

try:
    obj = yaml.safe_load(sys.stdin)
except (OSError, IOError) as e:
    sys.exit("y2j: " + e.strerror)
except yaml.YAMLError as e:
    sys.exit("y2j: " + e.problem)

json.dump(obj, sys.stdout, separators=(",", ":"))

#!/usr/bin/env python3
#
#  Generate docker manifests when necessary based on set of docker tags
#  provided by src/test/generate-matrix.py

import json
import subprocess
from collections import defaultdict

DOCKER_REPO = "fluxrm/flux-sched"

matrix = json.loads(
    subprocess.run(
        ["src/test/generate-matrix.py"], capture_output=True, text=True
    ).stdout
)

tags = defaultdict(list)
for entry in matrix["include"]:
    if "DOCKER_TAG" in entry["env"]:
        image = entry["image"]
        base = f"{DOCKER_REPO}:{image}"
        docker_tag = entry["env"]["DOCKER_TAG"]
        tags[base].append(docker_tag)
        # This is also a tagged version
        if entry.get("tag"):
            tag = entry["tag"]
            tags[f"{base}-{tag}"].append(docker_tag)

# Collect only those images with multiple tags or where image name does
# not equal tag name. This latter check ensures that non-multi-arch docker
# images for tags, e.g. fluxrm/flux-sched:el8-v0.81.0, have a corresponding
# image without the version tag, e.g. fluxrm/flux-sched:el8. This allows
# CI from other projects to immediately pick up the new version when pulling
# latest docker images after a tag. (See flux-core#7225).
tags = {k: v for k, v in tags.items() if len(v) > 1 or v[0] != k}

# The noble image is the canonical "latest" build. Create a latest
# manifest (and for tagged releases, also a versioned manifest) pointing
# to the same per-arch images as the el10 manifest.
latest_distro = "noble"
latest_key = f"{DOCKER_REPO}:{latest_distro}"
if latest_key in tags:
    latest_sources = tags[latest_key]
    github_tag = next(
        (entry["tag"] for entry in matrix["include"] if entry.get("tag")),
        None,
    )
    tags[f"{DOCKER_REPO}:latest"] = latest_sources
    if github_tag:
        tags[f"{DOCKER_REPO}:{github_tag}"] = latest_sources

for tag in tags.keys():
    print(f"docker manifest create {tag}", *tags[tag])
    subprocess.run(["docker", "manifest", "create", tag, *tags[tag]])
    print(f"docker manifest push {tag}")
    subprocess.run(["docker", "manifest", "push", tag])

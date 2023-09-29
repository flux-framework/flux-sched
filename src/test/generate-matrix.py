#!/usr/bin/env python3
#
#  Generate a build matrix for use with github workflows
#

import json
import os
import re

docker_run_checks = "src/test/docker/docker-run-checks.sh"

default_args = "--prefix=/usr" " --sysconfdir=/etc" " --localstatedir=/var"

DOCKER_REPO = "fluxrm/flux-sched"


class BuildMatrix:
    def __init__(self):
        self.matrix = []
        self.branch = None
        self.tag = None

        #  Set self.branch or self.tag based on GITHUB_REF
        if "GITHUB_REF" in os.environ:
            self.ref = os.environ["GITHUB_REF"]
            match = re.search("^refs/heads/(.*)", self.ref)
            if match:
                self.branch = match.group(1)
            match = re.search("^refs/tags/(.*)", self.ref)
            if match:
                self.tag = match.group(1)

    def create_docker_tag(self, image, env, command, platform):
        """Create docker tag string if this is master branch or a tag"""
        if self.branch == "master" or self.tag:
            tag = f"{DOCKER_REPO}:{image}"
            if self.tag:
                tag += f"-{self.tag}"
            if platform is not None:
                tag += "-" + platform.split("/")[1]
            env["DOCKER_TAG"] = tag
            command += f" --tag={tag}"
            return True, command

        return False, command

    def add_build(
        self,
        name=None,
        image="bookworm",
        args=default_args,
        jobs=4,
        env=None,
        docker_tag=False,
        coverage=False,
        recheck=True,
        platform=None,
        command_args="",
    ):
        """Add a build to the matrix.include array"""

        # Extra environment to add to this command:
        env = env or {}

        needs_buildx = False
        if platform:
            command_args += f"--platform={platform}"
            needs_buildx = True

        # The command to run:
        command = f"{docker_run_checks} -j{jobs} --image={image} {command_args}"

        # Add --recheck option if requested
        if recheck and "DISTCHECK" not in env:
            command += " --recheck"

        if docker_tag:
            #  Only export docker_tag if this is main branch or a tag:
            docker_tag, command = self.create_docker_tag(image, env, command, platform)

        if coverage:
            env["COVERAGE"] = "t"

        create_release = False
        if self.tag and "DISTCHECK" in env:
            create_release = True

        command += f" -- {args}"

        # TODO : remove this when the boost issue is dealt with
        if env.get("CC", "gcc").find("clang") < 0:
            cppflags = env.get("CPPFLAGS", "") + " -Wno-error=maybe-uninitialized"
            env["CPPFLAGS"] = cppflags

        self.matrix.append(
            {
                "name": name,
                "env": env,
                "command": command,
                "image": image,
                "tag": self.tag,
                "branch": self.branch,
                "coverage": coverage,
                "docker_tag": docker_tag,
                "needs_buildx": needs_buildx,
                "create_release": create_release,
            }
        )

    def __str__(self):
        """Return compact JSON representation of matrix"""
        return json.dumps(
            {"include": self.matrix}, skipkeys=True, separators=(",", ":")
        )


matrix = BuildMatrix()

# Debian: no args
matrix.add_build(name="bookworm")

# # Debian: 32b -- NOTE: VERY broken right now
# matrix.add_build(
#     name="bookworm - 32 bit",
#     image="bookworm",
#     platform="linux/386",
#     docker_tag=True,
# )

# Debian: arm64, expensive, only on master and tags, only install
if matrix.branch == "master" or matrix.tag:
    matrix.add_build(
        name="bookworm - arm64",
        image="bookworm",
        platform="linux/arm64",
        docker_tag=True,
        command_args="--install-only ",
    )

# Debian: gcc-12, distcheck
matrix.add_build(
    name="bookworm - gcc-12,distcheck",
    env=dict(
        CC="gcc-12",
        CXX="g++-12",
        DISTCHECK="t",
    ),
    args="--prefix=/usr",
)

# Debian: coverage
matrix.add_build(name="coverage", coverage=True, jobs=2)

# Debian: py3.7,clang-6.0
matrix.add_build(
    name="bookworm - clang-15",
    env=dict(
        CC="clang-15",
        CXX="clang++-15",
        CFLAGS="-O2 -gdwarf-4",
        chain_lint="t",
        TEST_CHECK_PREREQS="t",
    ),
    args='CXXFLAGS="-gdwarf-4"',
)

# Debian: TEST_INSTALL
matrix.add_build(
    name="bookworm - test-install",
    env=dict(TEST_INSTALL="t"),
    platform="linux/amd64",
    docker_tag=True,
)

# Ubuntu: TEST_INSTALL
matrix.add_build(
    name="jammy - test-install",
    image="jammy",
    env=dict(TEST_INSTALL="t"),
    docker_tag=True,
)

# Ubuntu 20.04: py3.8
matrix.add_build(
    name="focal",
    image="focal",
    docker_tag=True,
)

# Ubuntu 20.04: py3.8
matrix.add_build(
    name="focal - golang-test",
    image="focal-golang",
    env=dict(
        WITH_GO="yes",
    ),
)

# RHEL8 clone
matrix.add_build(
    name="el8",
    image="el8",
    docker_tag=True,
)

# Fedora34
matrix.add_build(
    name="fedora34",
    image="fedora34",
    docker_tag=True,
)

# Fedora38
matrix.add_build(
    name="fedora38",
    image="fedora38",
    docker_tag=True,
)

print(matrix)

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


def on_master_or_tag(matrix):
    return matrix.branch == "master" or matrix.tag


DEFAULT_MULTIARCH_PLATFORMS = {
    "linux/arm64": {
        # "when": on_master_or_tag, # use this to only run on merge
        "when": lambda _: True,
        "suffix": " - arm64",
        "timeout_minutes": 90,
        "runner": "ubuntu-24.04-arm",
    },
    "linux/amd64": {"when": lambda _: True, "runner": "ubuntu-latest"},
}


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
        image=None,
        args=default_args,
        jobs=6,
        env=None,
        docker_tag=False,
        coverage=False,
        coverage_flags=None,
        recheck=True,
        platform=None,
        command_args="",
        timeout_minutes=60,
        runner="ubuntu-latest",
    ):
        """Add a build to the matrix.include array"""

        if image is None:
            raise RuntimeError("you must specify an image explicitly")

        # Extra environment to add to this command:
        # NOTE: ensure we copy the dict rather than modify, re-used dicts can cause
        #       overwriting
        env = dict(env) if env is not None else {}

        # hwloc tries to look for opengl devices  by connecting to a port that might
        # sometimes be an x11 port, but more often for us is munge, turn it off
        env["HWLOC_COMPONENTS"] = "-gl"
        # the psm3 connector added to libfabrics in ~1.12 causes errors when allowed to
        # use non-local connectors on a system with virtual NICs, since we're in a
        # docker container, prevent this
        env["PSM3_HAL"] = "loopback"

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

        self.matrix.append(
            {
                "name": name,
                "env": env,
                "command": command,
                "image": image,
                "runner": runner,
                "tag": self.tag,
                "branch": self.branch,
                "coverage": coverage,
                "coverage_flags": coverage_flags,
                "docker_tag": docker_tag,
                "needs_buildx": needs_buildx,
                "create_release": create_release,
                "timeout_minutes": timeout_minutes,
            }
        )

    def add_multiarch_build(
        self,
        name: str,
        platforms=DEFAULT_MULTIARCH_PLATFORMS,
        default_suffix="",
        image=None,
        docker_tag=True,
        **kwargs,
    ):
        for p, args in platforms.items():
            if args["when"](self):
                suffix = args.get("suffix", default_suffix)
                self.add_build(
                    name + suffix,
                    platform=p,
                    docker_tag=docker_tag,
                    image=image if image is not None else name,
                    command_args=args.get("command_args", ""),
                    timeout_minutes=args.get("timeout_minutes", 30),
                    runner=args["runner"],
                    **kwargs,
                )

    def __str__(self):
        """Return compact JSON representation of matrix"""
        return json.dumps(
            {"include": self.matrix}, skipkeys=True, separators=(",", ":")
        )


matrix = BuildMatrix()

# Multi-arch builds, arm only builds on
common_args = (
    "--prefix=/usr"
    " --sysconfdir=/etc"
    " --with-systemdsystemunitdir=/etc/systemd/system"
    " --localstatedir=/var"
)
matrix.add_multiarch_build(
    name="bookworm",
    default_suffix=" - test-install",
    args=common_args,
    env=dict(
        TEST_INSTALL="t",
    ),
)

matrix.add_multiarch_build(
    name="noble",
    default_suffix=" - test-install",
    args=common_args,
    env=dict(
        TEST_INSTALL="t",
    ),
)
matrix.add_multiarch_build(
    name="el9",
    default_suffix=" - test-install",
    args=common_args,
    env=dict(
        TEST_INSTALL="t",
        CHECK_RUN_SOURCE_ENV="/opt/rh/gcc-toolset-13/enable",
    ),
)
# Disabled because the arm64 build is failing and preventing the
# generate-manifest step from running
# matrix.add_multiarch_build(
#     name="alpine",
#     default_suffix=" - test-install",
#     args=common_args,
#     env=dict(
#         TEST_INSTALL="t",
#     ),
# )
# single arch builds that still produce a container
matrix.add_build(
    name="fedora40 - test-install",
    image="fedora40",
    args=common_args,
    env=dict(
        TEST_INSTALL="t",
    ),
    docker_tag=True,
)

# Ubuntu: TEST_INSTALL, test oldest supported clang
matrix.add_build(
    name="jammy - test-install",
    image="jammy",
    env=dict(
        TEST_INSTALL="t",
        CC="clang-15",
        CXX="clang++-15",
        # NOTE: ancient valgrind (pre 3.20) fails with dwarf5
        CFLAGS="-gdwarf-4",
        CXXFLAGS="-gdwarf-4",
    ),
    docker_tag=True,
)

# Debian: gcc-12, distcheck
matrix.add_build(
    name="bookworm - gcc-12,distcheck",
    image="bookworm",
    env=dict(
        CC="gcc-12",
        CXX="g++-12",
        DISTCHECK="t",
    ),
    args="",
)

# fedora40: clang-18
matrix.add_build(
    name="fedora40 - clang-18",
    image="fedora40",
    env=dict(
        CC="clang-18",
        CXX="clang++-18",
        chain_lint="t",
    ),
)

# coverage
matrix.add_build(
    name="coverage",
    image="bookworm",
    coverage_flags="ci-basic",
    coverage=True,
)

# RHEL8 clone
matrix.add_build(
    name="el8 - test-install",
    image="el8",
    env=dict(
        TEST_INSTALL="t",
        # this is _required_ because of valgrind's new dependency on python3.11
        # which confuses rhel8 cmake's detection logic
        PYTHON="/usr/bin/python3.6",
    ),
    docker_tag=True,
)

print(matrix)

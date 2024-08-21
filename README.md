![ci](https://github.com/flux-framework/flux-sched/workflows/ci/badge.svg)[![codecov](https://codecov.io/gh/flux-framework/flux-sched/branch/master/graph/badge.svg)](https://codecov.io/gh/flux-framework/flux-sched)


*NOTE: The interfaces of flux-sched are being actively developed and
are not yet stable.* The github issue tracker is the primary way to
communicate with the developers.

### Fluxion: An Advanced Graph-Based Scheduler for HPC

Welcome to Fluxion<sup>1</sup>, an advanced job scheduling software
tool for High Performance Computing (HPC). Fluxion combines
graph-based resource modeling with efficient temporal plan
management schemes to schedule a wide range of HPC
resources (e.g., compute, storage, power etc)
in a highly scalable, customizable and effective fashion.

Fluxion has been integrated with flux-core to
provide it with both system-level batch job
scheduling and nested workflow-level scheduling.

See our [resource-query utility](https://github.com/flux-framework/flux-sched/blob/master/resource/utilities/README.md), if you want
to test your advanced HPC resource modeling and
selection ideas with Fluxion in a simplified,
easy-to-use environment.


### Fluxion Scheduler in Flux

Fluxion introduces queuing and resource matching services to extend Flux
to provide advanced batch scheduling. Jobs are submitted to Flux as usual,
and Fluxion makes a schedule to assign available resources to the job
requests according to its configured algorithm.

Fluxion installs two modules that are loaded by the Flux broker:

* `sched-fluxion-qmanager`, which manages one or more prioritized job queues
  with configurable queuing policies (fcfs, easy, conservative, or hybrid).
* `sched-fluxion-resource`, which matches resource requests to available
  resources using Fluxion's graph-based matching algorithm.


#### Building Fluxion

Fluxion requires an installed flux-core package.  Instructions
for installing flux-core can be found in [the flux-core
README](https://github.com/flux-framework/flux-core/blob/master/README.md).

To install our other dependencies on a dnf, apk or apt-using distro you should
be able to use ./scripts/install-deps.sh to install all necessary dependencies.

<!-- A collapsible section with markdown -->
<details>
  <summary>Click to expand and see our full dependency table</summary>

Fluxion also requires the following packages to build:

**redhat**                | **ubuntu**              | **version**       | **note**
----------                | ----------              | -----------       | --------
hwloc-devel               | libhwloc-dev            | >= 2         |
boost-devel               | libboost-dev            | >= 1.66 | *1*
boost-graph               | libboost-graph-dev      | >= 1.66 | *1*
libedit-devel             | libedit-dev             | >= 3.0            |
python3-pyyaml            | python3-yaml            | >= 3.10           |
yaml-cpp-devel            | libyaml-cpp-dev         | >= 0.5.1          |

</details>

##### Installing RedHat/CentOS Packages
```bash
sudo dnf install hwloc-devel boost-devel boost-graph libedit-devel python3-pyyaml yaml-cpp-devel
```

##### Installing Ubuntu Packages

```bash
sudo apt-get update
sudo apt install libhwloc-dev libboost-dev libboost-graph-dev libedit-dev libyaml-cpp-dev python3-yaml
```

Clone flux-sched, the repo name for Fluxion, from an upstream repo and prepare for configure:
```bash
git clone <flux-sched repo of your choice>
cd flux-sched
```

Fluxion uses a CMake based build system, and can be configured and
built as usual for cmake projects. If you wish you can use one of
our presets, the default is a `RelWithDebInfo` build using ninja 
that's good for most purposes:

```bash
cmake -B build --preset default
cmake --build build
cmake --build build -t install
ctest --test-dir build
# OR
cmake -B build
make -C build
make -C build check
make -C build install
# OR
cmake -B build -G Ninja
ninja -C build
ninja -C build check
ninja -C build install
```


If you prefer autotools style, we match the rest of the flux project
by offering a `configure` script that will provide the familiar autotools
script interface but use cmake underneath.

The build system will attempt to find a flux-core in the same prefix
as specified on the command line. If `-DCMAKE_INSTALL_PREFIX`, or for
configure `--prefix`, is not specified, then it will default to the 
same prefix as was used to install the first `flux` executable found 
in `PATH`. Therefore, if `which flux` returns the version of flux-core
against which Fluxion should be compiled, then the configuration may
be run without any arguments. If flux-core is side-installed, then
the prefix should be set to the same prefix as was used to install
the target flux-core. For example, if flux-core was installed in
`$FLUX_CORE_PREFIX`:

```bash
cmake -B build --preset default -DCMAKE_INSTALL_PREFIX="$FLUX_CORE_PREFIX"
cmake --build build
ctest --test-dir build
cmake --build build -t install
# OR
mkdir build
cd build
../configure --prefix=${FLUX_CORE_PREFIX}
make
make check
make install
```

To build go bindings, you will need go (tested with 1.19.10) available, and then:

```bash
export WITH_GO=yes
cmake -B build
cmake --build build
ctest --test-dir build
cmake --build build -t install
```

To run just one test, you can cd into t in the build directory, then run the script
from the source directory or use the usual ctest options to filter by regex:

```bash
$ cd build/t
$ ../../t/t9001-golang-basic.t
ok 1 - match allocate 1 slot: 1 socket: 1 core (pol=default)
ok 2 - match allocate 2 slots: 2 sockets: 5 cores 1 gpu 6 memory
# passed all 2 test(s)
1..2
# OR
cd build
ctest -R t9001 --output-on-failure
```

To run full tests (more robust and mimics what happens in CI) you can do:

```bash
ctest
```

##### Flux Instance

The examples below walk through exercising functioning flux-sched modules (i.e.,
`sched-fluxion-qmanager` and `sched-fluxion-resource`) in a Flux instance.
The following examples assume
that flux-core and Fluxion were both installed into
`${FLUX_CORE_PREFIX}`. For greater insight into what is happening, add the -v
flag to each flux command below.

Create a comms session comprised of 3 brokers:
```
${FLUX_CORE_PREFIX}/bin/flux start -s3
```
This will create a new shell in which you can issue various
flux commands such as following.

Check to see whether the qmanager and resource modules are loaded:
```
flux module list
```

Submit jobs:
```
flux submit -N3 -n3 hostname
flux submit -N3 -n3 sleep 30
```

Examine the status of these jobs:
```
flux jobs -a
```

Examine the output of the first job
```
flux job attach <jobid printed from the first submit>
```

Examine the ring buffer for details on what happened.
```
flux dmesg
```

Exit the Flux instance
```
exit
```

----
<sup>1</sup> The name was inspired by
Issac Newton's *Method of Fluxions* where
[fluxions](https://en.wikipedia.org/wiki/Fluxion) and
[fluents](https://en.wikipedia.org/wiki/Fluent_\(mathematics\))
are the key terms to define his calculus.
As his calculus describes the motion of points in time
for time-varying variables,
our Fluxion scheduler uses scalable techniques to
describe [the motion of scheduled points in time](https://github.com/flux-framework/flux-sched/blob/master/resource/planner/README.md)
for a diverse set of resources. 


#### License

SPDX-License-Identifier: LGPL-3.0

LLNL-CODE-764420

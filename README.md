[![Build Status](https://travis-ci.org/flux-framework/flux-sched.svg?branch=master)](https://travis-ci.org/flux-framework/flux-sched) [![Coverage Status](https://coveralls.io/repos/flux-framework/flux-sched/badge.svg?branch=master&service=github)](https://coveralls.io/github/flux-framework/flux-sched?branch=master)

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
to provide advanced batch scheduling. Jobs are submitted to Fluxion via
`flux job submit` which are then added to our queues for scheduling.

At the core of its functionality lie its two service modules:
`sched-fluxion-qmanager` and `sched-fluxion-resource`.
The first module is designed to manage our job queues and
to enforce queueing policies that are configurable (e.g.,
first-come-first-served, EASY, conservative backfilling policies etc).
The second module uses a graph to represent
resources of arbitrary types
as well as their complex relationships and to match the highly sophisticated
resource requirements of a Flux jobspec to the compute and other resources
on this graph. Both of these modules are loaded into a Flux instance and
work in tandem to provide highly effective scheduling.

Clearly, we recognize that a single scheduling policy will not sufficiently
optimize the scheduling of different kinds of workflows. In fact, one of the
main design points of flux-sched is its ability to customize the scheduling
behaviors. Users can use environment variables or module-load time options
to select and to tune the policies as to how resources are selected
and when to run their jobs.

Overall, the advanced job scheduling facility within Fluxion offers vastly
many opportunities for modern HPC and other worfklows to meet their highly
challenging scheduling objectives.


#### Building Fluxion

Fluxion requires an installed flux-core package.  Instructions
for installing flux-core can be found in [the flux-core
README](https://github.com/flux-framework/flux-core/blob/master/README.md).

Fluxion also requires the following packages to build:

```
libhwloc-dev >= 1.11.1
libboost packages == 1.53 or > 1.58
  -  libboost-dev
  -  libboost-system-dev
  -  libboost-filesystem-dev
  -  libboost-thread-dev
  -  libboost-graph-dev
  -  libboost-regex-dev
libxml2-dev >= 2.9.1
yaml-cpp-dev >= 0.5.1
python3-yaml >= 3.10
```

##### Installing Ubuntu Packages

```
apt install libhwloc-dev libboost-dev libboost-system-dev libboost-filesystem-dev libboost-thread-dev libboost-graph-dev libboost-regex-dev libxml2-dev libyaml-cpp-dev python3-yaml
```

Clone flux-sched, the repo name for Fluxion, from an upstream repo and prepare for configure:
```
git clone <flux-sched repo of your choice>
cd flux-sched
./autogen.sh
```

The Fluxion's `configure` will attempt to find a flux-core in the
same `--prefix` as specified on the command line. If `--prefix` is
not specified, then it will default to the same prefix as was used
to install the first `flux` executable found in `PATH`. Therefore,
if `which flux` returns the version of flux-core against which
Fluxion should be compiled, then `./configure` may be run without
any arguments. If flux-core is side-installed, then `--prefix` should
be set to the same prefix as was used to install the target flux-core.

For example, if flux-core was installed in `$FLUX_CORE_PREFIX`:

```
./configure --prefix=${FLUX_CORE_PREFIX}
make
make check
make install
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
flux mini submit -N3 -n3 hostname
flux mini submit -N3 -n3 sleep 30
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

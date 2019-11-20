[![Build Status](https://travis-ci.org/flux-framework/flux-sched.svg?branch=master)](https://travis-ci.org/flux-framework/flux-sched) [![Coverage Status](https://coveralls.io/repos/flux-framework/flux-sched/badge.svg?branch=master&service=github)](https://coveralls.io/github/flux-framework/flux-sched?branch=master)

*NOTE: The interfaces of flux-sched are being actively developed and
are not yet stable.* The github issue tracker is the primary way to
communicate with the developers.

### flux-sched

flux-sched contains an advanced job scheduling facility for the Flux resource
manager framework.

### Overview

flux-sched introduces queuing and resource matching services to extend Flux
to provide advanced batch scheduling. Jobs are submitted to flux-sched via
`flux job submit` which are then added to our queues for scheduling.

At the core of its functionality lie its two service modules: `qmanager` and
`resource`. The `qmanager` module is designed to manage our job queues and
to enforce queueing policies that are configurable (e.g.,
first-come-first-served, EASY, conservative backfilling policies etc).
The `resource` module uses a graph to represent resources of arbitrary types
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

Overall, the advanced job scheduling facility within flux-sched offers vastly
many opportunities for modern HPC and other worfklows to meet their highly
challenging scheduling objectives.


#### Building flux-sched

flux-sched can only be built against an installed flux-core build.  If
the flux-core is side installed, the PKG_CONFIG_PATH environment
variable must include the location to flux-core's pkgconfig directory.

The example below is of a side install of flux-core.  In this example,
the flux-core and flux-sched repos are under the $HOME directory.  The
flux-core repo is cloned, configured with a prefix that identifies the
install directory ($HOME/local), built and installed into that
directory.

```
git clone <flux-core repo of your choice>
cd ~/flux-core
./autogen.sh
./configure --prefix=$HOME/local
make
make install
```

The next step is to build the sched module.
flux-sched requires the following packages to build:

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
python-yaml >= 3.10
```

The sched module contains
a bifurcated structure of a core framework that has all the basic
functionality plus loadable modules that implement specific
scheduling behaviors.

To build the sched module, run the following commands:

```
export PKG_CONFIG_PATH=$HOME/local/lib/pkgconfig:$PKG_CONFIG_PATH
git clone <flux-sched repo of your choice>
cd ~/flux-sched
./autogen.sh
./configure --prefix=$HOME/local
make
make check
make install
```

To exercise functioning flux-sched modules in a Flux instance, follow
these steps.

##### Flux Instance

To run the example below, you will have to manually add flux-sched
directories to the pertinent environment variables.  The following
example assumes that flux-core and flux-sched live under your home
directory.  For greater insight into what is happening, add the -v
flag to each flux command below.

Create a comms session comprised of 3 brokers:
```
$HOME/local/bin/flux start -s3
```

Check to see whether the qmanager and resource modules are loaded:
```
flux module list
```

Submit jobs:
```
flux mini submit -N3 -n3 hostname
flux mini submit -N3 -n3 sleep 30
```

Examine the currently running job:
```
flux job list
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

[![Build Status](https://travis-ci.org/flux-framework/flux-sched.svg?branch=master)](https://travis-ci.org/flux-framework/flux-sched)
[![Coverage Status](https://coveralls.io/repos/flux-framework/flux-sched/badge.svg?branch=master&service=github)](https://coveralls.io/github/flux-framework/flux-sched?branch=master)

*NOTE: The interfaces of flux-sched are being actively developed and
are not yet stable.* The github issue tracker is the primary way to
communicate with the developers.

### flux-sched

flux-sched contains **Fluxion**, a graph-based job scheduler for
the Flux resource manager framework.  It consists of two flux
modules in support of batch job scheduling:
`fluxion-resource` and `fluxion-qmanager`. The former is
our resource matching service that represents
computing and other resources in a graph and matches a job's
resource requirements on this graph according to the
configurable resource match policy.
On the other hand, `fluxion-qmanager` implements queuing policies
on the job queues in accordance, again, with to the configurable
queue policies and parameters.
Combined, **Fluxion** provides computing sites and end users
alike many opportunites to create a job scheduling behavior
specifically tailored to their workflow characteristics and
the architectural and other constraints of their systems.

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
uuid-dev
```

The sched module contains
a bifurcated structure of a core framework that has all the basic
functionality plus the aforementioned modules that implement specific
scheduling behavior.

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

To exercise **Fluxion** in a comms session, follow
these steps.

##### Flux comms session

To run the example below, you will have to manually add flux-sched
directories to the pertinent environment variables.  The following
example assumes that flux-core and flux-sched live under your home
directory.  For greater insight into what is happening, add the -v
flag to each flux command below.

Create a comms session comprised of 3 brokers:
```
export FLUX_MODULE_PATH=$HOME/flux-sched/resource/modules:$HOME/flux-sched/qmanager/modules
$HOME/local/bin/flux start -s3
```

cd to ~/flux-sched and load the Fluxion modules:
```
flux module load -r 0 fluxion-resource hwloc-whitelist="node,core,gpu"
flux module load -r 0 fluxion-qmanager"
```

Check to see whether the sched module loaded:
```
flux module list
```

Submit a job:
```
flux jobspec srun -n3 -t 1 sleep 60 > myjobsepc
flux job submit myjobspec
```

Examine the job:
```
flux job list
```

Examine the ring buffer for details on what happened.
```
flux dmesg
```

Exit the session:
```
exit
```

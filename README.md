[![Build Status](https://travis-ci.org/flux-framework/flux-sched.svg?branch=master)](https://travis-ci.org/flux-framework/flux-sched)
[![Coverage Status](https://coveralls.io/repos/flux-framework/flux-sched/badge.svg?branch=master&service=github)](https://coveralls.io/github/flux-framework/flux-sched?branch=master)

*NOTE: The interfaces of flux-sched are being actively developed and
are not yet stable.* The github issue tracker is the primary way to
communicate with the developers.

### flux-sched

flux-sched contains the job scheduling facility for the Flux resource
manager framework.  It consists of an engine that handles all the
functionality common to scheduling.  The engine has the ability to
load one or more scheduling plugins that provide specific scheduling
behavior.

#### Overview

The `flux wreckrun` command is part of flux-core and provides the
ability to launch Flux programs on resources within a Flux instance.
flux-sched adds batch scheduling functionality to Flux.  Jobs are
submitted to flux-sched using `flux submit` which adds the jobs to a
queue.  A scheduling plugin selects resources and decides when to run
the job.

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
libboost-dev >= 1.53
```

The sched module contains
a bifurcated structure of a core framework that has all the basic
functionality plus a loadable plugin that implements specific
scheduling behavior.  There are currently two plugins available:
sched.fcfs and sched.backfill.

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

To exercise a functioning sched module in a comms session, follow
these steps.

##### Flux comms session

To run the example below, you will have to manually add flux-sched
directories to the pertinent environment variables.  The following
example assumes that flux-core and flux-sched live under your home
directory.  For greater insight into what is happening, add the -v
flag to each flux command below.

Create a comms session comprised of 3 brokers:
```
export LUA_PATH="$HOME/flux-sched/rdl/?.lua;${LUA_PATH};;"
export LUA_CPATH="$HOME/flux-sched/rdl/?.so;${LUA_CPATH};;"
export FLUX_MODULE_PATH=$HOME/flux-sched/sched
$HOME/local/bin/flux start -s3
```

cd to ~/flux-sched and load the sched module specifying the
appropriate rdl configuration file:
```
flux module load sched rdl-conf=$HOME/flux-sched/conf/hype.lua plugin=sched.fcfs
```

Check to see whether the sched module loaded:
```
flux module list
```

Submit a job:
```
flux submit -N3 -n3 sleep 30
```

Examine the job:
```
flux kvs dir lwj.1
```

Examine the ring buffer for details on what happened.
```
flux dmesg
```

Exit the session:
```
exit
```

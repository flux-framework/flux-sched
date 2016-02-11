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

flux-sched requires additional utilities from flux-core that are not
included in the flux-core library. Rather than copy/paste the source
code to these extra utilities into flux-sched, we opted to point to
the flux-core source files and developed a mechanism for identifying
where those extra source files can be found.  In the example below,
the root directory for the flux-core source files is
$HOME/local/build.

flux-sched's configure script offers the `--with-flux-core-builddir`
option that provides the means of identifying the location of the
source to the utilities.

The example below is of a side install of flux-core.  The flux-core
repo is cloned, configured with a prefix that identifies the install
directory, built and installed into that directory.  It assumes the
repos for flux-core and flux-sched are off your home directory.  It
also uses a prefix for flux-core called $HOME/local.

```
git clone <flux-core repo of your choice>
cd ~/flux-core
mkdir -p $HOME/local/build
cp -r * $HOME/local/build
./autogen.sh
./configure --prefix=$HOME/local
make
make install
```

The next step is to build the sched module.  The sched module contains
a bifurcated structure of a core framework that has all the basic
functionality plus a loadable plugin that implements specific
scheduling behavior.  There are currently two plugins available:
sched.plugin1 and backfill.plugin1.

To build the sched module, run the following commands:

```
export PKG_CONFIG_PATH=$HOME/local/lib/pkgconfig:$PKG_CONFIG_PATH
export FLUXOMETER_LUA_PATH=$HOME/local/share/lua/5.1
git clone <flux-sched repo of your choice>
cd ~/flux-sched
./autogen.sh
./configure --with-flux-core-builddir=$HOME/local/build
make
make check
```

`make install` is not yet supported in flux-sched.  When you run make
in ~/flux-sched, the products remain local; they do not populate
anything in ~/flux-core.

To actually exercise a functioning sched module in a comms session,
follow these steps.

##### SLURM session

The following instructions assume you have successfully run a SLURM
session as described in the flux-core README.  Also, if you have run
`make start` or `make load`, exit now.

To run the example below, you will have to manually add flux-sched
directories to the flux commands.  The following example assumes that
flux-core and flux-sched live under your home directory.  For greater
insight into what is happening, add the -v flag to each flux command
below.

Create a comms session within SLURM across 3 nodes with one rank per
node:
```
export LUA_PATH="${LUA_PATH};~/flux-sched/rdl/?.lua"
export LUA_PATH="${LUA_CPATH};~/flux-sched/rdl/?.so"
export FLUX_MODULE_PATH=~/flux-sched/sched
srun -N3 --pty ~/flux-core/src/cmd/flux broker
```

cd to ~/flux-sched and load the sched module specifying the
appropriate rdl configuration file:
```
flux module load sched rdl-conf=conf/hype.lua
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

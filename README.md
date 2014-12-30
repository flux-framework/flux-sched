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

The "flux wreckrun" command is part of flux-core and provides the
ability to launch light weight jobs on resources within a comms
session.  flux-sched adds batch scheduling functionality to Flux.
Jobs are submitted to flux-sched using "flux submit" which adds the
jobs to a queue.  A scheduling plugin selects resources and decides
when to run the job.

#### Building flux-sched

Start with clones of flux-core and flux-sched.  To distinguish the
directory names from the repository names, the cloned directories are
referred to as ~/flux-core and ~/flux-sched in the discussion below.
Follow the instructions provided in the flux-core README to build
flux-core.  Essentially it is:

```
cd ~/flux-core
./autogen.sh
./configure
make
make check
```

At this point, the build will have created the "config" file in
~/flux-core/etc/flux populated with values for cmbd_path, exec_path,
lua_cpath, lua_path, man_path, and module_path that facilitate running
flux out of the repo.  These would suffice if all you ran were
commands out of ~/flux-core.  But we will need to append some paths
(see below) to exercise the flux-sched facility.

The next step is to build the sched module.  The sched module contains
a bifurcated structure of a core framework that has all the basic
functionality plus a loadable plugin that implements specific
scheduling behavior.  There is currently only one plugin.  It is
called sched.plugin1 and it is hard coded at the moment to load
automatically by the sched comms module.

To build the sched module, run the following commands:

```
cd ~/flux-sched
./config ../flux-core
make
```

The flux-sched project has a rudimentary structure right now and does
not use autotools.  "make install" is not available.  To perhaps state
the obvious, when you run make in ~/flux-sched, the products remain
local; they do not populate anything in ~/flux-core.

Stepping into the ~/flux-sched/sched directory, there are a couple of
quick checks you can run: "make start" and "make load".  These pull in
the definitions from ~/flux-core/etc/flux/config, establish some
important environment variables, and should successfully fire up a
comms session of one rank and load the sched module.  You can confirm
by running "flux module list".  The PATH environment has been extended
to include the path to flux (under ~/flux-core/).

These two make targets are simple checks.  To actually exercise a
functioning sched module in a comms session, follow these steps.

##### SLURM session

The following instructions assume you have successfully run a SLURM
session as described in the flux-core README.  Also, if you have run
"make start" or "make load", exit now.

To run the example below, you will have to manually add flux-sched
directories to the flux commands.  The following example assumes that
flux-core and flux-sched live under your home directory.  For greater
insight into what is happening, add the -v flag to each flux command
below.

Create a comms session within SLURM across 3 nodes with one rank per
node:
```
~/flux-core/src/cmd/flux -M ~/flux-sched/sched -C ~/flux-sched/rdl/\?.so -L ~/flux-sched/rdl/\?.lua start -s 3 -S -N 3
```

Load the sched module specifying the appropriate rdl configuration
file:
```
~/flux-core/src/cmd/flux -M ~/flux-sched/sched -C ~/flux-sched/rdl/\?.so -L ~/flux-sched/rdl/\?.lua module load sched rdl-conf=../conf/hype.lua
```

Check to see whether the sched module loaded:
```
~/flux-core/src/cmd/flux module list
```

Submit a job:
```
~/flux-core/src/cmd/flux -x ~/flux-sched/sched submit -N3 -n3 sleep 30
```

Examine the job:
```
~/flux-core/src/cmd/flux kvs dir lwj.1
```

Exit the session:
```
exit
```

Read the cmbd.log that was created for details on what happened.

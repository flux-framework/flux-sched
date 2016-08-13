
flux-sched version 0.2.0 - 2016-08-13
-------------------------------------

#### New Features

 * Versatile scheduling: replace resource tree list searches
   with tree searches.

 * Runtime selection of backfill algorithm (#159).
   Examples of selecting various algorithms are:

  - *Conservative* : `flux module load sched.backfill reserve-depth=-1`
  - *EASY*         : `flux module load sched.backfill reserve-depth=1`
  - *Hybrid*       : `flux module load sched.backfill reserve-depth=4`
  - *Pure*         : `flux module load sched.backfill reserve-depth=0`

#### Fixes

 * Update to latest flux-core logging apis

 * Fix segfaults in simulator during shutdown

 * Small leak fixes in simulator

#### Deprecations

 * Remove `sim_sched` code that ran under simulator. The simulator
   now interfaces directly with sched module.

flux-sched version 0.1.0 - 2016-05-16
-------------------------------------

 * Initial release for build testing only

flux-sched version 0.6.0 - 2018-08-03
-------------------------------------

### Fixes
 * sched: fix request handler type mismatch on i386 (#346)
 * sched: Fix a job resrc_tree memory leak (#365)
 * sched: Print error messages when schedule_job cannot run (#348)
 * simulator: track a flux-core API change (#351)

### Features
 * resource: Introduce planner_multi_t  (#349)
 * Add hwloc-based GPU scheduling support (#354)
 * add sched parameter get & set commands to wreck (#362)
 * rdl: Lua 5.2 compatibility (#373)

### Cleanup
 * travis: change email notifications to go to the flux-sched-dev listserv
   (#353)


flux-sched version 0.5.0 - 2018-05-11
-------------------------------------

### Fixes
 * c++ configuration changes to mirror flux-core (#270)
 * track upstream changes in flux-core APIs (#271, #272, #278)
 * portability fixes for Ubuntu 16.04 and 17.10 (#298, #301)
 * fixes for hwloc input xml (#310)

### Features
 * add pending job cancellation support (#291)
 * add support for excluding resources for specific nodes (#305)
 * support for GPU scheduling (#313)
 * add new, more scalable resource assignment schema in kvs (R_lite) (#321)
 * include host names in R_lite resource assignment data (#324)
 * optimize queue-depth=1 FCFS (#294)
 * support for enhanced wreck submitted state event (#295)
 * support for new completing state for wreck jobs (#341)
 * add planner and resource-query utility support (#283, #323, #325, #274, #281)
 * initial priority plugin development (#290)

### Cleanup
 * testsuite enhancements (#316, #328, #331)
 * drop old rank.N method for resource assignment (#339)


flux-sched version 0.4.0 - 2017-08-24
-------------------------------------

### Fixes
 * Fix allocation of cores unevenly across nodes (#226)
 * Fixes and updates to track latest libflux-core API changes
   (#255, #253, #246, #238, #234)
 * simulator: fix and re-enable sim integration tests (#250)
 * Fix autoreconf when using side-installed autotools (#243)
 * flux-waitjob: fixes to avoid flux-core issue #1027 (#244)
 * Other fixes (#227, #231)

#### New features
 * simulator: add ISO time parsing to job traces (#260)
 * resrc: add resource API context support (#236)

#### Cleanup
 * simulator: refactor, cleanup, and ensure proper initialization.
   (#240, #250)

flux-sched version 0.3.0 - 2016-10-28
-------------------------------------

#### New features

 * Support resource membership in multiple hierarchies and
   include "flow graphs" for modeling flow resources such as power
   and bandwidh. Enhance find/select of resources to support consideration
   of power, bandwidth, etc.  (#189)

 * Add configurable scheduling optimizations for throughput-oriented workloads
   (#190, #191):

   - `sched-params=queue-depth=<k>`:
       limits the number of jobs to consider per scheduling loop
   - `sched-params=delay-scheduling=true`:
       delays scheduling to batch up individual scheduling events

#### Fixes

 * Updates to track changes in flux-core (#188, #207, #212, #216)

#### Misc

 * Update JSON handling to libjansson (#215, #220)

 * Simulator updates and cleanup (#203, 218, #219)

 * RDL enhancements for multi-hierarchy support (#196)

 * Do not assume kvs path to jobid is in lwj.jobid (#216)


flux-sched version 0.2.0 - 2016-08-13
-------------------------------------

#### New Features

 * Versatile scheduling: replace resource tree list searches
   with tree searches.

 * Runtime selection of backfill algorithm (#159).
   Examples of selecting various algorithms are
   (first remove any loaded sched plugin, e.g., flux module remove sched.fcfs):

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

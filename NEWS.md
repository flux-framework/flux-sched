flux-sched version 0.9.0 - 2020-06-18
-------------------------------------

Note: flux-sched has been re-branded to "Fluxion"!

### Fixes
 * testsuite: break a race in a qmgr reload test (#617)
 * testsuite: avoid rc3 hang in qmanager-reload test (#593)
 * Fix build/test issues and enable travis on CentOS 8 (#565)
 * t4000-match-params.t: suppress flux module unload failure (#561)
 * Fix t2002-qmanager-reload.t with new core resource module (#661)
 * docker: add ENTRYPOINT to start munged by default (#648)
 * testsuite: prevent python version mismatch in test scripts (#568)

### Features
 * Add state recovery support (#663, #666)
 * qmanager: add multi-queue support (#649)
 * Add flux-tree (#622, #621, #616, #582)
 * Add configuration file support (#599, #645, #630)
 * Multi-tiered storage scheduling support demo and tests (#620)

### Cleanup
 * Update the project name to Fluxion (#655)
 * qmanager: add exception safety for schedutil callbacks (#646)
 * Add path, vertex pair to by_path map (#641)
 * testsuite: update flux-sharness.sh from flux-core (#657)
 * Use Jansson for JSON-based writers + time keys in RV1 (#614)
 * rc: use reload -f to load qmanager/resource (#619)
 * simplify build process and instructions (#603, #578, #595, #578, #579)
 * testsuite: update flux-job list usage (#571)
 * mergify: add config from flux-core (#562)
 * github: add workflow action to validate PR commits (#553)
 * drop flux module -r option (#557)

flux-sched version 0.8.0 - 2019-11-20
-------------------------------------

### Fixes
 * resource/sched/simulator: update KVS API usage (#435)
 * build: fix build issues for priority_mod_fair_tree.so (#434)
 * api: sync flux-sched with removed interfaces in flux-core (#440)
 * resource: update for kvs watch API removal (#442)
 * API: remove the use of deprecated Python API (#450)
 * resource: update to flux_respond_error() (#457)
 * resource: bug fix for incorrectly handling implicit exclusivity (#502)
 * libschedutil: remove vendored copy and use flux-core's exported lib (#516)
 * testsuite: update flux-sharness.sh to version from flux-core (#523)
 * rc3: Fix a bug in RC3 dir definition within configure.ac (#525)
 * resource: fix buffer overflow when handling slot type in a jobspec (#548)

### Features
 * qmanager: integrate with the new exec system (#481)
 * qmanager: add hello/exception callback support (#493)
 * qmanager: add EASY/HYBRID/CONSERVATIVE policies (#504)
 * resource: RFC20 resource set specification version 1 support (#455)
 * resource: add hwloc whitelist support (#467)
 * resource: add set- and get-property support (#490, #513)
 * resource: add support for checking a job's satisfiability (#503)
 * resource: support for variation-aware scheduler (#517)
 * resource: add JGF reader support (#521)
 * resource: resource graph metadata (by_path) optimization (#536)
 * libjobspec: update command to be list instead of list or string (#549)
 * resource: add resource update support (#543)
 * add smart pointer support and misc. cleanup (#537)
 * test: add test cases to support systems with disaggregated resources (#460)
 * test: add test cases for AMD GPUs (#464)

### Cleanup
 * resource: tidy up JGF match writer support (#520)

flux-sched version 0.7.0 - 2019-01-20
-------------------------------------

### Fixes
 * sched/plugin: track flux-core module API changes (#414)
 * Ensure scheduling correctness with different prune filter configurations
   (#419)
 * travis: fix docker and github release deployment (#424)
 * travis-ci: fix docker deploy better (#426)
 * docker: update repo tag names, ensure flux-sched installed with
   prefix=/usr (#427)
 * docker: add yaml-cpp dependency (#428)
 * config: Fix a non-portable use of schell conditionals for automake
   (#405)
 * config: Add libtoolize into autogen.sh (#406)
 * Integrate jobspec into resource, etc (#398)
 * Compilation fixes (#417)

### Features
 * travis-ci: use docker for test builds (#392)
 * resource: support for hwloc ingestion (#385)
 * Add support for resource matching service (#386)
 * planner: Replace zhash_t to zhashx_t for higher performance (#391)
 * resource: wire in --prune-filters option for resource-query and
   matching module (#401)
 * resource: Add run-level support for resource matching module (#418)
 * Add better autoconf support for libboost (#394)

### Cleanup
 * build: do not install libflux-rdl.so, fix rebuild of aclocal.m4 on
   first make (#421)
 * sched: remove trailing whitespaces (#376)


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

flux-sched version 0.25.0 - 2022-10-04
--------------------------------------

Note: the flux-sched test suite requires flux-core 0.44.0 or newer.

### New Features

 * qmanager: support RFC33 TOML queue config (#980, #971)

### Fixes
 * Resource graph duration and job expiration to conform to RFC 14 (#969)
 * Fix REAPI C++ bindings  (#974)
 * add an exception note to fatal queue exceptions (#957)

### Cleanup

 * rc: combine fluxion rc scripts (#958)
 * move flux-tree to the test suite (#956)
 * build: drop unnecessary preqreqs, update README (#954)

### Testsuite

 * testsuite: fix coverage method for queue exception (#978)
 * testsuite: cover queues with non-overlapping resource constraints (#976)
 * testsuite: fix integer overflow (#968)
 * testsuite: use explicit duration units (#966)
 * minor cleanup (#973, #959)

flux-sched version 0.24.0 - 2022-08-03
--------------------------------------

### Fixes

 * Always emit exclusive resources with --exclusive (#951)


flux-sched version 0.23.0 - 2022-05-19
--------------------------------------

### Features

 * Add property constraints-based matching support (#922)

### Fixes

 * qmanager: fix potential leak in alloc callback (#943)
 * hlapi: workaround integer compare error with gcc 9.4.0 (#937)
 * docs: fix code sample in resource README (#935)
 * Fix an integer overflow bug etc. (#933)
 * reader: fix index type (#934)
 * Don't assume each hierarchical path is unique (#924)


flux-sched version 0.22.0 - 2022-04-11
--------------------------------------

### Features

 * Add rv1exec reader support (#921)
 * resource: support discovery of AMD RSMI GPUs in hwloc reader (#918)

### Testsuite

 * testsuite: disable hwloc reader tests (#926)
 * testsuite: disable more failing test that need resource.get-xml (#929)


flux-sched version 0.21.1 - 2022-03-04
--------------------------------------
A minor release fixing two problems discovered during system
instance testing of v0.21.0.

### Fixes
 * Encode the nodelist key in strict rank order (#893, #916)
 * build: fix missing doc file in distributed tarball (#915)


flux-sched version 0.21.0 - 2022-03-02
--------------------------------------
Check out our new node-exclusive scheduling support!

### Features
 * HLAPI: add CLI initialization and implement match_allocate, cancel,
   and info (#888)
 * fluxion-resource: Introduce multilevel ID match policy support (#895)
 * fluxion-resource: Add node-exclusive scheduling (#900)
 * doc: add infrastructure for sphinx documentation (#906)
 * config: Add TOML config support for sched-fluxion-[qmanager|resource]
   (#906, #910)

### Fixes
 * doc: add LLNL code release number to README.md (#885)
 * build: Fix compilation issues with clang-12 and gcc-11 (#891)
 * build: add JANSSON_CFLAGS into Makefile.am (#892)
 * Replace DBL_MAX with std::numeric_limits (#902)
 * testsuite: fix a couple brittle tests (#909)

### Cleanup
 * testsuite: use flux mini not flux jobspec (#884)
 * t5000-valgrind.t: do not run valgrind test by default (#887)
 * testsuite: drop flux overlay status from test (#890)
 * mergify: update configuration to replace deprecated keys (#901)
 * ci: rename centos7 and centos8 docker images to el7 and el8 (#904)
 * testsuite: update flux-sharness.sh and add fedora34 (#905)


flux-sched version 0.20.0 - 2021-11-09
--------------------------------------
This version of Fluxion lays a foundation for
node-exclusive scheduling, which will
use a moldable jobspec to match every node-local
resource of each selected compute node.

### Fixes

 * jobspec: make max/operator/operand optional (#879)
 * resource: improve effective max calc performance for operator="+"
   (#882)
 * resource: beefing up min/max count-based matching (#878)

### Cleanup

 * build: drop ax_python_devel.m4 (#881)


flux-sched version 0.19.0 - 2021-10-15
--------------------------------------

This version of Fluxion includes bug fixes for the issues
discovered during system instance testing on an LLNL
cluster.

### Features

 * Add `sched.feasibility` service for flux-core's jobspec feasibility
   validator (#871)

### Fixes

 * queue: add provisional queuing for reprioritization in support
   of async. execution (#862)
 * resource: fix a satisfiabilty bug with backfilling (#859) 
 * testsuite: fix logging test in t7000-shell-datastaging.t (#865)
 * Using static_cast to ensure type compatibility in readers. (#858)
 * Fix an assortment of issues discovered during system instance testing
   on LLNL's Fluke machine (#872)

### Cleanup

 * data-staging: define FLUX_SHELL_PLUGIN_NAME (#864)


flux-sched version 0.18.0 - 2021-09-04
--------------------------------------

This version of Fluxion includes updates required for compatibility
with flux-core v0.29.0.

### Fixes

 * resource: update for libflux API change (#852)

flux-sched version 0.17.0 - 2021-07-01
--------------------------------------

This version re-releases Fluxion under the LGPL-3.0 license.

This version also changes the default match policy to first-match
and the default queue-depth to 32, resulting in further performance
improvements.

### Features

 * LLNS re-release under LGPL-3.0 (#850)
 * python: export FluxionResource classes (#845)
 * Default policy changes (#843)

### Fixes

 * Fix cancel race (#838)
 * bugfix: include missing header (limits.h) (#837)
 * datastaging: add -avoid-version to LDFLAGS (#835)

### Cleanup

 * Replace GNU readline with editline (#849)
 * planner: port to Yggdrasil's augmented red-black library (#846)
 * github: fix ci builds on master (#848)
 * github: fixes for auto-release deployment (#834)


flux-sched version 0.16.0 - 2021-05-05
--------------------------------------

Note: Do you have a need to run high-throughput workloads
at large scale? Check out this version. It combines
our new first-match policy with asynchrous
communications to significantly improve scheduling scalability.

### Features

 * Add async FCFS policy support and other scheduling optimization
   (#826)
 * Add "ultrafast" first-match policy support (#820)
 * CI: Add fedora33 for gcc10 (#825)

### Fixes

 * libczmqcontainers: introduce new internal library of czmq zhashx
   library (#822)
 * Add bug fix for by_subplan pruning (#817)
 * resource: fix 'namespace' spelling (#813)
 * Add minor C++ safety improvements (#809)

### Cleanup

 * sharness: update flux start options (#823)
 * testsuite: fix shell path discovery in datastaging test (#810)


flux-sched version 0.15.0 - 2021-01-25
--------------------------------------

### Features

 * Implement unpack_at for JGF (#775)
 * qmanager: support re-prioritization of jobs (#803)
 * fluxion-R: add flux-ion-R.py as a public command (#800)
 * ci: migrate to github actions (#794)

### Fixes

 * traverser: fix segfault for bi-edge traversal (#775)
 * qmanager: update per libschedutil API changes (#801)
 * mergify: fix referenced test name (#804)

### Cleanup

 * Globally standardize spelling of "canceled" (#797)

flux-sched version 0.14.0 - 2020-12-18
--------------------------------------

Note: In support of fully hierarchical scheduling for our
workflow users, Fluxion is now capable of correctly
discovering and scheduling GPUs at any level of nesting.

### Fixes
 * ci: drop unnecessary flux keygen (#788)
 * shell: reduce logging verbosity of datastaging plugin (#779)
 * t0000-sharness.sh: fix test with debug/verbose set in env (#784)
 * codecov: ignore vendored json.hpp (#774)

### Features
  * use RV1+JGF JSON from resource.acquire to populate graph data store (#787)
  * execution target and core/gpu Id remapping support for nesting (#773, #787)

### Cleanup
  * Globally rename "administrative priority" to "urgency" (#781)

flux-sched version 0.13.0 - 2020-11-05
--------------------------------------

### Fixes

 * expect Rv1 from resource.acquire, use flux resource reload (#766)
 * update configs for travis-ci.com migration (#769)

### Features

 * Add support for nodelist for RV1 change and R_lite compression (#764)
 * Add job shell plugin for data staging on tiered storage systems (#743)

flux-sched version 0.12.0 - 2020-09-30
--------------------------------------

### Fixes

 * qmanager: remove `t_estimate` when job alloc succeeds (#746)
 * rc: avoid unloading sched-simple, which is no longer loaded by default (#749)
 * rc: only load/unload scheduler on rank 0 (#741)
 * makefile: workaround boost errors under latest compilers (#757)

flux-sched version 0.11.0 - 2020-08-31
--------------------------------------

Note: Fluxion can now annotate its jobs with their
start-time estimate and queue info (to be displayed by `flux-jobs`).

### Fixes
 * Handle invalid sibling requests in jobspecs more gracefully (#737)
 * Fix various errors detected when tests are run under valgrind (#734)
 * fluxion-resource: check return value of `find_first_not_of` (#715)

### Features
 * Add support for job annotation including its start-time estimate (#735)
 * Add support for node-local storage to hwloc reader (#714)
 * flux module load returns early for staggered system-instance bring-up (#719)

### Cleanup
 * Update for non-integer jobid encodings (#718)
 * Use version 9999 for test jobspecs (#739)

flux-sched version 0.10.0 - 2020-07-29
--------------------------------------

Note: Fluxion now uses flux-core's resource service
to acquire resources to schedule.

### Fixes
 * qmanager: support for ordering pending jobs by priority/submit time (#694)
 * testsuite: increase wait-event timeouts (#692)
 * testsuite: enable valgrind testing (#703)
 * resource: errno support for std::ifstream open (#698)
 * Undo compiler workarounds (#697)

### Features
 * Add support for hwloc 2.0+ (#677)
 * Augment multi-tiered storage scheduling test (#673)
 * Integrate with flux-core's resource module (#665, #667, #674, #693, #694)
 * Update for flux-core annotation support changes (#681)

### Cleanup
 * Update pkg dependency matrix in README.md (#696)
 * Globally rename whitelist to allowlist (#690)

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

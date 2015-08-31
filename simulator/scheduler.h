/*****************************************************************************\
 *  Copyright (c) 2014 Lawrence Livermore National Security, LLC.  Produced at
 *  the Lawrence Livermore National Laboratory (cf, AUTHORS, DISCLAIMER.LLNS).
 *  LLNL-CODE-658032 All rights reserved.
 *
 *  This file is part of the Flux resource manager framework.
 *  For details, see https://github.com/flux-framework.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the license, or (at your option)
 *  any later version.
 *
 *  Flux is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the terms and conditions of the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *  See also:  http://www.gnu.org/licenses/
\*****************************************************************************/

#ifndef SCHEDULER_H
#define SCHEDULER_H 1

#include <stdint.h>
#include <czmq.h>
#include <flux/core.h>

#include "rdl.h"
#include "simulator.h"

#define MAX_STR_LEN 128
#define SCHED_INTERVAL 30

/**
 *  Enumerates lightweight-job and resource events
 */
typedef enum {
    j_null,       /*!< the state has yet to be assigned */
    j_reserved,   /*!< a job has a reservation in KVS*/
    j_submitted,  /*!< a job added to KVS */
    j_unsched,    /*!< a job never gone through sched_loop */
    j_pending,    /*!< a job set to pending */
    j_allocated,  /*!< a job got allocated to resource */
    j_runrequest, /*!< a job is requested to be executed */
    j_starting,   /*!< a job is starting */
    j_running,    /*!< a job is running */
    j_cancelled,  /*!< a job got cancelled */
    j_complete,   /*!< a job completed */
    j_reaped,     /*!< a job reaped */
    j_for_rent    /*!< Space For Rent */
} lwj_event_e;

typedef lwj_event_e lwj_state_e;

/**
 *  Enumerates resource events
 */
typedef enum {
    r_null,      /*!< the state has yet to be assigned */
    r_added,     /*!< RDL reported some resources added */
    r_released,  /*!< a lwj released some resources */
    r_attempt,   /*!< attemp to schedule again */
    r_failed,    /*!< some resource failed */
    r_recovered, /*!< some failed resources recovered */
    r_for_rent   /*!< Space For Rent */
} res_event_e;

/**
 *  Whether an event is a lwj or resource event
 */
typedef enum {
    lwj_event, /*!< is a lwj event */
    res_event, /*!< is a resource event */
} event_class_e;

/**
 *  Defines resources as provided by RDL.
 *  This is mostly a placeholder
 */
typedef struct {
    void *resource;
} flux_res_t;

/**
 *  Defines resource request info block.
 *  This needs to be expanded as RDL evolves.
 *  TODO: standardize these data types
 */
typedef struct {
    uint64_t nnodes; /*!< num of nodes requested by a job */
    uint32_t ncores; /*!< num of cores requested by a job */
    uint64_t io_rate;  // amount of io bw (in MB) requested by a job
    double walltime;  // amount of time requested by a job
} flux_req_t;

/**
 *  Defines LWJ info block (this needs to be expanded of course)
 */
typedef struct {
    int64_t lwj_id; /*!< LWJ id */
    lwj_state_e state;
    flux_req_t req;   /*!< count of resources requested by this LWJ */
    flux_req_t alloc; /*!< count of resources allocated to this LWJ */
    struct rdl *rdl;  /*!< resources allocated to this LWJ */
} flux_lwj_t;

/**
 *  Defines an event that goes into the event queue
 */
typedef struct {
    event_class_e t; /*!< lwj or res event? */
    union u {
        lwj_event_e je; /*!< use this if the class is lwj */
        res_event_e re; /*!< use this if the class is res */
    } ev;               /*!< event */
    flux_lwj_t *lwj;    /*!< if lwj event, a ref. Don't free */
} flux_event_t;

typedef struct {
    flux_t h;
    zlist_t *p_queue;
    zlist_t *r_queue;
    zlist_t *c_queue;
    zlist_t *ev_queue;
    zlist_t *kvs_queue;
    zlist_t *timer_queue;
    bool in_sim;
    bool run_schedule_loop;
    char *uri;
    struct rdl *rdl;
} ctx_t;

struct stab_struct {
    int i;
    const char *s;
};

typedef struct {
    char *key;
    char *val;
    int errnum;
} kvs_event_t;

extern const char *IDLETAG;
extern const char *CORETYPE;

int send_rdl_update (flux_t h, struct rdl *rdl);
struct rdl *get_free_subset (struct rdl *rdl, const char *type);
int64_t get_free_count (struct rdl *rdl, const char *uri, const char *type);
void remove_job_resources_from_rdl (struct rdl *rdl,
                                    const char *uri,
                                    flux_lwj_t *job);

void trigger_cb (flux_t h,
                 flux_msg_watcher_t *w,
                 const flux_msg_t *msg,
                 void *arg);
void handle_kvs_queue (ctx_t *ctx);

int queue_kvs_cb (const char *key, const char *val, void *arg, int errnum);
void newlwj_rpc (flux_t h,
                 flux_msg_watcher_t *w,
                 const flux_msg_t *msg,
                 void *arg);
int newlwj_cb (const char *key, int64_t val, void *arg, int errnum);
int reg_lwj_state_hdlr (flux_t h, const char *path, kvs_set_string_f func);
int reg_newlwj_hdlr (flux_t h, kvs_set_int64_f func);
int wait_for_lwj_init (flux_t h);

void freectx (void *arg);
ctx_t *getctx (flux_t h);

flux_lwj_t *find_lwj (ctx_t *ctx, int64_t id);
int extract_lwjid (const char *k, int64_t *i);
int extract_lwjinfo (ctx_t *ctx, flux_lwj_t *j);
void issue_lwj_event (ctx_t *ctx, lwj_event_e e, flux_lwj_t *j);
void lwjstate_cb (const char *key, const char *val, void *arg, int errnum);
int signal_event (ctx_t *ctx);

void queue_timer_change (ctx_t *ctx, const char *module);
void handle_timer_queue (ctx_t *ctx, sim_state_t *sim_state);

void set_next_event (const char *module, sim_state_t *sim_state);
void handle_event_queue (ctx_t *ctx);
int issue_res_event (ctx_t *ctx, flux_lwj_t *lwj);
int move_to_r_queue (ctx_t *ctx, flux_lwj_t *lwj);
int move_to_c_queue (ctx_t *ctx, flux_lwj_t *lwj);
int action_j_event (ctx_t *ctx, flux_event_t *e);
int action_r_event (ctx_t *ctx, flux_event_t *e);
int action (ctx_t *ctx, flux_event_t *e);

int request_run (ctx_t *ctx, flux_lwj_t *job);

char *ctime_iso8601_now (char *buf, size_t sz);
int stab_lookup (struct stab_struct *ss, const char *s);
const char *stab_rlookup (struct stab_struct *ss, int i);

int update_job (ctx_t *ctx, flux_lwj_t *job);
int update_job_state (ctx_t *ctx, flux_lwj_t *job, lwj_event_e e);
int update_job_resources (ctx_t *ctx, flux_lwj_t *job);
int update_job_cores (ctx_t *ctx,
                      struct resource *jr,
                      flux_lwj_t *job,
                      uint64_t *pnode,
                      uint32_t *pcores);

int print_resources (struct resource *r);
int idlize_resources (struct resource *r);
int release_resources (ctx_t *ctx,
                       struct rdl *rdl,
                       const char *uri,
                       flux_lwj_t *job);
int release_lwj_resource (ctx_t *ctx,
                          struct rdl *rdl,
                          struct resource *jr,
                          char *lwjtag);
void deallocate_bandwidth (ctx_t *ctx,
                           struct rdl *rdl,
                           const char *uri,
                           flux_lwj_t *job);
void deallocate_resource_bandwidth (ctx_t *ctx,
                                    struct resource *r,
                                    int64_t amount);
void allocate_resource_bandwidth (struct resource *r, int64_t amount);
bool allocate_resources (struct rdl *rdl,
                         const char *hierarchy,
                         struct resource *fr,
                         struct rdl_accumulator *accum,
                         flux_lwj_t *job,
                         zlist_t *ancestors);
int64_t get_avail_bandwidth (struct resource *r);

#if CZMQ_VERSION < CZMQ_MAKE_VERSION(3, 0, 1)
bool job_compare_termination_fn (void *item1, void *item2);
bool job_compare_t (void *item1, void *item2);
#else
int job_compare_termination_fn (void *item1, void *item2);
int job_compare_t (void *item1, void *item2);
#endif

void queue_schedule_loop (ctx_t *ctx);
bool should_run_schedule_loop (ctx_t *ctx, int time);
void end_schedule_loop (ctx_t *ctx);

void start_cb (flux_t h,
               flux_msg_watcher_t *w,
               const flux_msg_t *msg,
               void *arg);

int init_and_start_scheduler (flux_t h,
                              ctx_t *ctx,
                              zhash_t *args,
                              struct flux_msghandler *tab);

int schedule_job (ctx_t *ctx,
                  struct rdl *rdl,
                  struct rdl *free_rdl,
                  const char *uri,
                  int64_t free_cores,
                  flux_lwj_t *job);

bool resources_equal (struct resource *r1, struct resource *r2);

/********
 User-implemented
********/

// Should return the number of jobs scheduled
int schedule_jobs (ctx_t *ctx, double sim_time);
bool allocate_bandwidth (flux_lwj_t *job,
                         struct resource *r,
                         zlist_t *ancestors);

#endif /* SCHEDULER_H */

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

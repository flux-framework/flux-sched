#ifndef _FLUX_SCHED_PLUGIN_H
#define _FLUX_SCHED_PLUGIN_H

#include "scheduler.h"

struct behavior_plugin {
    void         *dso;                /* Scheduler plug-in DSO handle */
    char         *name;               /* Name of plugin */
    char         *path;               /* Path to plugin dso */

    int                  (*sched_loop_setup)(flux_t *h);

    int64_t              (*find_resources)(flux_t *h,
                                           resrc_api_ctx_t *rsapi,
                                           resrc_t *resrc,
                                           resrc_reqst_t *resrc_reqst,
                                           resrc_tree_t **found_tree);

    resrc_tree_t *       (*select_resources)(flux_t *h,
                                             resrc_api_ctx_t *rsapi,
                                             resrc_tree_t *found_tree,
                                             resrc_reqst_t *resrc_reqst,
                                             resrc_tree_t *selected_parent);

    int                  (*allocate_resources)(flux_t *h,
                                               resrc_api_ctx_t *rsapi,
                                               resrc_tree_t *rt,
                                               int64_t job_id,
                                               int64_t starttime,
                                               int64_t endtime);

    int                  (*reserve_resources)(flux_t *h,
                                              resrc_api_ctx_t *rsapi,
                                              resrc_tree_t **selected_tree,
                                              int64_t job_id,
                                              int64_t starttime,
                                              int64_t walltime,
                                              resrc_t *resrc,
                                              resrc_reqst_t *resrc_reqst);

    int                  (*process_args)(flux_t *h,
                                         char *argz,
                                         size_t argz_len,
                                         const sched_params_t *params);
};

struct priority_plugin {
    void         *dso;                /* Scheduler plug-in DSO handle */
    char         *name;               /* Name of plugin */
    char         *path;               /* Path to plugin dso */

    int         (*priority_setup)(flux_t *h);
    void        (*prioritize_jobs)(flux_t *h, zlist_t *jobs);
    int         (*record_job_usage)(flux_t *h, flux_lwj_t *job);
};

/* Create/destroy the plugin loader apparatus.
 */
struct sched_plugin_loader;
struct sched_plugin_loader *sched_plugin_loader_create (flux_t *h);
void sched_plugin_loader_destroy (struct sched_plugin_loader *sploader);

/* Load plugin by name.
 * 'name' may be either a file pathname (contains / chars) or a plugin name
 * like "sched.fifo".
 */
int sched_plugin_load (struct sched_plugin_loader *sploader, const char *name);

/* Unload the currently loaded plugin.
 */
void sched_plugin_unload (struct sched_plugin_loader *sploader);

/* Retrieve the currently loaded plugin.
 * Return NULL if plugin is not loaded.
 */
struct behavior_plugin *behavior_plugin_get (struct sched_plugin_loader
                                             *sploader);
struct priority_plugin *priority_plugin_get (struct sched_plugin_loader
                                             *sploader);

#endif /* !_FLUX_SCHED_PLUGIN_H */

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

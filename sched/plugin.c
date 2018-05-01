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

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <dlfcn.h>
#include <flux/core.h>
#include <czmq.h>

#if HAVE_VALGRIND
# if HAVE_VALGRIND_H
#  include <valgrind.h>
# elif HAVE_VALGRIND_VALGRIND_H
#  include <valgrind/valgrind.h>
# endif
#endif

#include "src/common/libutil/shortjansson.h"
#include "scheduler.h"
#include "plugin.h"

struct sched_plugin_loader {
    flux_t *h;
    flux_msg_handler_t **handlers;
    struct behavior_plugin *behavior_plugin;
    struct priority_plugin *priority_plugin;
};

static void behavior_plugin_destroy (struct behavior_plugin *plugin)
{
    if (plugin) {
        if (plugin->dso)
            dlclose (plugin->dso);
        if (plugin->name)
            free (plugin->name);
        if (plugin->path)
            free (plugin->path);
        free (plugin);
    }
}

static void priority_plugin_destroy (struct priority_plugin *plugin)
{
    if (plugin) {
        if (plugin->dso)
            dlclose (plugin->dso);
        if (plugin->name)
            free (plugin->name);
        if (plugin->path)
            free (plugin->path);
        free (plugin);
    }
}

static struct behavior_plugin *behavior_plugin_create (flux_t *h, void *dso)
{
    int saved_errno;
    char *strerr = NULL;
    struct behavior_plugin *plugin = malloc (sizeof (*plugin));

    if (!plugin) {
        errno = ENOMEM;
        goto error;
    }
    memset (plugin, 0, sizeof (*plugin));
    dlerror (); // Clear old dlerrors

    plugin->get_sched_properties = dlsym (dso, "get_sched_properties");
    strerr = dlerror();
    if (strerr || !plugin->get_sched_properties || !*plugin->get_sched_properties) {
        flux_log (h, LOG_ERR, "can't load get_sched_properties: %s", strerr);
        goto error;
    }
    plugin->sched_loop_setup = dlsym (dso, "sched_loop_setup");
    strerr = dlerror();
    if (strerr || !plugin->sched_loop_setup || !*plugin->sched_loop_setup) {
        flux_log (h, LOG_ERR, "can't load sched_loop_setup: %s", strerr);
        goto error;
    }
    plugin->find_resources = dlsym (dso, "find_resources");
    strerr = dlerror();
    if (strerr || !plugin->find_resources || !*plugin->find_resources) {
        flux_log (h, LOG_ERR, "can't load find_resources: %s", strerr);
        goto error;
    }
    plugin->select_resources = dlsym (dso, "select_resources");
    strerr = dlerror();
    if (strerr || !plugin->select_resources || !*plugin->select_resources) {
        flux_log (h, LOG_ERR, "can't load select_resources: %s", strerr);
        goto error;
    }
    plugin->allocate_resources = dlsym (dso, "allocate_resources");
    strerr = dlerror();
    if (strerr || !plugin->allocate_resources || !*plugin->allocate_resources) {
        flux_log (h, LOG_ERR, "can't load allocate_resources: %s", strerr);
        goto error;
    }
    plugin->reserve_resources = dlsym (dso, "reserve_resources");
    strerr = dlerror();
    if (strerr || !plugin->reserve_resources || !*plugin->reserve_resources) {
        flux_log (h, LOG_ERR, "can't load reserve_resources: %s", strerr);
        goto error;
    }
    plugin->process_args = dlsym (dso, "process_args");
    strerr = dlerror();
    if (strerr || !plugin->process_args || !*plugin->process_args) {
        flux_log (h, LOG_ERR, "can't load process_args: %s", strerr);
        goto error;
    }
    plugin->dso = dso;
    return plugin;
error:
    saved_errno = errno;
    if (plugin)
        free (plugin);
    errno = saved_errno;
    return NULL;
}

static struct priority_plugin *priority_plugin_create (flux_t *h, void *dso)
{
    int saved_errno;
    char *strerr = NULL;
    struct priority_plugin *plugin = malloc (sizeof (*plugin));

    if (!plugin) {
        errno = ENOMEM;
        goto error;
    }
    memset (plugin, 0, sizeof (*plugin));
    dlerror (); // Clear old dlerrors

    plugin->priority_setup = dlsym (dso, "sched_priority_setup");
    strerr = dlerror();
    if (strerr || !plugin->priority_setup || !*plugin->priority_setup) {
        flux_log (h, LOG_ERR, "can't load priority_setup: %s", strerr);
        goto error;
    }
    plugin->prioritize_jobs = dlsym (dso, "sched_priority_prioritize_jobs");
    strerr = dlerror();
    if (strerr || !plugin->prioritize_jobs || !*plugin->prioritize_jobs) {
        flux_log (h, LOG_ERR, "can't load prioritize_jobs: %s", strerr);
        goto error;
    }
    plugin->record_job_usage = dlsym (dso, "sched_priority_record_job_usage");
    strerr = dlerror();
    if (strerr || !plugin->record_job_usage || !*plugin->record_job_usage) {
        flux_log (h, LOG_ERR, "can't load record_job_usage: %s", strerr);
        goto error;
    }
    plugin->dso = dso;
    return plugin;
error:
    saved_errno = errno;
    if (plugin)
        free (plugin);
    errno = saved_errno;
    return NULL;
}

void behavior_plugin_unload (struct sched_plugin_loader *sploader)
{
    if (sploader->behavior_plugin) {
        behavior_plugin_destroy (sploader->behavior_plugin);
        sploader->behavior_plugin = NULL;
    }
}

void priority_plugin_unload (struct sched_plugin_loader *sploader)
{
    if (sploader->priority_plugin) {
        priority_plugin_destroy (sploader->priority_plugin);
        sploader->priority_plugin = NULL;
    }
}

int sched_plugin_load (struct sched_plugin_loader *sploader, const char *s)
{
    char *path = NULL;
    char *name = NULL;
    char *searchpath = getenv ("FLUX_MODULE_PATH");
    void *dso = NULL;

    if (!searchpath) {
        flux_log (sploader->h, LOG_ERR, "FLUX_MODULE_PATH not set");
        goto error;
    }
    if (strchr (s, '/')) {
        if (!(name = flux_modname (s))) {
            flux_log (sploader->h, LOG_ERR, "%s: %s", s, dlerror ());
            errno = ENOENT;
            goto error;
        }
        if (!(path = strdup (s))) {
            errno = ENOMEM;
            goto error;
        }
    } else {
        if (!(path = flux_modfind (searchpath, s))) {
            flux_log (sploader->h, LOG_ERR,
                      "%s: not found in module search path %s", s, searchpath);
            goto error;
        }
        if (!(name = flux_modname (path)))
            goto error;
    }
    if (!(dso = dlopen (path, RTLD_NOW | RTLD_LOCAL | RTLD_DEEPBIND))) {
        flux_log (sploader->h, LOG_ERR, "failed to open sched plugin: %s",
                  dlerror ());
        goto error;
    }
    flux_log (sploader->h, LOG_DEBUG, "loaded: %s", name);

    dlerror (); // Clear old dlerrors
    dlsym (dso, "sched_loop_setup");
    if (dlerror()) {
        // It is not a behavior plugin so assume it is the priority plugin
        dlerror (); // Clear old dlerrors
        if (sploader->priority_plugin) {
            errno = EEXIST;
            goto error;
        }
        if (!(sploader->priority_plugin = priority_plugin_create (sploader->h,
                                                              dso))) {
            dlclose (dso);
            goto error;
        }
        sploader->priority_plugin->name = name;
        sploader->priority_plugin->path = path;
    } else {
        if (sploader->behavior_plugin) {
            errno = EEXIST;
            goto error;
        }
        if (!(sploader->behavior_plugin = behavior_plugin_create (sploader->h, dso))) {
            dlclose (dso);
            goto error;
        }
        sploader->behavior_plugin->name = name;
        sploader->behavior_plugin->path = path;
    }
    return 0;
error:
    if (path)
        free (path);
    if (name)
        free (name);
    return -1;
}

struct behavior_plugin *behavior_plugin_get (struct sched_plugin_loader *sploader)
{
    return sploader->behavior_plugin;
}

struct priority_plugin *priority_plugin_get (struct sched_plugin_loader
                                             *sploader)
{
    return sploader->priority_plugin;
}

static void rmmod_cb (flux_t *h, flux_msg_handler_t *w,
                      const flux_msg_t *msg, void *arg)
{
    struct sched_plugin_loader *sploader = arg;
    struct behavior_plugin *behavior_plugin = behavior_plugin_get (sploader);
    struct priority_plugin *priority_plugin = priority_plugin_get (sploader);
    const char *json_str;
    char *name = NULL;
    int rc = -1;

    if (flux_request_decode (msg, NULL, &json_str) < 0)
        goto done;
    if (!json_str) {
        errno = EPROTO;
        goto done;
    }
    if (flux_rmmod_json_decode (json_str, &name) < 0)
        goto done;

    if ((behavior_plugin) && (strcmp (name, behavior_plugin->name) == 0)) {
        behavior_plugin_unload (sploader);
        flux_log (h, LOG_INFO, "%s unloaded", name);
        rc = 0;
    }
    if ((priority_plugin) && (strcmp (name, priority_plugin->name) == 0)) {
        priority_plugin_unload (sploader);
        flux_log (h, LOG_INFO, "%s unloaded", name);
        rc = 0;
    }
    if (rc)
        errno = ENOENT;
done:
    if (flux_respond (h, msg, rc < 0 ? errno : 0, NULL) < 0)
        flux_log_error (h, "%s: flux_respond", __FUNCTION__);
    if (name)
        free (name);
}

static void insmod_cb (flux_t *h, flux_msg_handler_t *w,
                       const flux_msg_t *msg, void *arg)
{
    struct sched_plugin_loader *sploader = arg;
    const sched_params_t *sp = sched_params_get (h);
    const char *json_str;
    char *path = NULL;
    char *argz = NULL;
    size_t argz_len = 0;
    int rc = -1;

    if (flux_request_decode (msg, NULL, &json_str) < 0)
        goto done;
    if (!json_str) {
        errno = EPROTO;
        goto done;
    }
    if (flux_insmod_json_decode (json_str, &path, &argz, &argz_len) < 0)
        goto done;
    if (sched_plugin_load (sploader, path) < 0)
        goto done;
    if (argz && sploader->behavior_plugin->process_args (sploader->h, argz,
                                                      argz_len, sp) < 0) {
        goto done;
    }
    rc = 0;

done:
    if (flux_respond (h, msg, rc < 0 ? errno : 0, NULL) < 0)
        flux_log_error (h, "%s: flux_respond", __FUNCTION__);
    if (path)
        free (path);
    if (argz)
        free (argz);
}

static void lsmod_cb (flux_t *h, flux_msg_handler_t *w,
                      const flux_msg_t *msg, void *arg)
{
    struct sched_plugin_loader *sploader = arg;
    struct behavior_plugin *behavior_plugin = behavior_plugin_get (sploader);
    struct priority_plugin *priority_plugin = priority_plugin_get (sploader);
    flux_modlist_t *mods = NULL;
    zfile_t *zf = NULL;
    char *json_str = NULL;
    struct stat sb;
    int rc = -1;

    if (flux_request_decode (msg, NULL, NULL) < 0)
        goto done;
    if (!(mods = flux_modlist_create ()))
        goto done;
    if (behavior_plugin) {
        if (stat (behavior_plugin->path, &sb) < 0)
            goto done;
        if (!(zf = zfile_new (NULL, behavior_plugin->path)))
            goto done;
        if (flux_modlist_append (mods, behavior_plugin->name, sb.st_size,
                                 zfile_digest (zf),
                                 0, FLUX_MODSTATE_RUNNING) < 0)
            goto done;
    }
    if (priority_plugin) {
        if (stat (priority_plugin->path, &sb) < 0)
            goto done;
        if (!(zf = zfile_new (NULL, priority_plugin->path)))
            goto done;
        if (flux_modlist_append (mods, priority_plugin->name, sb.st_size,
                                 zfile_digest (zf),
                                 0, FLUX_MODSTATE_RUNNING) < 0)
            goto done;
    }
    if (!(json_str = flux_lsmod_json_encode (mods)))
        goto done;
    rc = 0;
done:
    if (flux_respond (h, msg, rc < 0 ? errno : 0,
                              rc < 0 ? NULL : json_str) < 0)
        flux_log_error (h, "%s: flux_respond", __FUNCTION__);
    if (mods)
        flux_modlist_destroy (mods);
    zfile_destroy (&zf);
    if (json_str)
        free (json_str);
}


static const struct flux_msg_handler_spec plugin_htab[] = {
    { FLUX_MSGTYPE_REQUEST, "sched.insmod",         insmod_cb, 0 },
    { FLUX_MSGTYPE_REQUEST, "sched.rmmod",          rmmod_cb, 0 },
    { FLUX_MSGTYPE_REQUEST, "sched.lsmod",          lsmod_cb, 0 },
    FLUX_MSGHANDLER_TABLE_END,
};

struct sched_plugin_loader *sched_plugin_loader_create (flux_t *h)
{
    struct sched_plugin_loader *sploader = malloc (sizeof (*sploader));
    if (!sploader) {
        errno = ENOMEM;
        return NULL;
    }
    memset (sploader, 0, sizeof (*sploader));
    sploader->h = h;
    if (flux_msg_handler_addvec (h, plugin_htab, sploader,
                                 &sploader->handlers) < 0) {
        flux_log_error (h, "flux_msghandler_addvec");
        free (sploader);
        return NULL;
    }
    return sploader;
}

void sched_plugin_loader_destroy (struct sched_plugin_loader *sploader)
{
    if (sploader) {
        behavior_plugin_unload (sploader);
        priority_plugin_unload (sploader);
        flux_msg_handler_delvec (sploader->handlers);
        free (sploader);
    }
}

#if HAVE_VALGRIND
/* Disable dlclose() during valgrind operation
 */
void I_WRAP_SONAME_FNNAME_ZZ(Za,dlclose)(void *dso) {}
#endif

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

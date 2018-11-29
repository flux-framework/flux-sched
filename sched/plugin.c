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
#include <argz.h>

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

static void module_dlerror (const char *errmsg, void *arg)
{
    flux_t *h = arg;
    flux_log (h, LOG_DEBUG, "flux_modname: %s", errmsg);
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
        if (!(name = flux_modname (s, module_dlerror, sploader->h))) {
            flux_log (sploader->h, LOG_ERR, "%s: %s", s, dlerror ());
            errno = ENOENT;
            goto error;
        }
        if (!(path = strdup (s))) {
            errno = ENOMEM;
            goto error;
        }
    } else {
        if (!(path = flux_modfind (searchpath, s,
                                   module_dlerror, sploader->h))) {
            flux_log (sploader->h, LOG_ERR,
                      "%s: not found in module search path %s", s, searchpath);
            goto error;
        }
        if (!(name = flux_modname (path, module_dlerror, sploader->h)))
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
    const char *name;
    bool found = false;

    if (flux_request_unpack (msg, NULL, "{s:s}", "name", &name) < 0)
        goto error;
    if ((behavior_plugin) && (strcmp (name, behavior_plugin->name) == 0)) {
        behavior_plugin_unload (sploader);
        flux_log (h, LOG_INFO, "%s unloaded", name);
        found = true;
    }
    if ((priority_plugin) && (strcmp (name, priority_plugin->name) == 0)) {
        priority_plugin_unload (sploader);
        flux_log (h, LOG_INFO, "%s unloaded", name);
        found = true;
    }
    if (!found) {
        errno = ENOENT;
        goto error;
    }
    if (flux_respond (h, msg, 0, NULL) < 0)
        flux_log_error (h, "%s: flux_respond", __FUNCTION__);
    return;
error:
    if (flux_respond_error (h, msg, errno, NULL) < 0)
        flux_log_error (h, "%s: flux_respond_error", __FUNCTION__);
}

static void insmod_cb (flux_t *h, flux_msg_handler_t *w,
                       const flux_msg_t *msg, void *arg)
{
    struct sched_plugin_loader *sploader = arg;
    const sched_params_t *sp = sched_params_get (h);
    json_t *args;
    size_t index;
    json_t *value;
    const char *path = NULL;
    char *argz = NULL;
    size_t argz_len = 0;
    error_t e;

    if (flux_request_unpack (msg, NULL, "{s:s s:o}", "path", &path,
                                                     "args", &args) < 0)
        goto error;
    if (!json_is_array (args))
        goto proto;
    json_array_foreach (args, index, value) {
        if (!json_is_string (value))
            goto proto;
        if ((e = argz_add (&argz, &argz_len, json_string_value (value)))) {
            errno = e;
            goto error;
        }
    }
    if (sched_plugin_load (sploader, path) < 0)
        goto error;
    if (argz && sploader->behavior_plugin->process_args (sploader->h, argz,
                                                         argz_len, sp) < 0) {
        goto error;
    }
    if (flux_respond (h, msg, 0, NULL) < 0)
        flux_log_error (h, "%s: flux_respond", __FUNCTION__);
    free (argz);
    return;
proto:
    errno = EPROTO;
error:
    if (flux_respond_error (h, msg, errno, NULL) < 0)
        flux_log_error (h, "%s: flux_respond_error", __FUNCTION__);
    free (argz);
}

/* For plugin 'name' at 'path', append an RFC 5 lsmod module record to
 * the JSON array 'mods'.  Return 0 on success, -1 on failure with errno set.
 */
static int lsmod_plugin_append (const char *name, const char *path,
                                json_t *mods)
{
    zfile_t *zf;
    struct stat sb;
    json_t *entry;

    if (stat (path, &sb) < 0)
        return -1;
    if (!(zf = zfile_new (NULL, path)))
        return -1;
    entry = json_pack ("{s:s s:i s:s s:i s:i s:[]}",
                       "name", name,
                       "size", sb.st_size,
                       "digest", zfile_digest (zf),
                       "idle", 0,
                       "status", FLUX_MODSTATE_RUNNING,
                       "services");
    zfile_destroy (&zf);
    if (!entry)
        goto nomem;
    if (json_array_append_new (mods, entry) < 0) {
        json_decref (entry);
        goto nomem;
    }
    return 0;
nomem:
    errno = ENOMEM;
    return -1;
}

static void lsmod_cb (flux_t *h, flux_msg_handler_t *w,
                      const flux_msg_t *msg, void *arg)
{
    struct sched_plugin_loader *sploader = arg;
    struct behavior_plugin *behavior_plugin = behavior_plugin_get (sploader);
    struct priority_plugin *priority_plugin = priority_plugin_get (sploader);
    json_t *mods = NULL;

    if (flux_request_decode (msg, NULL, NULL) < 0)
        goto error;
    if (!(mods = json_array ()))
        goto nomem;
    if (priority_plugin) {
        if (lsmod_plugin_append (priority_plugin->name,
                                 priority_plugin->path, mods) < 0)
            goto error;
    }
    if (behavior_plugin) {
        if (lsmod_plugin_append (behavior_plugin->name,
                                 behavior_plugin->path, mods) < 0)
            goto error;
    }
    if (flux_respond_pack (h, msg, "{s:O}", "mods", mods) < 0)
        flux_log_error (h, "%s: flux_respond_pack", __FUNCTION__);
    json_decref (mods);
    return;
nomem:
    errno = ENOMEM;
error:
    if (flux_respond_error (h, msg, errno, NULL) < 0)
        flux_log_error (h, "%s: flux_respond_error", __FUNCTION__);
    json_decref (mods);
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

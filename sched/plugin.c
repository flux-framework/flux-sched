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

#include "src/common/libutil/shortjson.h"
#include "scheduler.h"
#include "plugin.h"

struct sched_plugin_loader {
    flux_t h;
    struct sched_plugin *plugin;
};

static void plugin_destroy (struct sched_plugin *plugin)
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

static struct sched_plugin *plugin_create (flux_t h, void *dso)
{
    int saved_errno;
    char *strerr = NULL;
    struct sched_plugin *plugin = malloc (sizeof (*plugin));

    if (!plugin) {
        errno = ENOMEM;
        goto error;
    }
    memset (plugin, 0, sizeof (*plugin));
    dlerror (); // Clear old dlerrors

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

void sched_plugin_unload (struct sched_plugin_loader *sploader)
{
    if (sploader->plugin) {
        plugin_destroy (sploader->plugin);
        sploader->plugin = NULL;
    }
}

int sched_plugin_load (struct sched_plugin_loader *sploader, const char *s)
{
    char *path = NULL;
    char *name = NULL;
    char *searchpath = getenv ("FLUX_MODULE_PATH");
    void *dso = NULL;

    if (sploader->plugin) {
        errno = EEXIST;
        goto error;
    }
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
    if (!(dso = dlopen (path, RTLD_NOW | RTLD_LOCAL))) {
        flux_log (sploader->h, LOG_ERR, "failed to open sched plugin: %s",
                  dlerror ());
        goto error;
    }
    flux_log (sploader->h, LOG_DEBUG, "loaded: %s", name);
    if (!(sploader->plugin = plugin_create (sploader->h, dso))) {
        dlclose (dso);
        goto error;
    }
    sploader->plugin->name = name;
    sploader->plugin->path = path;
    return 0;
error:
    if (path)
        free (path);
    if (name)
        free (name);
    return -1;
}

struct sched_plugin *sched_plugin_get (struct sched_plugin_loader *sploader)
{
    return sploader->plugin;
}

static void rmmod_cb (flux_t h, flux_msg_handler_t *w,
                      const flux_msg_t *msg, void *arg)
{
    struct sched_plugin_loader *sploader = arg;
    struct sched_plugin *plugin = sched_plugin_get (sploader);
    const char *json_str;
    char *name = NULL;
    int rc = -1;

    if (flux_request_decode (msg, NULL, &json_str) < 0)
        goto done;
    if (flux_rmmod_json_decode (json_str, &name) < 0)
        goto done;
    if (!plugin || strcmp (name, plugin->name) != 0) {
        errno = ENOENT;
        goto done;
    }
    sched_plugin_unload (sploader);
    flux_log (h, LOG_INFO, "%s unloaded", name);
    rc = 0;
done:
    if (flux_respond (h, msg, rc < 0 ? errno : 0, NULL) < 0)
        flux_log_error (h, "%s: flux_respond", __FUNCTION__);
    if (name)
        free (name);
}

static void insmod_cb (flux_t h, flux_msg_handler_t *w,
                       const flux_msg_t *msg, void *arg)
{
    struct sched_plugin_loader *sploader = arg;
    struct sched_plugin *plugin = sched_plugin_get (sploader);
    const sched_params_t *sp = sched_params_get (h);
    const char *json_str;
    char *path = NULL;
    char *argz = NULL;
    size_t argz_len = 0;
    int rc = -1;

    if (flux_request_decode (msg, NULL, &json_str) < 0)
        goto done;
    if (flux_insmod_json_decode (json_str, &path, &argz, &argz_len) < 0)
        goto done;
    if (plugin) {
        errno = EEXIST;
        goto done;
    }
    if (sched_plugin_load (sploader, path) < 0)
        goto done;

    if (sploader->plugin->process_args (sploader->h, argz, argz_len, sp) < 0) {
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

static void lsmod_cb (flux_t h, flux_msg_handler_t *w,
                      const flux_msg_t *msg, void *arg)
{
    struct sched_plugin_loader *sploader = arg;
    struct sched_plugin *plugin = sched_plugin_get (sploader);
    flux_modlist_t *mods = NULL;
    zfile_t *zf = NULL;
    char *json_str = NULL;
    struct stat sb;
    int rc = -1;

    if (flux_request_decode (msg, NULL, NULL) < 0)
        goto done;
    if (!(mods = flux_modlist_create ()))
        goto done;
    if (plugin) {
        if (stat (plugin->path, &sb) < 0)
            goto done;
        if (!(zf = zfile_new (NULL, plugin->path)))
            goto done;
        if (flux_modlist_append (mods, plugin->name, sb.st_size,
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


static struct flux_msg_handler_spec plugin_htab[] = {
    { FLUX_MSGTYPE_REQUEST, "sched.insmod",         insmod_cb },
    { FLUX_MSGTYPE_REQUEST, "sched.rmmod",          rmmod_cb },
    { FLUX_MSGTYPE_REQUEST, "sched.lsmod",          lsmod_cb },
    FLUX_MSGHANDLER_TABLE_END,
};

struct sched_plugin_loader *sched_plugin_loader_create (flux_t h)
{
    struct sched_plugin_loader *sploader = malloc (sizeof (*sploader));
    if (!sploader) {
        errno = ENOMEM;
        return NULL;
    }
    memset (sploader, 0, sizeof (*sploader));
    sploader->h = h;
    if (flux_msg_handler_addvec (h, plugin_htab, sploader) < 0) {
        flux_log_error (h, "flux_msghandler_addvec");
        free (sploader);
        return NULL;
    }
    return sploader;
}

void sched_plugin_loader_destroy (struct sched_plugin_loader *sploader)
{
    if (sploader) {
        sched_plugin_unload (sploader);
        flux_msg_handler_delvec (plugin_htab);
        free (sploader);
    }
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

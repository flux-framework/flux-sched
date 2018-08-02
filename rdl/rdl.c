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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <dlfcn.h>

#include <jansson.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "rdl.h"
#include "src/common/liblsd/list.h"
#include "src/common/libutil/xzmalloc.h"
#include "src/common/libutil/shortjansson.h"

#include "jansson-lua.h"

#define VERR(r,args...) (*((r)->errf)) ((r)->errctx, args)

static rdl_err_f default_err_f = NULL;
static void *    default_err_ctx = NULL;

struct rdllib {
    lua_State *L;          /*  Global lua state                        */
    rdl_err_f  errf;       /*  Error/debug function                    */
    void *     errctx;     /*  ctx passed to error/debug function      */
    List       rdl_list;   /*  List of rdl db instances                */
};

/*
 *  Single RDL instance.
 */
struct rdl {
    struct rdllib *rl;     /*  Pointer back to rdllib owning instance  */
    lua_State *L;          /*  Pointer back to main Lua state          */
    int        env_ref;    /*  Env Reference in global Lua registry    */
    List       resource_list;  /*  List of active resource references  */
};

/*
 *  Handle to a resource representation inside an rdl instance
 */
struct resource {
    struct rdl *rdl;       /*  Reference back to 'owning' rdl instance  */
    int         lua_ref;   /*  Lua reference into  rdl->L globals table */
    char *      basename;  /*  Copy of resource basename (on first use) */
    char *      name;      /*  Copy of resource name (on first use)     */
    char *      path;      /*  Copy of resource path (on first use)     */
};

/***************************************************************************
 *  Static functions
 ***************************************************************************/

static void verr (void *ctx, const char *fmt, ...)
{
    va_list ap;
    va_start (ap, fmt);
    vfprintf (stderr, fmt, ap);
    va_end (ap);
}

void lsd_fatal_error (char *file, int line, char *msg)
{
    verr (NULL, msg);
    exit (1);
}

void * lsd_nomem_error (char *file, int line, char *msg)
{
    verr (NULL, "Out of memory: %s: %s:%d\n", msg, file, line);
    return NULL;
}

void rdllib_close (struct rdllib *rl)
{
    if (rl == NULL)
        return;
    if (rl->rdl_list)
        list_destroy (rl->rdl_list);
    if (rl->L)
        lua_close (rl->L);
    free (rl);
}

int rtld_global_liblua (struct rdllib *rl)
{
    if (!dlopen (LIBLUA_SO, RTLD_NOW|RTLD_GLOBAL)) {
        VERR (rl, "dlopen (%s) failed: %s", LIBLUA_SO, dlerror ());
        return (-1);
    }
    return (0);
}

static int rdllib_init (struct rdllib *rl)
{
    int status;
    lua_State *L = rl->L;

    /* force liblua global symbols */
    if (rtld_global_liblua (rl) < 0)
        return (-1);

    lua_getglobal (L, "require");
    lua_pushstring (L, "RDL");

    status = lua_pcall (L, 1, LUA_MULTRET, 0);
    if ((status != 0) && !lua_isnil (L, -1)) {
        VERR (rl, "Failed to load RDL: %s\n", lua_tostring (rl->L, -1));
        return (-1);
    }
    if (!lua_istable (L, -1)) {
        VERR (rl, "Failed to load RDL: %s\n", lua_tostring (rl->L, -1));
        return (-1);
    }
    /*
     *  Assign implementation to global RDL table.
     */
    lua_setglobal (rl->L, "RDL");

    lua_settop (rl->L, 0);
    return (0);
}

static int ptrcmp (void *x, void *y)
{
    return (x == y);
}

static void rdllib_rdl_delete (struct rdllib *l, struct rdl *rdl)
{
    if (l->rdl_list)
        list_delete_all (l->rdl_list, (ListFindF) ptrcmp, rdl);
}

static void rdl_free (struct rdl *rdl)
{
   if (rdl == NULL)
        return;
    if (rdl->resource_list)
        list_destroy (rdl->resource_list);
    if (rdl->L && rdl->rl && rdl->rl->L) {
        /*
         *  unref the globals table for this rdl
         */
        luaL_unref (rdl->rl->L, LUA_REGISTRYINDEX, rdl->env_ref);
        lua_gc (rdl->rl->L, LUA_GCCOLLECT, 0);

        /*
         *  Only call lua_close() on global/main rdllib Lua state:
         */
        rdl->L = NULL;
        rdl->rl = NULL;
    }
    free (rdl);
}

struct rdllib * rdllib_open (void)
{
    struct rdllib *rl = malloc (sizeof (*rl));
    if (rl == NULL)
        return NULL;

    rl->L = luaL_newstate ();
    if (rl->L == NULL) {
        rdllib_close (rl);
        return NULL;
    }

    luaL_openlibs (rl->L);
    rl->errf = default_err_f ? default_err_f : &verr;
    rl->errctx = default_err_ctx;

    rl->rdl_list = list_create ((ListDelF) rdl_free);
    if (rl->rdl_list == NULL) {
        rdllib_close (rl);
        return (NULL);
    }

    if (rdllib_init (rl) < 0)
        return NULL;

    return (rl);
}

int rdllib_set_errf (struct rdllib *l, void *ctx, rdl_err_f fn)
{
    l->errf = fn;
    l->errctx = ctx;
    return (0);
}

void rdllib_set_default_errf (void *ctx, rdl_err_f fn)
{
    default_err_ctx = ctx;
    default_err_f = fn;
}

void rdl_destroy (struct rdl *rdl)
{
    if (rdl && rdl->rl)
        rdllib_rdl_delete (rdl->rl, rdl);
    else
        rdl_free (rdl);
}

int rdl_lua_setfenv (struct rdl *rdl)
{
    lua_rawgeti (rdl->L, LUA_REGISTRYINDEX, rdl->env_ref);
#if LUA_VERSION_NUM >= 502
    /* 5.2 and greater: set table as first upvalue: i.e. _ENV */
    return (lua_setupvalue (rdl->L, -2, 1) != NULL ? 0 : -1);
#else
    /* 5.0, 5.1: Set table as function environment for the chunk */
    return (lua_setfenv (rdl->L, -2) != 0 ? 0: -1);
#endif
}

static int rdl_dostringf (struct rdl *rdl, const char *fmt, ...)
{
    char *s;
    int rc;
    int top;

    va_list ap;
    va_start (ap, fmt);
    rc = vasprintf (&s, fmt, ap);
    va_end (ap);

    if (rc < 0)
        return (-1);

    top = lua_gettop (rdl->L);

    if (luaL_loadstring (rdl->L, s)) {
        VERR (rdl->rl, "loadstring (%s) failed\n", s);
        return (-1);
    }

    if (rdl_lua_setfenv (rdl)) {
        VERR (rdl->rl, "setfenv failed\n");
        return (-1);
    }

    if (lua_pcall (rdl->L, 0, LUA_MULTRET, 0)) {
        VERR (rdl->rl, "dostring (%s): %s\n", s, lua_tostring (rdl->L, -1));
        lua_settop (rdl->L, 0);
        free (s);
        return (-1);
    }
    free (s);
    return (lua_gettop (rdl->L) - top);
}

static void rdl_resource_delete (struct rdl *rdl, struct resource *r)
{
    if (!rdl->resource_list)
        return;
    list_delete_all (rdl->resource_list, (ListFindF) ptrcmp, r);
}

static void rdl_resource_destroy_nolist (struct resource *r)
{
    if (r->rdl->L) {
        luaL_unref (r->rdl->L, LUA_REGISTRYINDEX, r->lua_ref);
        r->rdl = NULL;
    }
    free (r->basename);
    free (r->name);
    free (r->path);
    free (r);
}

void rdl_resource_destroy (struct resource *r)
{
    if (r->rdl && r->rdl->resource_list)
        rdl_resource_delete (r->rdl, r);
    else
        rdl_resource_destroy_nolist (r);
}

/* Return a reference to an empty table that shadows the lua state's
 *  global table
 */
static int shadow_global_table_ref (lua_State *L)
{
    lua_newtable (L);
    lua_newtable (L);
    lua_pushstring (L, "__index");
    lua_getglobal (L, "_G");
    lua_rawset (L, -3);
    lua_setmetatable (L, -2);
    return (luaL_ref (L, LUA_REGISTRYINDEX));
}

/*
 *  Allocate a new RDL instance under library state [rl].
 */
static struct rdl * rdl_new (struct rdllib *rl)
{
    struct rdl * rdl = malloc (sizeof (*rdl));
    if (rdl == NULL)
        return NULL;

    /* Pointer back to 'library'instance */
    rdl->rl = rl;
    rdl->L  = rl->L;
    rdl->resource_list = list_create ((ListDelF) rdl_resource_destroy_nolist);

    /*
     *  Link this rdl to rdllib rdl list:
     */
    list_append (rl->rdl_list, rdl);

    if ((rdl->env_ref = shadow_global_table_ref (rdl->L)) < 0) {
        rdl_destroy (rdl);
        return (NULL);
    }

    return (rdl);
}

/*  set the value at the top of the stack under name in the rdl
 *   "globals" table.
 */
static void lua_rdl_setglobal (struct rdl *rdl, const char *name)
{
    /* Push globals table: [v, t] */
    lua_rawgeti (rdl->L, LUA_REGISTRYINDEX, rdl->env_ref);
    /* Push key    [v, t, k]*/
    lua_pushstring (rdl->L, name);
    /* Push value  [v, t, k, v ]*/
    lua_pushvalue (rdl->L, -3);
    /* t[k] = v    [v, t] */
    lua_rawset (rdl->L, -3);
    /* pop table and original value [] */
    lua_pop (rdl->L, 2);
}

/*
 *  Set the current RDL table at top of stack as rdl
 *   in new struct rdl.
 */
static struct rdl * lua_pop_new_rdl (struct rdl *from)
{
    struct rdl *to;
    /*
     *  Ensure item at top of stack is at least a table:
     */
    if (lua_type (from->L, -1) != LUA_TTABLE) {
        return (NULL);
    }
    /*
     *  Create a new rdl object within this library state:
     */
    to = rdl_new (from->rl);
    lua_rdl_setglobal (to, "rdl");

    return (to);
}

static struct rdl * loadfn (struct rdllib *rl, const char *fn, const char *s)
{
    int rc;
    struct rdl * rdl = rdl_new (rl);
    if (rdl == NULL)
        return NULL;

    rc = rdl_dostringf (rdl, "rdl = RDL.%s ('%s'); return rdl", fn, s);
    if (rc <= 0) {
        VERR (rl, "rdl_load: Failed to get function RDL.%s: %s\n", fn, lua_tostring (rdl->L, -1));
        rdl_destroy (rdl);
        return (NULL);
    }
    if (lua_type (rdl->L, -1) != LUA_TTABLE) {
        VERR (rl, "rdl_load: %s\n", lua_tostring (rdl->L, -1));
        rdl_destroy (rdl);
        return (NULL);
    }
    lua_settop (rdl->L, 0);
    return (rdl);
}

struct rdl * rdl_loadfile (struct rdllib *rl, const char *file)
{
    return loadfn (rl, "evalf", file);
}

struct rdl * rdl_load (struct rdllib *rl, const char *s)
{
    return loadfn (rl, "eval", s);
}


struct rdl * rdl_copy (struct rdl *rdl)
{
    if (rdl == NULL)
        return (NULL);
    assert (rdl->L);
    assert (rdl->rl);
    lua_settop (rdl->L, 0);
    /*
     *  Call memstore:dup() function to push copy of current rdl
     *   onto stack:
     */
    rdl_dostringf (rdl, "return (rdl:dup())");
    return (lua_pop_new_rdl (rdl));
}

static int lua_rdl_push (struct rdl *rdl)
{
    //lua_rawgeti (rdl->L, LUA_GLOBALSINDEX, rdl->lua_ref);
    rdl_dostringf (rdl, "return rdl");
    return (1);
}

static int lua_rdl_method_push (struct rdl *rdl, const char *name)
{
    lua_State *L = rdl->L;
    lua_settop (L, 0);
    /*
     *  First push rdl object onto stack
     */
    lua_rdl_push (rdl);
    lua_getfield (L, -1, name);
    if (lua_type (L, -1) != LUA_TFUNCTION) {
        lua_pushnil (L);
        lua_pushstring (L, "not a method");
        return (-1);
    }

    /*
     *  Push rdl reference again as first argument to "Method"
     */
    lua_rdl_push (rdl);
    return (0);
}

const char *rdl_next_hierarchy (struct rdl *rdl, const char *last)
{
    lua_rdl_method_push (rdl, "hierarchy_next");

    if (last)
        lua_pushstring (rdl->L, last);
    else
        lua_pushnil (rdl->L);

    /* stack: [ Method, object, last ] */ if (lua_pcall (rdl->L, 2, LUA_MULTRET, 0)) {
        VERR (rdl->rl, "next_hierarchy: %s\n", lua_tostring (rdl->L, -1));
        return (NULL);
    }
    if (lua_isnil (rdl->L, -1)) {
        /* End of child list is indicated by nil return */
        return (NULL);
    }
    return (lua_tostring (rdl->L, -1));
}


struct rdl * rdl_find (struct rdl *rdl, json_t *args)
{
    lua_rdl_method_push (rdl, "find");

    if (json_object_to_lua (rdl->L, args) < 0) {
        VERR (rdl->rl, "Failed to convert JSON to Lua\n");
        return (NULL);
    }
    /*
     *  stack: [ Method, object, args-table ]
     */
    if (lua_pcall (rdl->L, 2, LUA_MULTRET, 0) || lua_isnoneornil (rdl->L, 1)) {
        char *s = Jtostr (args);
        VERR (rdl->rl, "find(%s): %s\n",
                s,
                lua_tostring (rdl->L, -1));
        free (s);
        return (NULL);
    }

    return (lua_pop_new_rdl (rdl));
}

char * rdl_serialize (struct rdl *rdl)
{
    char *s;
    if (rdl == NULL)
        return (NULL);
    assert (rdl->L);
    assert (rdl->rl);

    rdl_dostringf (rdl, "return rdl:serialize()");
    s = xasprintf ("%s\n%s", "-- RDL v1.0", lua_tostring (rdl->L, -1));
    lua_settop (rdl->L, 0);
    return s;
}

static struct resource * create_resource_ref (struct rdl *rdl, int index)
{
    struct resource *r;
    r = malloc (sizeof (*r));
    r->lua_ref = luaL_ref (rdl->L, LUA_REGISTRYINDEX);
    r->rdl = rdl;
    r->path = NULL;
    r->basename = NULL;
    r->name = NULL;
    list_append (rdl->resource_list, r);
    return (r);
}

struct resource * rdl_resource_get (struct rdl *rdl, const char *uri)
{
    struct resource *r;
    if (uri == NULL)
        uri = "default";
    rdl_dostringf (rdl, "return rdl:resource ('%s')", uri);
    if (lua_type (rdl->L, -1) != LUA_TTABLE) {
        VERR (rdl->rl, "resource (%s): %s\n", uri, lua_tostring (rdl->L, -1));
        return (NULL);
    }
    r = create_resource_ref (rdl, -1);
    lua_settop (rdl->L, 0);
    return (r);
}

static int lua_rdl_resource_push (struct resource *r)
{
    lua_rawgeti (r->rdl->L, LUA_REGISTRYINDEX, r->lua_ref);
    return (1);
}

static int lua_rdl_resource_method_push (struct resource *r, const char *name)
{
    lua_State *L = r->rdl->L;
    /*
     *  First push rdl resource proxy object onto stack
     */
    lua_rdl_resource_push (r);
    lua_getfield (L, -1, name);

    if (lua_type (L, -1) != LUA_TFUNCTION) {
        lua_pop (L, 1);
        lua_pushnil (L);
        lua_pushstring (L, "not a method");
        return (-1);
    }

    /*
     *  Push rdl resource reference again as first argument to "Method"
     */
    lua_rdl_resource_push (r);
    return (0);
}

static int lua_rdl_resource_getfield (struct resource *r, const char *x)
{
    lua_State *L = r->rdl->L;
    lua_rdl_resource_push (r);
    lua_getfield (L, -1, x);
    if (lua_isnoneornil (L, -1))
        return (-1);
    lua_replace (L, -2);
    return (0);
}

int lua_rdl_resource_method_call (struct resource *r, const char *name)
{
    if (lua_rdl_resource_method_push (r, name) < 0)
        return (-1);
    return lua_pcall (r->rdl->L, 1, LUA_MULTRET, 0);
}

/*
 *  For resource name and path, be cowardly and reread from Lua for
 *   each call. In the future, we may want to cache these values.
 */
const char * rdl_resource_basename (struct resource *r)
{
    lua_State *L = r->rdl->L;

    if (lua_rdl_resource_getfield (r, "basename") < 0)
        return (NULL);
    if (r->basename)
        free (r->basename);
    r->basename = strdup (lua_tostring (L, -1));
    lua_pop (L, 1);
    return (r->basename);
}

const char * rdl_resource_name (struct resource *r)
{
    lua_State *L = r->rdl->L;

    if (lua_rdl_resource_getfield (r, "name") < 0)
        return (NULL);
    if (r->name)
        free (r->name);
    r->name = strdup (lua_tostring (L, -1));
    lua_pop (L, 1);
    return (r->name);
}

const char * rdl_resource_path (struct resource *r)
{
    lua_State *L = r->rdl->L;

    if (lua_rdl_resource_getfield (r, "path") < 0)
        return (NULL);
    if (r->path)
        free (r->path);
    r->path = strdup (lua_tostring (L, -1));
    lua_pop (L, 1);
    return (r->path);
}

static size_t rdl_resource_get_value (struct resource *r, const char *name)
{
    size_t sz;
    lua_State *L = r->rdl->L;
    if (lua_rdl_resource_getfield (r, name) < 0)
        return (-1);
    sz = (size_t) lua_tointeger (L, -1);
    lua_pop (L, 1);
    return (sz);
}

size_t rdl_resource_size (struct resource *r)
{
    return rdl_resource_get_value (r, "size");
}

size_t rdl_resource_available (struct resource *r)
{
    return rdl_resource_get_value (r, "available");
}

size_t rdl_resource_allocated (struct resource *r)
{
    return rdl_resource_get_value (r, "allocated");
}

enum method_arg_type {
    M_ARG_TYPE_INTEGER,
    M_ARG_TYPE_STRING
};

static int rdl_resource_method_call1_keepstack (struct resource *r,
    const char *method, enum method_arg_type mtype, void *argptr)
{
    int rc = 0;
    lua_State *L = r->rdl->L;
    if (lua_rdl_resource_method_push (r, method) < 0)
        return (-1);

    if (mtype == M_ARG_TYPE_STRING)
        lua_pushstring (L, *(char **)argptr);
    else if (mtype == M_ARG_TYPE_INTEGER)
        lua_pushinteger (L, (lua_Integer) *(size_t *)argptr);

    /*
     *  stack: [ Method, object, arg ]
     */
    if (lua_pcall (L, 2, LUA_MULTRET, 0) || lua_isnoneornil (L, 1)) {
        VERR (r->rdl->rl, "%s(): %s\n", method, lua_tostring (L, -1));
        lua_settop (L, 0);
        rc = -1;
    }
    return (rc);
}

static int rdl_resource_method_call1_string (struct resource *r,
    const char *method, const char *arg)
{
    enum method_arg_type mt = M_ARG_TYPE_STRING;

    int rc = rdl_resource_method_call1_keepstack (r, method, mt, &arg);
    lua_settop (r->rdl->L, 0);
    return (rc);
}

static int rdl_resource_method_call1_int (struct resource *r,
    const char *method, size_t n)
{
    enum method_arg_type mt = M_ARG_TYPE_INTEGER;

    int rc = rdl_resource_method_call1_keepstack (r, method, mt, &n);
    lua_settop (r->rdl->L, 0);
    return (rc);
}

void rdl_resource_tag (struct resource *r, const char *tag)
{
    rdl_resource_method_call1_string (r, "tag", tag);
}

void rdl_resource_delete_tag (struct resource *r, const char *tag)
{
    if (rdl_resource_method_call1_string (r, "delete_tag", tag) < 0) {
        VERR (r->rdl->rl, "delete_tag (%s): %s\n", tag,
              lua_tostring (r->rdl->L, -1));
    }
}

int rdl_resource_set_int (struct resource *r, const char *tag, int64_t val)
{
    int rc = 0;
    lua_State *L = r->rdl->L;

    if (lua_rdl_resource_method_push (r, "tag") < 0)
        return (-1);
    lua_pushstring (L, tag);
    lua_pushnumber (L, val);
    if (lua_pcall (L, 3, LUA_MULTRET, 0) || lua_isnoneornil (L, 1)) {
        VERR (r->rdl->rl, "%s(%s): %s\n", "tag", tag, lua_tostring (L, -1));
        rc = -1;
    }
    lua_settop (L, 0);
    return (rc);
}

int rdl_resource_get_int (struct resource *r, const char *tag, int64_t *valp)
{
    enum method_arg_type mt = M_ARG_TYPE_STRING;
    lua_State *L = r->rdl->L;
    if (rdl_resource_method_call1_keepstack (r, "get", mt, &tag)  < 0)
        return (-1);
    *valp = (int64_t) lua_tointeger (L, -1);
    lua_settop (L, 0);
    return (0);
}


int rdl_resource_alloc (struct resource *r, size_t n)
{
    return rdl_resource_method_call1_int (r, "alloc", n);
}

int rdl_resource_free (struct resource *r, size_t n)
{
    return rdl_resource_method_call1_int (r, "free", n);
}

int rdl_resource_unlink_child (struct resource *r, const char *name)
{
    return rdl_resource_method_call1_string (r, "unlink", name);
}

/*
 *  Call [method] on resource [r] and return resulting Lua table
 *   as a json-c json_object.
 */
static json_t *
rdl_resource_method_to_json (struct resource *r, const char *method)
{
    json_t *o = NULL;
    lua_State *L = r->rdl->L;

    if (lua_rdl_resource_method_call (r,  method)) {
        VERR (r->rdl->rl, "json: %s\n", lua_tostring (L, -1));
        return (NULL);
    }
    if (lua_type (L, -1) != LUA_TTABLE) {
        VERR (r->rdl->rl, "json: Failed to get table. Got %s\n",
                             luaL_typename (L, -1));
        lua_pop (L, 1);
        return (NULL);
    }
    if (lua_value_to_json (L, -1, &o) < 0)
        o = NULL;

    /* Keep Lua stack clean */
    lua_settop (L, 0);
    return (o);
}

json_t * rdl_resource_json (struct resource *r)
{
    return rdl_resource_method_to_json (r, "tabulate");
}

json_t * rdl_resource_aggregate_json (struct resource *r)
{
    return rdl_resource_method_to_json (r, "aggregate");
}


struct resource * rdl_resource_next_child (struct resource *r)
{
    struct resource *c;
    if (lua_rdl_resource_method_call (r, "next_child")) {
        VERR (r->rdl->rl, "next child: %s\n", lua_tostring (r->rdl->L, -1));
        return NULL;
    }
    if (lua_isnil (r->rdl->L, -1)) {
        /* End of child list is indicated by nil return */
        return (NULL);
    }
    c = create_resource_ref (r->rdl, -1);
    lua_settop (r->rdl->L, 0);
    return (c);
}


void rdl_resource_iterator_reset (struct resource *r)
{
    if (lua_rdl_resource_method_call (r, "reset"))
        VERR (r->rdl->rl, "iterator reset: %s\n", lua_tostring (r->rdl->L, -1));
}

/*
 * vi: ts=4 sw=4 expandtab
 */

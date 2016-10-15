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
#include "config.h"
#endif
#include <math.h>
#include <lua.h>
#include <lauxlib.h>
#include <jansson.h>

#if JANSSON_VERSION_HEX <= 0x020300
#define json_object_foreach(x, k, v) \
    for(void *__json_iterator__ = json_object_iter(x); \
        __json_iterator__ && \
        (k = json_object_iter_key(__json_iterator__), v = json_object_iter_value(__json_iterator__), 1); \
        __json_iterator__ = json_object_iter_next(x, __json_iterator__))
#endif

static void * json_nullptr;

static int json_object_to_lua_table (lua_State *L, json_t *o);
static int json_array_to_lua (lua_State *L, json_t *o);

void lua_push_json_null (lua_State *L)
{
    lua_pushlightuserdata (L, json_nullptr);
}

int lua_is_json_null (lua_State *L, int index)
{
    return (lua_touserdata (L, index) == json_null);
}

int json_object_to_lua (lua_State *L, json_t *o)
{
        if (o == NULL)
            lua_pushnil (L);
        switch (json_typeof (o)) {
        case JSON_OBJECT:
            json_object_to_lua_table (L, o);
            break;
        case JSON_ARRAY:
            json_array_to_lua (L, o);
            break;
        case JSON_STRING:
            lua_pushstring (L, json_string_value (o));
            break;
        case JSON_INTEGER:
            lua_pushinteger (L, json_integer_value (o));
            break;
        case JSON_REAL:
            lua_pushnumber (L, json_real_value (o));
            break;
        case JSON_TRUE:
            lua_pushboolean (L, 1);
            break;
        case JSON_FALSE:
            lua_pushboolean (L, 0);
            break;
        case JSON_NULL:
            /* XXX: crap. */
            break;
        }
        return (1);
}

static int json_array_to_lua (lua_State *L, json_t *o)
{
    int i;
    int index;
    int n = json_array_size (o);
    lua_newtable (L);
    index = lua_gettop (L);


    for (i = 0; i < n; i++) {
        json_t *entry = json_array_get (o, i);
        if (entry == NULL)
            continue;
        json_object_to_lua (L, entry);
        lua_rawseti (L, index, i+1);
    }
    return (1);
}

static int json_object_to_lua_table (lua_State *L, json_t *o)
{
    const char *key;
    json_t *value;
    lua_newtable (L);

    json_object_foreach(o, key, value) {
        lua_pushstring (L, key);
        json_object_to_lua (L, value);
        lua_rawset (L, -3);
    }
    return (1);
}

static json_t * lua_table_to_json (lua_State *L, int i);

int lua_value_to_json (lua_State *L, int i, json_t **valp)
{
    int index = (i < 0) ? (lua_gettop (L) + 1) + i : i;
    json_t *o = NULL;

    if (lua_isnoneornil (L, i))
        return (-1);

    switch (lua_type (L, index)) {
        case LUA_TNUMBER:
            o = json_integer (lua_tointeger (L, index));
            break;
        case LUA_TBOOLEAN:
            o = json_boolean (lua_toboolean (L, index));
            break;
        case LUA_TSTRING:
            o = json_string (lua_tostring (L, index));
            break;
        case LUA_TTABLE:
            o = lua_table_to_json (L, index);
            break;
        case LUA_TNIL:
            o = json_object ();
            break;
        case LUA_TLIGHTUSERDATA:
            fprintf (stderr, "Got userdata\n");
            if (lua_touserdata (L, index) == json_null)
                break;
        default:
            luaL_error (L, "Unexpected Lua type %s",
                lua_typename (L, lua_type (L, index)));
            return (-1);
    }
    *valp = o;
    return (0);
}

static int lua_is_integer (lua_State *L, int index)
{
    double l;
    if ((lua_type (L, index) == LUA_TNUMBER) &&
        (l = lua_tonumber (L, index)) &&
        ((int) l == l) &&
        (l >= 1))
        return (1);
    return (0);
}

static int lua_table_is_array (lua_State *L, int index)
{
    int haskeys = 0;
    lua_pushnil (L);
    while (lua_next (L, index)) {
        haskeys = 1;
        /* If key is not a number abort */
        if (!lua_is_integer (L, -2)) {
            lua_pop (L, 2); /* pop key and value */
            return (0);
        }
        lua_pop (L, 1);
    }
    return (haskeys);
}

static json_t * lua_table_to_json_array (lua_State *L, int index)
{
    int rc;
    json_t *o = json_array ();
    lua_pushnil (L);
    while ((rc = lua_next (L, index))) {
        int i = lua_tointeger (L, -2);
        json_t *val;

        if (lua_value_to_json (L, -1, &val) < 0) {
            json_decref (o);
            return (NULL);
        }
        json_array_set_new (o, i-1, val);
        lua_pop (L, 1);
    }
    return (o);
}

static json_t * lua_table_to_json (lua_State *L, int index)
{
    json_t *o;

    if (!lua_istable (L, index))
        fprintf (stderr, "Object at index=%d is not table, is %s\n",
                index, lua_typename (L, lua_type (L, index)));

    if (lua_table_is_array (L, index))
        return lua_table_to_json_array (L, index);

    o = json_object ();
    lua_pushnil (L);
    while (lua_next (L, index)) {
        json_t *val;
        /* -2: key, -1: value */
        const char *key = lua_tostring (L, -2);
        if (lua_value_to_json (L, -1, &val) < 0) {
            json_decref (o);
            return (NULL);
        }
        json_object_set_new (o, key, val);
        /* Remove value, save 'key' for next iteration: */
        lua_pop (L, 1);
    }
    return (o);
}


/*
 * vi: ts=4 sw=4 expandtab
 */

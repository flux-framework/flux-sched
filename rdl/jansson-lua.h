
#ifndef HAVE_JSON_LUA
#define HAVE_JSON_LUA
#include <jansson.h>

void lua_push_json_null (lua_State *L);
int lua_is_json_null (lua_State *L, int index);
int json_object_to_lua (lua_State *L, json_t *o);
int lua_value_to_json (lua_State *L, int index, json_t **v);

#endif /* HAVE_JSON_LUA */

#ifndef _WIN32
#include "lua-utils.h"

////// convenience functions
/* assume that table is on the stack top */
const char *get_table_string(lua_State *L, const char *key)
{
    lua_pushstring(L, key);
    lua_gettable(L, -2);
    if (!lua_isstring(L, -1))
    {
        luaL_error(L, "Unexpected non-string value for key %s", key);
        return NULL;
    }
    const char *result = luaL_checkstring(L, -1);
    lua_pop(L, 1);
    return result;
}
void set_table_string(lua_State *L, const char *key, const char *value)
{
    lua_pushstring(L, key);
    lua_pushstring(L, value);
    lua_settable(L, -3);
}
void set_table_number(lua_State *L, const char *key, double value)
{
    lua_pushstring(L, key);
    lua_pushnumber(L, value);
    lua_settable(L, -3);
}
double get_table_number(lua_State *L, const char *key)
{
    lua_pushstring(L, key);
    lua_gettable(L, -2);
    if (!lua_isnumber(L, -1))
    {
        luaL_error(L, "Unexpected non-number value for key %s", key);
        return 0.0;
    }
    double result = luaL_checknumber(L, -1);
    lua_pop(L, 1);
    return result;
}
bool store_function(lua_State *L, int *storage)
{
    if(*storage) {
        luaL_unref(L, LUA_REGISTRYINDEX, *storage);
    }

    if(lua_type(L, -1) == LUA_TFUNCTION)
    {
        *storage = luaL_ref(L, LUA_REGISTRYINDEX);
        return true;
    }
    else if(lua_type(L, -1) == LUA_TNIL)
    {
        *storage = 0;
        return false;
    }
    else
    {
        luaL_error(L, "Invalid function");
        return false;
    }
}
bool get_function(lua_State *L, int storage)
{
    if(storage == 0)
    {
        return false;
    }
    lua_rawgeti(L, LUA_REGISTRYINDEX, storage);
    if(lua_isnil(L, -1))
    {
        lua_pop(L, 1);
        return false;
    }
    return true;
}

void allua_push_cjson_value(lua_State *L, cJSON *cjvalue)
{
    switch(cjvalue->type) {
    case cJSON_False:
        lua_pushboolean(L, false);
        break;
    case cJSON_True:
        lua_pushboolean(L, true);
        break;
    case cJSON_NULL:
        lua_pushnil(L);
        break;
    case cJSON_Number:
        lua_pushnumber(L, cjvalue->valuedouble);
        break;
    case cJSON_String:
        lua_pushstring(L, cjvalue->valuestring);
        break;
    case cJSON_Array:
    {
        lua_newtable(L);
        int i = 1;
        for(struct cJSON *c = cjvalue->child; c != NULL; i++, c = c->next)
        {
            lua_pushinteger(L, i);
            allua_push_cjson_value(L, c);
            lua_settable(L, -3);
        }
        break;
    }
    case cJSON_Object:
        lua_newtable(L);
        for(struct cJSON *c = cjvalue->child; c != NULL; c = c->next)
        {
            lua_pushstring(L, c->string);
            allua_push_cjson_value(L, c);
            lua_settable(L, -3);
        }
    }
}

void push_interaction_table(lua_State *L, allo_interaction *inter)
{
    lua_newtable(L);
    set_table_string(L, "type", inter->type);
    set_table_string(L, "sender_entity_id", inter->sender_entity_id);
    set_table_string(L, "receiver_entity_id", inter->receiver_entity_id);
    set_table_string(L, "request_id", inter->request_id);
    set_table_string(L, "body", inter->body);
}

void push_state_table(lua_State *L, allo_state *state)
{
    lua_newtable(L); // state table
    set_table_number(L, "revision", state->revision);

    lua_pushstring(L, "entities");
    lua_newtable(L);
    int i = 1;
    allo_entity *entity = NULL;
    LIST_FOREACH(entity, &state->entities, pointers)
    {
        lua_pushinteger(L, i++);
        lua_newtable(L); // entity description
            set_table_string(L, "id", entity->id);

            lua_pushstring(L, "components");
            allua_push_cjson_value(L, entity->components);
            lua_settable(L, -3);

        lua_settable(L, -3);
    }
    lua_settable(L, -3);
}
#endif
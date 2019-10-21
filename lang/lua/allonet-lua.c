#include <stdio.h>
#include <string.h>
#include <math.h>
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "../../include/allonet/client.h"

// Todo: figure out how reference lua symbols weakly in Windows (see LUA_LINKER_FLAGS for mac/linux)
// so that this library can contain a Lua API without requiring linking with Lua (e g when used from
// C or Python or whatev)
#ifndef _WIN32

////// convenience functions
/* assume that table is on the stack top */
static const char *get_table_string(lua_State *L, const char *key)
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
static void set_table_string(lua_State *L, const char *key, const char *value)
{
    lua_pushstring(L, key);
    lua_pushstring(L, value);
    lua_settable(L, -3);
}
static double get_table_number(lua_State *L, const char *key)
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
static bool store_function(lua_State *L, int *storage)
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
static bool get_function(lua_State *L, int storage)
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

static void push_interaction_table(lua_State *L, allo_interaction *inter)
{
    lua_newtable(L);
    set_table_string(L, "type", inter->type);
    set_table_string(L, "sender_entity_id", inter->sender_entity_id);
    set_table_string(L, "receiver_entity_id", inter->receiver_entity_id);
    set_table_string(L, "request_id", inter->request_id);
    set_table_string(L, "body", inter->body);
}

static void push_state_table(lua_State *L, allo_state *state)
{
    lua_newtable(L);
}


//// alloclient structure
typedef struct l_alloclient
{
    alloclient *client;
    lua_State *L;
    int state_callback_index;
    int interaction_callback_index;
    int disconnected_callback_index;
} l_alloclient_t;

static int l_alloclient_connect (lua_State *L)
{
    const char *url =  luaL_checkstring(L, 1);
    const char *identity =  luaL_checkstring(L, 2);
    const char *avatar_desc =  luaL_checkstring(L, 3);

    alloclient *client = allo_connect(url, identity, avatar_desc);

    if(client == NULL)
    {
        return luaL_error(L, "Allonet failed to connect to %s", url);
    }

    l_alloclient_t *lclient = (l_alloclient_t *)lua_newuserdata(L, sizeof(l_alloclient_t));
    lclient->client = client;
    lclient->L = L;
    client->_backref = lclient;
    luaL_getmetatable(L, "allonet.client");
    lua_setmetatable(L, -2);
    return 1;
}

static const struct luaL_reg allonet [] =
{
    {"connect", l_alloclient_connect},
    {NULL, NULL}
};

/// Fetch an l_alloclient_t userdata from stack, throwing if it's a mismatch
static l_alloclient_t *check_alloclient (lua_State *L, int n)
{
    void *ud = luaL_checkudata(L, n, "allonet.client");
    luaL_argcheck(L, ud != NULL, n, "`allonet.client' expected");
    return (l_alloclient_t *)ud;
}

///// alloclient methods

static int l_alloclient_disconnect (lua_State *L)
{
    l_alloclient_t *lclient = check_alloclient(L, 1);
    int reason = luaL_checkint(L, 2);
    alloclient_disconnect(lclient->client, reason);
    return 0;
}

static int l_alloclient_poll (lua_State *L)
{
    l_alloclient_t *lclient = check_alloclient(L, 1);
    alloclient_poll(lclient->client);
    return 0;
}

static int l_alloclient_send_interaction (lua_State *L)
{
    l_alloclient_t *lclient = check_alloclient(L, 1);
    if(!lua_istable(L, 2))
    {
        return luaL_error(L, "send_interaction: Expected table with keys type, sender_entity_id, receiver_entity_id, request_id and body");
    }
    allo_interaction *inter = allo_interaction_create(
        get_table_string(L, "type"),
        get_table_string(L, "sender_entity_id"),
        get_table_string(L, "receiver_entity_id"),
        get_table_string(L, "request_id"),
        get_table_string(L, "body")
    );
    alloclient_send_interaction(lclient->client, inter);
    allo_interaction_free(inter);
    return 0;
}

static int l_alloclient_set_intent (lua_State *L)
{
    l_alloclient_t *lclient = check_alloclient(L, 1);
    if(!lua_istable(L, 2))
    {
        return luaL_error(L, "set_interaction: Expected table with keys type, sender_entity_id, receiver_entity_id, request_id and body");
    }
    allo_client_intent intent = {
        get_table_number(L, "zmovement"),
        get_table_number(L, "xmovement"),
        get_table_number(L, "yaw"),
        get_table_number(L, "pitch")
    };
    alloclient_set_intent(lclient->client, intent);
    return 0;
}

static int l_alloclient_pop_interaction (lua_State *L)
{
    l_alloclient_t *lclient = check_alloclient(L, 1);
    allo_interaction *inter = alloclient_pop_interaction(lclient->client);
    if(!inter)
    {
        lua_pushnil(L);
    } else {
        push_interaction_table(L, inter);
    }
    return 1;
}

static void state_callback(alloclient *client, allo_state *state);
static void interaction_callback(alloclient *client, allo_interaction *interaction);
static void disconnected_callback(alloclient *client);

static int l_alloclient_set_state_callback (lua_State *L)
{
    l_alloclient_t *lclient = check_alloclient(L, 1);
    if(store_function(L, &lclient->state_callback_index)) {
        lclient->client->state_callback = state_callback;
    } else {
        lclient->client->state_callback = NULL;
    }
    return 0;
}

static int l_alloclient_set_interaction_callback (lua_State *L)
{
    l_alloclient_t *lclient = check_alloclient(L, 1);
    if(store_function(L, &lclient->interaction_callback_index)) {
        lclient->client->interaction_callback = interaction_callback;
    } else {
        lclient->client->interaction_callback = NULL;
    }
    return 0;
}

static int l_alloclient_set_disconnected_callback (lua_State *L)
{
    l_alloclient_t *lclient = check_alloclient(L, 1);
    if(store_function(L, &lclient->disconnected_callback_index)) {
        lclient->client->disconnected_callback = disconnected_callback;
    } else {
        lclient->client->disconnected_callback = NULL;
    }
    return 0;
}

static int l_alloclient_get_state (lua_State *L)
{
    l_alloclient_t *lclient = check_alloclient(L, 1);
    push_state_table(L, lclient->client->state);
    return 1;
}

////// Callbacks from allonet
static void state_callback(alloclient *client, allo_state *state)
{
    l_alloclient_t *lclient = (l_alloclient_t*)client->_backref;
    if(get_function(lclient->L, lclient->state_callback_index))
    {
        push_state_table(L, lclient->client->state);
        lua_pcall(lclient->L, 1, 0, 0);
    }
}

static void interaction_callback(alloclient *client, allo_interaction *inter)
{
    l_alloclient_t *lclient = (l_alloclient_t*)client->_backref;
    if(get_function(lclient->L, lclient->interaction_callback_index))
    {
        push_interaction_table(lclient->L, inter);
        lua_pcall(lclient->L, 1, 0, 0);
    }
}

static void disconnected_callback(alloclient *client)
{
    l_alloclient_t *lclient = (l_alloclient_t*)client->_backref;
    if(get_function(lclient->L, lclient->disconnected_callback_index))
    {
        lua_pcall(lclient->L, 0, 0, 0);
    }
}


////// library initialization

static const struct luaL_reg alloclient_m [] = {
    {"disconnect", l_alloclient_disconnect},
    {"poll", l_alloclient_poll},
    {"send_interaction", l_alloclient_send_interaction},
    {"set_intent", l_alloclient_set_intent},
    {"pop_interaction", l_alloclient_pop_interaction},
    {"set_state_callback", l_alloclient_set_state_callback},
    {"set_interaction_callback", l_alloclient_set_interaction_callback},
    {"set_disconnected_callback", l_alloclient_set_disconnected_callback},
    {"get_state", l_alloclient_get_state},
    {NULL, NULL}
};

int luaopen_liballonet (lua_State *L) {
    luaL_newmetatable(L, "allonet.client");
    lua_pushstring(L, "__index");
    lua_pushvalue(L, -2);  /* pushes the metatable */
    lua_settable(L, -3);  /* metatable.__index = metatable */
    luaL_openlib(L, NULL, alloclient_m, 0);
    
    luaL_register (L, "allonet", allonet);
    return 1;
}

#endif
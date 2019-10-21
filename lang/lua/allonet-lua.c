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

typedef struct l_alloclient {
    alloclient *client;
} l_alloclient_t;

static int l_alloclient_connect (lua_State *L) {
    const char *url =  luaL_checkstring(L, 1);
    const char *identity =  luaL_checkstring(L, 2);
    const char *avatar_desc =  luaL_checkstring(L, 3);

    alloclient *client = allo_connect(url, identity, avatar_desc);

    if(client == NULL)
    {
        luaL_error(L, "Allonet failed to connect to %s", url);
        return 0;
    }

    l_alloclient_t *lclient = (l_alloclient_t *)lua_newuserdata(L, sizeof(l_alloclient_t));
    lclient->client = client;
    luaL_getmetatable(L, "allonet.client");
    lua_setmetatable(L, -2);
    return 1;
}

static const struct luaL_reg allonet [] = {
    {"connect", l_alloclient_connect},
    {NULL, NULL}
};


static int l_alloclient_disconnect (lua_State *L) {
    return 0;
}

static int l_alloclient_poll (lua_State *L) {
    return 0;
}

static int l_alloclient_send_interaction (lua_State *L) {
    return 0;
}

static int l_alloclient_set_intent (lua_State *L) {
    return 0;
}

static int l_alloclient_pop_interaction (lua_State *L) {
    return 0;
}

static int l_alloclient_set_state_callback (lua_State *L) {
    return 0;
}

static int l_alloclient_set_interaction_callback (lua_State *L) {
    return 0;
}

static int l_alloclient_set_disconnected_callback (lua_State *L) {
    return 0;
}

static const struct luaL_reg alloclient_m [] = {
    {"disconnect", l_alloclient_disconnect},
    {"poll", l_alloclient_poll},
    {"send_interaction", l_alloclient_send_interaction},
    {"set_intent", l_alloclient_set_intent},
    {"pop_interaction", l_alloclient_pop_interaction},
    {"set_state_callback", l_alloclient_set_state_callback},
    {"set_interaction_callback", l_alloclient_set_interaction_callback},
    {"set_disconnected_callback", l_alloclient_set_disconnected_callback},
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
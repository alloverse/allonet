#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "lua-utils.h"

//// alloclient structure
typedef struct l_alloclient
{
    alloclient *client;
    lua_State *L;
    int state_callback_index;
    int interaction_callback_index;
    int disconnected_callback_index;
    int audio_callback_index;
} l_alloclient_t;

static int l_alloclient_create (lua_State *L)
{
    alloclient *client = alloclient_create();
    l_alloclient_t *lclient = (l_alloclient_t *)lua_newuserdata(L, sizeof(l_alloclient_t));
    lclient->client = client;
    lclient->L = L;
    client->_backref = lclient;
    luaL_getmetatable(L, "allonet.client");
    lua_setmetatable(L, -2);
    return 1;
}

static int server_count = 0;
static int l_alloserv_start_standalone(lua_State* L)
{
  int port = luaL_checkint(L, 1);
  server_count++;
  bool done_successfully = alloserv_run_standalone(port);
  server_count--;
  lua_pushboolean(L, done_successfully);
  return 1;
}

static int l_alloserv_standalone_server_count(lua_State* L)
{
  lua_pushnumber(L, server_count);
  return 1;
}

static const struct luaL_reg allonet [] =
{
    {"create", l_alloclient_create},
    {"start_standalone_server", l_alloserv_start_standalone},
    {"standalone_server_count", l_alloserv_standalone_server_count},
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

static int l_alloclient_connect (lua_State *L)
{
    l_alloclient_t *lclient = check_alloclient(L, 1);

    const char *url =  luaL_checkstring(L, 2);
    const char *identity =  luaL_checkstring(L, 3);
    const char *avatar_desc =  luaL_checkstring(L, 4);

    bool success = allo_connect(lclient->client, url, identity, avatar_desc);

    lua_pushboolean(L, success);
    return 1;
}

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
    char* type = get_table_string(L, "type");
    char* sender = get_table_string(L, "sender_entity_id");
    char* recv = get_table_string(L, "receiver_entity_id");
    char* req = get_table_string(L, "request_id");
    char* b = get_table_string(L, "body");
    allo_interaction *inter = allo_interaction_create(type, sender, recv, req, b);
    free(type); free(sender); free(recv); free(req); free(b);
    alloclient_send_interaction(lclient->client, inter);
    allo_interaction_free(inter);
    return 0;
}

static int l_alloclient_send_audio(lua_State* L)
{
    l_alloclient_t* lclient = check_alloclient(L, 1);
    int32_t track_id = luaL_checkint(L, 2);
    size_t bytelength = 0;
    const char* data = luaL_checklstring(L, 3, &bytelength);
    size_t samplelength = bytelength / sizeof(int16_t);
    lua_assert(samplelength == 480 || samplelength == 960);
    alloclient_send_audio(lclient->client, track_id, (int16_t*)data, samplelength);
    return 0;
}

static int l_alloclient_set_intent (lua_State *L)
{
    l_alloclient_t *lclient = check_alloclient(L, 1);
    if(!lua_istable(L, 2))
    {
        return luaL_error(L, "set_intent: Expected table with keys zmovement, xmovement, yaw, pitch, poses");
    }
    allo_client_intent* intent = allo_client_intent_create();
    intent->entity_id = get_table_string(L, "entity_id"),
    intent->zmovement = get_table_number(L, "zmovement"),
    intent->xmovement = get_table_number(L, "xmovement"),
    intent->yaw = get_table_number(L, "yaw"),
    intent->pitch = get_table_number(L, "pitch"),
    intent->poses = get_table_poses(L, "poses"),
    alloclient_set_intent(lclient->client, intent);
    allo_client_intent_free(intent);
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
static void disconnected_callback(alloclient *client, alloerror code, const char* message );
static void audio_callback(alloclient* client, uint32_t track_id, int16_t pcm[], int32_t bytes_decoded);

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

static int l_alloclient_set_audio_callback(lua_State* L)
{
    l_alloclient_t* lclient = check_alloclient(L, 1);
    if (store_function(L, &lclient->audio_callback_index)) {
        lclient->client->audio_callback = audio_callback;
    }
    else {
        lclient->client->audio_callback = NULL;
    }
    return 0;
}

static int l_alloclient_get_state (lua_State *L)
{
    l_alloclient_t *lclient = check_alloclient(L, 1);
    push_state_table(L, &lclient->client->state);
    return 1;
}

static int l_alloclient_simulate(lua_State* L)
{
    l_alloclient_t* lclient = check_alloclient(L, 1);
    double dt = luaL_checknumber(L, 2);
    alloclient_simulate(lclient->client, dt);
    return 0;
}

////// Callbacks from allonet
static void state_callback(alloclient *client, allo_state *state)
{
    l_alloclient_t *lclient = (l_alloclient_t*)client->_backref;
    if(get_function(lclient->L, lclient->state_callback_index))
    {
        push_state_table(lclient->L, &lclient->client->state);
        lua_call(lclient->L, 1, 0);
    }
}

static void interaction_callback(alloclient *client, allo_interaction *inter)
{
    l_alloclient_t *lclient = (l_alloclient_t*)client->_backref;
    if(get_function(lclient->L, lclient->interaction_callback_index))
    {
        push_interaction_table(lclient->L, inter);
        lua_call(lclient->L, 1, 0);
    }
}

static void disconnected_callback(alloclient *client, alloerror code, const char* message )
{
    l_alloclient_t *lclient = (l_alloclient_t*)client->_backref;
    if(get_function(lclient->L, lclient->disconnected_callback_index))
    {
        lua_pushinteger(lclient->L, code);
        lua_pushstring(lclient->L, message);
        lua_call(lclient->L, 2, 0);
    }
}

static void audio_callback(alloclient* client, uint32_t track_id, int16_t pcm[], int32_t samples_decoded)
{
    l_alloclient_t* lclient = (l_alloclient_t*)client->_backref;
    if (get_function(lclient->L, lclient->audio_callback_index))
    {
        lua_pushnumber(lclient->L, track_id);
        lua_pushlstring(lclient->L, (const char*)pcm, samples_decoded*sizeof(int16_t));
        lua_call(lclient->L, 2, 0);
    }
}


////// library initialization

static const struct luaL_reg alloclient_m [] = {
    {"connect", l_alloclient_connect},
    {"disconnect", l_alloclient_disconnect},
    {"poll", l_alloclient_poll},
    {"send_interaction", l_alloclient_send_interaction},
    {"send_audio", l_alloclient_send_audio},
    {"set_intent", l_alloclient_set_intent},
    {"pop_interaction", l_alloclient_pop_interaction},
    {"set_state_callback", l_alloclient_set_state_callback},
    {"set_interaction_callback", l_alloclient_set_interaction_callback},
    {"set_disconnected_callback", l_alloclient_set_disconnected_callback},
    {"set_audio_callback", l_alloclient_set_audio_callback},
    {"get_state", l_alloclient_get_state},
    {"simulate", l_alloclient_simulate},
    {NULL, NULL}
};

int luaopen_liballonet (lua_State *L) {
	load_weak_lua_symbols();
	allo_initialize(false);
	assert(luaL_register != NULL);

    luaL_newmetatable(L, "allonet.client");
    lua_pushstring(L, "__index");
    lua_pushvalue(L, -2);  /* pushes the metatable */
    lua_settable(L, -3);  /* metatable.__index = metatable */
    luaL_register(L, NULL, alloclient_m);
    
    luaL_register (L, "allonet", allonet);
    return 1;
}

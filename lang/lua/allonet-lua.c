#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <stdlib.h>
#ifdef _WIN32
    #include <process.h>
#else
    #include <sys/types.h>
    #include <unistd.h>
#endif
#include "lua-utils.h"
#include "../../src/util.h"
#include "../../src/asset.h"
#include "../../src/simulation/simulation.h"
#include <allonet/client.h>

//// alloclient structure
typedef struct l_alloclient
{
    alloclient *client;
    lua_State *L;
    int state_callback_index;
    int interaction_callback_index;
    int disconnected_callback_index;
    int audio_callback_index;
    int video_callback_index;
    int asset_request_callback_index;
    int asset_state_callback_index;
    int asset_receive_callback_index;
    bool wants_full_state_in_callback;
} l_alloclient_t;

static int l_alloclient_create (lua_State *L)
{
    bool threaded = lua_toboolean(L, 1);
    alloclient *client = alloclient_create(threaded);
    l_alloclient_t *lclient = (l_alloclient_t *)lua_newuserdata(L, sizeof(l_alloclient_t));
    memset(lclient, 0, sizeof(*lclient));
    lclient->client = client;
    lclient->L = L;
    client->_backref = lclient;
    luaL_getmetatable(L, "allonet.client");
    lua_setmetatable(L, -2);
    return 1;
}

static int l_get_monotonic_time(lua_State *L)
{
    double t = get_ts_monod();
    lua_pushnumber(L, t);
    return 1;
}

static int l_get_random_seed(lua_State *L)
{
    uint64_t t = get_ts_mono();
    t += getpid();
    t += (uint64_t)L;
    t = (t << 16) | (t >> 16);
    lua_pushnumber(L, t);
    return 1;
}

static int l_alloserv_run_standalone(lua_State* L)
{
  int host = luaL_checkint(L, 1);
  int port = luaL_checkint(L, 2);
  const char *name = luaL_checkstring(L, 3);
  bool done_successfully = alloserv_run_standalone(host, port, name);
  lua_pushboolean(L, done_successfully);
  return 1;
}

static int l_alloserv_start_standalone(lua_State* L)
{
  int host = luaL_checkint(L, 1);
  int port = luaL_checkint(L, 2);
  const char *name = luaL_checkstring(L, 3);
  alloserver *serv = alloserv_start_standalone(host, port, name);
  int allosocket = allo_socket_for_select(serv);
  int actualport = serv->_port;
  lua_pushinteger(L, allosocket);
  lua_pushinteger(L, actualport);
  return 2;
}

static int l_alloserv_poll_standalone(lua_State* L)
{
  int allosocket = luaL_checkint(L, 1);
  bool done_successfully = alloserv_poll_standalone(allosocket);
  lua_pushboolean(L, done_successfully);
  return 1;
}

static int l_alloserv_stop_standalone(lua_State* L)
{
  (void)L;
  alloserv_stop_standalone();
  return 0;
}


static int l_asset_generate_identifier (lua_State *L)
{
    size_t size = 0;
    const char *data  = luaL_checklstring(L, 1, &size);
    char *res = asset_generate_identifier((uint8_t*)data, size);
    lua_pushstring(L, res);
    free(res);
    return 1;
}

static const struct luaL_Reg allonet [] =
{
    {"create", l_alloclient_create },
    {"get_monotonic_time", l_get_monotonic_time },
    {"get_random_seed", l_get_random_seed},
    {"run_standalone_server", l_alloserv_run_standalone},
    {"start_standalone_server", l_alloserv_start_standalone},
    {"poll_standalone_server", l_alloserv_poll_standalone},
    {"stop_standalone_server", l_alloserv_stop_standalone},
    {"asset_generate_identifier", l_asset_generate_identifier},
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

    bool success = alloclient_connect(lclient->client, url, identity, avatar_desc);

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
    double timeout = luaL_checknumber(L, 2);
    alloclient_poll(lclient->client, timeout * 1000);
    return 0;
}

static int l_alloclient_send_interaction (lua_State *L)
{
    l_alloclient_t *lclient = check_alloclient(L, 1);
    if(!lua_istable(L, 2))
    {
        return luaL_error(L, "send_interaction: Expected table with keys type, sender_entity_id, receiver_entity_id, request_id and body");
    }
    const char* type = get_table_string(L, "type");
    const char* sender = get_table_string(L, "sender_entity_id");
    const char* recv = get_table_string(L, "receiver_entity_id");
    const char* req = get_table_string(L, "request_id");
    const char* b = get_table_string(L, "body");
    allo_interaction *inter = allo_interaction_create(type, sender, recv, req, b);
    free((void*)type);
    free((void*)sender);
    free((void*)recv);
    free((void*)req);
    free((void*)b);
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

static int l_alloclient_send_video(lua_State *L)
{
    l_alloclient_t *lclient = check_alloclient(L, 1);
    int32_t track_id = luaL_checkint(L, 2);
    size_t bytelength = 0;
    const uint8_t* data = (uint8_t*)luaL_checklstring(L, 3, &bytelength);
    size_t width = luaL_checkint(L, 4);
    size_t height = luaL_checkint(L, 5);
    const char *format = luaL_optstring(L, 6, "rgba");
    allopicture *picture = calloc(1, sizeof(allopicture));
    picture->format = 
        strcmp(format, "rgba") == 0 ? allopicture_format_rgba8888 :
        strcmp(format, "bgra") == 0 ? allopicture_format_bgra8888 :
        strcmp(format, "xrgb8") == 0 ? allopicture_format_xrgb8888 :
        strcmp(format, "rgb1555") == 0 ? allopicture_format_rgb1555 :
        strcmp(format, "rgb565") == 0 ? allopicture_format_rgb565 : 255;
    assert(picture->format != 255 && "invalid allopicture format");
    int bytes_per_pixel = allopicture_bpp(picture->format);
    size_t stride = luaL_optint(L, 7, (int)(width * bytes_per_pixel));

    picture->plane_strides[0] = stride;
    picture->plane_byte_lengths[0] = stride * height;
    picture->planes[0].monochrome = malloc(picture->plane_byte_lengths[0]);
    memcpy(picture->planes[0].monochrome, data, picture->plane_byte_lengths[0]);
    picture->height = height;
    picture->width = width;
    picture->plane_count = 1;

    alloclient_send_video(lclient->client, track_id, picture);
    
    return 0;
}

static int l_alloclient_set_intent (lua_State *L)
{
    l_alloclient_t *lclient = check_alloclient(L, 1);
    if(!lua_istable(L, 2))
    {
        return luaL_error(L, "set_intent: Expected table with keys wants_stick_movement, zmovement, xmovement, yaw, pitch, poses");
    }
    allo_client_intent* intent = get_intent(L);
    alloclient_set_intent(lclient->client, intent);
    allo_client_intent_free(intent);
    return 0;
}

static void state_callback(alloclient *client, allo_state *state, allo_state_diff *diff);
static bool interaction_callback(alloclient *client, allo_interaction *interaction);
static void disconnected_callback(alloclient *client, alloerror code, const char* message );
static bool audio_callback(alloclient* client, uint32_t track_id, int16_t pcm[], int32_t bytes_decoded);
static bool video_callback(alloclient *client, uint32_t track_id, allopixel pixels[], int32_t pixels_wide, int32_t pixels_high);
static void asset_state_callback(alloclient *client, const char *asset_id, client_asset_state state);
static void asset_request_callback(alloclient *client, const char *asset_id, size_t offset, size_t length);
static void asset_receive_callback(alloclient *client, const char *asset_id, const uint8_t *data, size_t offset, size_t length, size_t total_size);

static int l_alloclient_set_state_callback (lua_State *L)
{
    l_alloclient_t *lclient = check_alloclient(L, 1);
    // default to true for compat
    lclient->wants_full_state_in_callback = !lua_isboolean(L, 3) || lua_toboolean(L, 3);
    
    if(store_function(L, 2, &lclient->state_callback_index)) {
        lclient->client->state_callback = state_callback;
    } else {
        lclient->client->state_callback = NULL;
    }
    return 0;
}

static int l_alloclient_set_asset_state_callback (lua_State *L)
{
    l_alloclient_t *lclient = check_alloclient(L, 1);
    if(store_function(L, 2, &lclient->asset_state_callback_index)) {
        lclient->client->asset_state_callback = asset_state_callback;
    } else {
        lclient->client->asset_state_callback = NULL;
    }
    return 0;
}

static int l_alloclient_set_asset_request_callback (lua_State *L)
{
    l_alloclient_t *lclient = check_alloclient(L, 1);
    if(store_function(L, 2, &lclient->asset_request_callback_index)) {
        lclient->client->asset_request_bytes_callback = asset_request_callback;
    } else {
        lclient->client->asset_request_bytes_callback = NULL;
    }
    return 0;
}

static int l_alloclient_set_asset_receive_callback (lua_State *L)
{
    l_alloclient_t *lclient = check_alloclient(L, 1);
    if (store_function(L, 2, &lclient->asset_receive_callback_index)) {
        lclient->client->asset_receive_callback = asset_receive_callback;
    } else {
        lclient->client->asset_receive_callback = NULL;
    }
    return 0;
}

static int l_alloclient_set_interaction_callback (lua_State *L)
{
    l_alloclient_t *lclient = check_alloclient(L, 1);
    if(store_function(L, 2, &lclient->interaction_callback_index)) {
        lclient->client->interaction_callback = interaction_callback;
    } else {
        lclient->client->interaction_callback = NULL;
    }
    return 0;
}

static int l_alloclient_set_disconnected_callback (lua_State *L)
{
    l_alloclient_t *lclient = check_alloclient(L, 1);
    if(store_function(L, 2, &lclient->disconnected_callback_index)) {
        lclient->client->disconnected_callback = disconnected_callback;
    } else {
        lclient->client->disconnected_callback = NULL;
    }
    return 0;
}

static int l_alloclient_set_audio_callback(lua_State* L)
{
    l_alloclient_t* lclient = check_alloclient(L, 1);
    if (store_function(L, 2, &lclient->audio_callback_index)) {
        lclient->client->audio_callback = audio_callback;
    }
    else {
        lclient->client->audio_callback = NULL;
    }
    return 0;
}

static int l_alloclient_set_video_callback(lua_State *L)
{
    l_alloclient_t* lclient = check_alloclient(L, 1);
    if (store_function(L, 2, &lclient->video_callback_index)) {
        lclient->client->video_callback = video_callback;
    }
    else {
        lclient->client->video_callback = NULL;
    }
    return 0;
}

static int l_alloclient_simulate(lua_State* L)
{
    l_alloclient_t* lclient = check_alloclient(L, 1);
    alloclient_simulate(lclient->client);
    return 0;
}

static int l_alloclient_simulate_root_pose(lua_State* L)
{
    l_alloclient_t* lclient = check_alloclient(L, 1);
    const char *avatar_id = luaL_checkstring(L, 2);
    allo_entity *avatar = state_get_entity(lclient->client->_state, avatar_id);
    allo_entity* head = allosim_get_child_with_pose(lclient->client->_state, avatar, "head");
    float dt = luaL_checknumber(L, 3);
    allo_client_intent *intent = get_intent(L);
    
    allo_m4x4 root_pose = allosim_stick_movement(avatar, head, intent, dt, false, NULL);
    allo_client_intent_free(intent);
    push_matrix_table(L, root_pose);
    return 1;
}

static int l_alloclient_get_state (lua_State *L)
{
    l_alloclient_t *lclient = check_alloclient(L, 1);
    push_state_table(L, lclient->client->_state);
    return 1;
}


static int l_alloclient_get_time(lua_State *L)
{
    l_alloclient_t* lclient = check_alloclient(L, 1);
    (void)lclient;
    double t = get_ts_monod();
    lua_pushnumber(L, t);
    return 1;
}

static int l_alloclient_get_server_time(lua_State *L)
{
    l_alloclient_t* lclient = check_alloclient(L, 1);
    double t = alloclient_get_time(lclient->client);
    lua_pushnumber(L, t);
    return 1;
}

static int l_alloclient_get_latency(lua_State *L)
{
    l_alloclient_t* lclient = check_alloclient(L, 1);
    double t = lclient->client->clock_latency;
    lua_pushnumber(L, t);
    return 1;
}

static int l_alloclient_get_clock_delta(lua_State *L)
{
    l_alloclient_t* lclient = check_alloclient(L, 1);
    double t = lclient->client->clock_deltaToServer;
    lua_pushnumber(L, t);
    return 1;
}

static int l_alloclient_get_stats(lua_State *L)
{
    l_alloclient_t* lclient = check_alloclient(L, 1);
    char buf[1024];
    alloclient_get_stats(lclient->client, buf, 1024);
    lua_pushstring(L, buf);
    return 1;
}


////// Assets

static int l_alloclient_asset_send(lua_State *L)
{
    l_alloclient_t *lclient = check_alloclient(L, 1);
    
    const char *asset_id = luaL_checkstring(L, 2);
    size_t data_length = 0;
    const char *data = luaL_optlstring(L, 3, NULL, &data_length);
    size_t offset = luaL_checklong(L, 4);
    size_t total_size = luaL_checklong(L, 5);
    alloclient_asset_send(lclient->client, asset_id, (const uint8_t *)data, offset-1, data_length, total_size);
    return 0;
}

static int l_alloclient_asset_request(lua_State *L)
{
    l_alloclient_t *lclient = check_alloclient(L, 1);
    
    const char *asset_id  = luaL_checkstring(L, 2);
    const char *entity_id = luaL_optstring(L, 3, NULL);
    alloclient_asset_request(lclient->client, asset_id, entity_id);
    return 0;
}

////// Callbacks from allonet

static void state_callback(alloclient *client, allo_state *state, allo_state_diff *diff)
{
    l_alloclient_t *lclient = (l_alloclient_t*)client->_backref;
    if(get_function(lclient->L, lclient->state_callback_index))
    {
        // every lua operation runs a gc step so stop it while we do this
        lua_gc(lclient->L, LUA_GCSTOP, 0);
        if(lclient->wants_full_state_in_callback)
        {
            push_state_table(lclient->L, state);
        }
        else
        {
            lua_pushnil(lclient->L);
        }
        push_statediff(lclient->L, state, diff);
        lua_call(lclient->L, 2, 0);
        lua_gc(lclient->L, LUA_GCRESTART, 0);
    }
}

static bool interaction_callback(alloclient *client, allo_interaction *inter)
{
    l_alloclient_t *lclient = (l_alloclient_t*)client->_backref;
    if(get_function(lclient->L, lclient->interaction_callback_index))
    {
        push_interaction_table(lclient->L, inter);
        lua_call(lclient->L, 1, 0);
    }
    return true;
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

static bool audio_callback(alloclient* client, uint32_t track_id, int16_t pcm[], int32_t samples_decoded)
{
    l_alloclient_t* lclient = (l_alloclient_t*)client->_backref;
    if (get_function(lclient->L, lclient->audio_callback_index))
    {
        lua_pushnumber(lclient->L, track_id);
        lua_pushlstring(lclient->L, (const char*)pcm, samples_decoded*sizeof(int16_t));
        lua_call(lclient->L, 2, 0);
    }
    return true;
}

static bool video_callback(alloclient *client, uint32_t track_id, allopixel pixels[], int32_t pixels_wide, int32_t pixels_high)
{
    l_alloclient_t* lclient = (l_alloclient_t*)client->_backref;
    if (get_function(lclient->L, lclient->video_callback_index))
    {
        lua_pushnumber(lclient->L, track_id);
        lua_pushlightuserdata(lclient->L, (void*)pixels);
        lua_pushnumber(lclient->L, pixels_wide);
        lua_pushnumber(lclient->L, pixels_high);
        lua_call(lclient->L, 4, 0);
    }
    return true;
}

static void asset_state_callback(alloclient *client, const char *asset_id, client_asset_state state) {
    l_alloclient_t *lclient = (l_alloclient_t *)client->_backref;
    
    if (get_function(lclient->L, lclient->asset_state_callback_index)) {
        lua_pushstring(lclient->L, asset_id);
        lua_pushnumber(lclient->L, state);
        lua_call(lclient->L, 2, 0);
    }
}

static void asset_request_callback(alloclient *client, const char *asset_id, size_t offset, size_t length) {
    l_alloclient_t *lclient = (l_alloclient_t *)client->_backref;
    
    if (get_function(lclient->L, lclient->asset_request_callback_index)) {
        lua_pushstring(lclient->L, asset_id);
        lua_pushnumber(lclient->L, offset+1);
        lua_pushnumber(lclient->L, length);
        lua_call(lclient->L, 3, 0);
    }
}

static void asset_receive_callback(alloclient *client, const char *asset_id, const uint8_t *data, size_t offset, size_t length, size_t total_size) {
    l_alloclient_t *lclient = (l_alloclient_t *)client->_backref;
    
    if (get_function(lclient->L, lclient->asset_receive_callback_index)) {
        lua_pushstring(lclient->L, asset_id);
        lua_pushlstring(lclient->L, (const char *)data, length);
        lua_pushnumber(lclient->L, offset+1);
        lua_pushnumber(lclient->L, total_size);
        lua_call(lclient->L, 4, 0);
    }
}

////// library initialization

static const struct luaL_Reg alloclient_m [] = {
    {"connect", l_alloclient_connect},
    {"disconnect", l_alloclient_disconnect},
    {"poll", l_alloclient_poll},
    {"send_interaction", l_alloclient_send_interaction},
    {"send_audio", l_alloclient_send_audio},
    {"send_video", l_alloclient_send_video},
    {"set_intent", l_alloclient_set_intent},
    {"set_state_callback", l_alloclient_set_state_callback},
    {"set_interaction_callback", l_alloclient_set_interaction_callback},
    {"set_disconnected_callback", l_alloclient_set_disconnected_callback},
    {"set_audio_callback", l_alloclient_set_audio_callback},
    {"set_video_callback", l_alloclient_set_video_callback},
    {"simulate", l_alloclient_simulate},
    {"simulate_root_pose", l_alloclient_simulate_root_pose},
    {"get_state", l_alloclient_get_state},
    {"get_time", l_alloclient_get_time},
    {"get_server_time", l_alloclient_get_server_time},
    {"get_latency", l_alloclient_get_latency},
    {"get_clock_delta", l_alloclient_get_clock_delta},
    {"get_stats", l_alloclient_get_stats},
    
    {"asset_request", l_alloclient_asset_request},
    {"asset_send", l_alloclient_asset_send},
    {"set_asset_state_callback", l_alloclient_set_asset_state_callback},
    {"set_asset_request_callback", l_alloclient_set_asset_request_callback},
    {"set_asset_receive_callback", l_alloclient_set_asset_receive_callback},
    
    {NULL, NULL}
};

int luaopen_liballonet (lua_State *L)
{
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

int luaopen_allonet(lua_State* L)
{
    return luaopen_liballonet(L);
}


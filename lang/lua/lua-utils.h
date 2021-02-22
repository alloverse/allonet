#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "lua-weak.h"
#include "../../include/allonet/client.h"
#include "../../include/allonet/server.h"

// Utility APIs:
char *get_table_string(lua_State *L, const char *key); // returns owning reference
void set_table_string(lua_State *L, const char *key, const char *value);
double get_table_number(lua_State *L, const char *key);
double get_table_inumber(lua_State *L, int key);
bool store_function(lua_State *L, int *storage);
bool get_function(lua_State *L, int storage);
void push_interaction_table(lua_State *L, allo_interaction *inter);
void push_state_table(lua_State *L, allo_state *state);
void push_matrix_table(lua_State *L, allo_m4x4 m);
allo_client_intent* get_intent(lua_State *L);
allo_client_poses get_table_poses(lua_State *L, const char *key);
allo_client_hand_pose get_table_hand_pose(lua_State *L, const char *key);
allo_client_plain_pose get_table_head_pose(lua_State *L, const char *key);
allo_client_pose_grab get_table_grab(lua_State* L, const char* key);
void get_table_skeleton(lua_State *L, const char *key, allo_m4x4 skeleton[ALLO_HAND_SKELETON_JOINT_COUNT]);
allo_vector get_table_vector(lua_State *L, const char *key);
allo_m4x4 get_table_imatrix(lua_State* L, int idx);
allo_m4x4 get_table_matrix(lua_State* L, const char* key);

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "../../include/allonet/client.h"

const char *get_table_string(lua_State *L, const char *key);
void set_table_string(lua_State *L, const char *key, const char *value);
double get_table_number(lua_State *L, const char *key);
bool store_function(lua_State *L, int *storage);
bool get_function(lua_State *L, int storage);
void push_interaction_table(lua_State *L, allo_interaction *inter);
void push_state_table(lua_State *L, allo_state *state);
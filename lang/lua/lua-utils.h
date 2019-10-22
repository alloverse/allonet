#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "../../include/allonet/client.h"

// Utility APIs:
const char *get_table_string(lua_State *L, const char *key);
void set_table_string(lua_State *L, const char *key, const char *value);
double get_table_number(lua_State *L, const char *key);
bool store_function(lua_State *L, int *storage);
bool get_function(lua_State *L, int storage);
void push_interaction_table(lua_State *L, allo_interaction *inter);
void push_state_table(lua_State *L, allo_state *state);





// on MacOS, -Wl,-undefined,dynamic_lookup works fine. But on linux I can't get it to work :S
// generated with:
/*
for x in luaL_checkinteger lua_settop luaL_argerror lua_pcall luaL_ref lua_createtable luaL_checknumber lua_rawgeti luaL_register lua_pushinteger lua_isnumber luaL_checklstring lua_pushvalue lua_gettable lua_newuserdata lua_setmetatable lua_pushnumber lua_settable lua_pushboolean luaL_checkudata luaL_error lua_pushnil lua_type luaL_newmetatable luaL_unref lua_pushstring lua_getfield lua_isstring
do
    grep -hR --include \*.h $x lib/lua/src | grep -v define | grep _API
done
*/
lua_Integer luaL_checkinteger (lua_State *L, int narg) __attribute__((weak));
LUA_API void  (lua_settop) (lua_State *L, int idx) __attribute__((weak));
LUALIB_API int (luaL_argerror) (lua_State *L, int numarg, const char *extramsg) __attribute__((weak));
LUA_API int   (lua_pcall) (lua_State *L, int nargs, int nresults, int errfunc) __attribute__((weak));
LUALIB_API int (luaL_ref) (lua_State *L, int t) __attribute__((weak));
LUA_API void  (lua_createtable) (lua_State *L, int narr, int nrec) __attribute__((weak));
LUALIB_API lua_Number (luaL_checknumber) (lua_State *L, int numArg) __attribute__((weak));
LUA_API void  (lua_rawgeti) (lua_State *L, int idx, int n) __attribute__((weak));
void luaL_register (lua_State *L,
                    const char *libname,
                    const luaL_Reg *l) __attribute__((weak));
LUA_API void  (lua_pushinteger) (lua_State *L, lua_Integer n) __attribute__((weak));
LUA_API int             (lua_isnumber) (lua_State *L, int idx) __attribute__((weak));
const char *luaL_checklstring (lua_State *L, int narg, size_t *l) __attribute__((weak));
LUA_API void  (lua_pushvalue) (lua_State *L, int idx) __attribute__((weak));
LUA_API void  (lua_gettable) (lua_State *L, int idx) __attribute__((weak));
LUA_API void *(lua_newuserdata) (lua_State *L, size_t sz) __attribute__((weak));
LUA_API int   (lua_setmetatable) (lua_State *L, int objindex) __attribute__((weak));
LUA_API void  (lua_pushnumber) (lua_State *L, lua_Number n) __attribute__((weak));
LUA_API void  (lua_settable) (lua_State *L, int idx) __attribute__((weak));
LUA_API void  (lua_pushboolean) (lua_State *L, int b) __attribute__((weak));
LUALIB_API void *(luaL_checkudata) (lua_State *L, int ud, const char *tname) __attribute__((weak));
LUALIB_API int (luaL_error) (lua_State *L, const char *fmt, ...) __attribute__((weak));
LUA_API void  (lua_pushnil) (lua_State *L) __attribute__((weak));
LUA_API int             (lua_type) (lua_State *L, int idx) __attribute__((weak));
LUA_API const char     *(lua_typename) (lua_State *L, int tp) __attribute__((weak));
LUALIB_API int   (luaL_newmetatable) (lua_State *L, const char *tname) __attribute__((weak));
LUALIB_API void (luaL_unref) (lua_State *L, int t, int ref) __attribute__((weak));
LUA_API void  (lua_pushstring) (lua_State *L, const char *s) __attribute__((weak));
LUA_API void  (lua_getfield) (lua_State *L, int idx, const char *k) __attribute__((weak));
LUA_API int             (lua_isstring) (lua_State *L, int idx) __attribute__((weak));

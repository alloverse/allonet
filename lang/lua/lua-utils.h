#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "../../include/allonet/client.h"

// Utility APIs:
const char *get_table_string(lua_State *L, const char *key);
void set_table_string(lua_State *L, const char *key, const char *value);
double get_table_number(lua_State *L, const char *key);
double get_table_inumber(lua_State *L, int key);
bool store_function(lua_State *L, int *storage);
bool get_function(lua_State *L, int storage);
void push_interaction_table(lua_State *L, allo_interaction *inter);
void push_state_table(lua_State *L, allo_state *state);
allo_client_poses get_table_poses(lua_State *L, const char *key);
allo_client_pose get_table_pose(lua_State *L, const char *key);
allo_vector get_table_vector(lua_State *L, const char *key);





// on MacOS, -Wl,-undefined,dynamic_lookup works fine. But on linux I can't get it to work :S
// generated with:
/*
for x in luaL_checkinteger lua_settop luaL_argerror lua_pcall luaL_ref lua_createtable luaL_checknumber lua_rawgeti luaL_register lua_pushinteger lua_isnumber luaL_checklstring lua_pushvalue lua_gettable lua_newuserdata lua_setmetatable lua_pushnumber lua_settable lua_pushboolean luaL_checkudata luaL_error lua_pushnil lua_type luaL_newmetatable luaL_unref lua_pushstring lua_getfield lua_isstring
do
    grep -hR --include \*.h $x lib/lua/src | grep -v define | grep _API
done
*/

// TODO: On Windows, there is no weak linking?! So now allonet depends on lua.dll :(

#ifdef CLANG
	#define WEAK_ATTRIBUTE WEAK_ATTRIBUTE
#else
	#define WEAK_ATTRIBUTE 
#endif

lua_Integer luaL_checkinteger (lua_State *L, int narg) WEAK_ATTRIBUTE;
LUA_API void  (lua_settop) (lua_State *L, int idx) WEAK_ATTRIBUTE;
LUALIB_API int (luaL_argerror) (lua_State *L, int numarg, const char *extramsg) WEAK_ATTRIBUTE;
LUA_API int   (lua_pcall) (lua_State *L, int nargs, int nresults, int errfunc) WEAK_ATTRIBUTE;
LUALIB_API int (luaL_ref) (lua_State *L, int t) WEAK_ATTRIBUTE;
LUA_API void  (lua_createtable) (lua_State *L, int narr, int nrec) WEAK_ATTRIBUTE;
LUALIB_API lua_Number (luaL_checknumber) (lua_State *L, int numArg) WEAK_ATTRIBUTE;
LUA_API void  (lua_rawgeti) (lua_State *L, int idx, int n) WEAK_ATTRIBUTE;
void luaL_register (lua_State *L,
                    const char *libname,
                    const luaL_Reg *l) WEAK_ATTRIBUTE;
LUA_API void  (lua_pushinteger) (lua_State *L, lua_Integer n) WEAK_ATTRIBUTE;
LUA_API int             (lua_isnumber) (lua_State *L, int idx) WEAK_ATTRIBUTE;
const char *luaL_checklstring (lua_State *L, int narg, size_t *l) WEAK_ATTRIBUTE;
LUA_API void  (lua_pushvalue) (lua_State *L, int idx) WEAK_ATTRIBUTE;
LUA_API void  (lua_gettable) (lua_State *L, int idx) WEAK_ATTRIBUTE;
LUA_API void *(lua_newuserdata) (lua_State *L, size_t sz) WEAK_ATTRIBUTE;
LUA_API int   (lua_setmetatable) (lua_State *L, int objindex) WEAK_ATTRIBUTE;
LUA_API void  (lua_pushnumber) (lua_State *L, lua_Number n) WEAK_ATTRIBUTE;
LUA_API void  (lua_settable) (lua_State *L, int idx) WEAK_ATTRIBUTE;
LUA_API void  (lua_pushboolean) (lua_State *L, int b) WEAK_ATTRIBUTE;
LUALIB_API void *(luaL_checkudata) (lua_State *L, int ud, const char *tname) WEAK_ATTRIBUTE;
LUALIB_API int (luaL_error) (lua_State *L, const char *fmt, ...) WEAK_ATTRIBUTE;
LUA_API void  (lua_pushnil) (lua_State *L) WEAK_ATTRIBUTE;
LUA_API int             (lua_type) (lua_State *L, int idx) WEAK_ATTRIBUTE;
LUA_API const char     *(lua_typename) (lua_State *L, int tp) WEAK_ATTRIBUTE;
LUALIB_API int   (luaL_newmetatable) (lua_State *L, const char *tname) WEAK_ATTRIBUTE;
LUALIB_API void (luaL_unref) (lua_State *L, int t, int ref) WEAK_ATTRIBUTE;
LUA_API void  (lua_pushstring) (lua_State *L, const char *s) WEAK_ATTRIBUTE;
LUA_API void  (lua_getfield) (lua_State *L, int idx, const char *k) WEAK_ATTRIBUTE;
LUA_API int             (lua_isstring) (lua_State *L, int idx) WEAK_ATTRIBUTE;

// Do not modify. Generated by generate-lua-weak-h.lua
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#ifdef __APPLE__
#endif
#if __clang__ || defined(__GNUC__)
#define WEAK_ATTRIBUTE __attribute__((weak))
void   lua_call (lua_State *L, int nargs, int nresults) WEAK_ATTRIBUTE;
const char     *lua_typename (lua_State *L, int tp) WEAK_ATTRIBUTE;
void  lua_pushstring (lua_State *L, const char *s) WEAK_ATTRIBUTE;
void  lua_pushnumber (lua_State *L, lua_Number n) WEAK_ATTRIBUTE;
void  lua_pushboolean (lua_State *L, int b) WEAK_ATTRIBUTE;
lua_Number luaL_checknumber (lua_State *L, int numArg) WEAK_ATTRIBUTE;
void lua_pushlstring (lua_State *L, const char *s, size_t len) WEAK_ATTRIBUTE;
void luaL_unref (lua_State *L, int t, int ref) WEAK_ATTRIBUTE;
int lua_gettop (lua_State *L) WEAK_ATTRIBUTE;
int   lua_pcall (lua_State *L, int nargs, int nresults, int errfunc) WEAK_ATTRIBUTE;
int luaL_error (lua_State *L, const char *fmt, ...) WEAK_ATTRIBUTE;
void  lua_pushnil (lua_State *L) WEAK_ATTRIBUTE;
int luaL_argerror (lua_State *L, int numarg, const char *extramsg) WEAK_ATTRIBUTE;
const void *lua_topointer (lua_State *L, int index) WEAK_ATTRIBUTE;
void  lua_gettable (lua_State *L, int idx) WEAK_ATTRIBUTE;
void  lua_pushinteger (lua_State *L, lua_Integer n) WEAK_ATTRIBUTE;
const char *luaL_checklstring (lua_State *L, int narg, size_t *l) WEAK_ATTRIBUTE;
void  lua_createtable (lua_State *L, int narr, int nrec) WEAK_ATTRIBUTE;
lua_Integer luaL_optinteger(lua_State *L, int nArg, lua_Integer def) WEAK_ATTRIBUTE;
void  lua_settable (lua_State *L, int idx) WEAK_ATTRIBUTE;
int luaL_ref (lua_State *L, int t) WEAK_ATTRIBUTE;
int lua_toboolean (lua_State *L, int index) WEAK_ATTRIBUTE;
const char *lua_tolstring (lua_State *L, int index, size_t *len) WEAK_ATTRIBUTE;
lua_Number lua_tonumber (lua_State *L, int index) WEAK_ATTRIBUTE;
const char *luaL_optlstring (lua_State *L, int narg, const char *d, size_t *l) WEAK_ATTRIBUTE;
int             lua_type (lua_State *L, int idx) WEAK_ATTRIBUTE;
int             lua_isnumber (lua_State *L, int idx) WEAK_ATTRIBUTE;
int   luaL_newmetatable (lua_State *L, const char *tname) WEAK_ATTRIBUTE;
void  lua_settop (lua_State *L, int idx) WEAK_ATTRIBUTE;
void *lua_newuserdata (lua_State *L, size_t sz) WEAK_ATTRIBUTE;
void  lua_getfield (lua_State *L, int idx, const char *k) WEAK_ATTRIBUTE;
int             lua_isstring (lua_State *L, int idx) WEAK_ATTRIBUTE;
void luaL_register (lua_State *L, const char *libname, const luaL_Reg *l) WEAK_ATTRIBUTE;
lua_Integer luaL_checkinteger (lua_State *L, int narg) WEAK_ATTRIBUTE;
void *luaL_checkudata (lua_State *L, int ud, const char *tname) WEAK_ATTRIBUTE;
int   lua_setmetatable (lua_State *L, int objindex) WEAK_ATTRIBUTE;
void  lua_rawgeti (lua_State *L, int idx, int n) WEAK_ATTRIBUTE;
void  lua_rawseti (lua_State *L, int idx, int n) WEAK_ATTRIBUTE;
void  lua_pushvalue (lua_State *L, int idx) WEAK_ATTRIBUTE;
void lua_pushlightuserdata (lua_State *L, void *p) WEAK_ATTRIBUTE;
#endif
#ifdef WIN32
extern void   (*lua_call_weak) (lua_State *L, int nargs, int nresults);
#define lua_call lua_call_weak
extern const char     *(*lua_typename_weak) (lua_State *L, int tp);
#define lua_typename lua_typename_weak
extern void  (*lua_pushstring_weak) (lua_State *L, const char *s);
#define lua_pushstring lua_pushstring_weak
extern void  (*lua_pushnumber_weak) (lua_State *L, lua_Number n);
#define lua_pushnumber lua_pushnumber_weak
extern void  (*lua_pushboolean_weak) (lua_State *L, int b);
#define lua_pushboolean lua_pushboolean_weak
extern lua_Number (*luaL_checknumber_weak) (lua_State *L, int numArg);
#define luaL_checknumber luaL_checknumber_weak
extern void (*lua_pushlstring_weak) (lua_State *L, const char *s, size_t len);
#define lua_pushlstring lua_pushlstring_weak
extern void (*luaL_unref_weak) (lua_State *L, int t, int ref);
#define luaL_unref luaL_unref_weak
extern int (*lua_gettop_weak) (lua_State *L);
#define lua_gettop lua_gettop_weak
extern int   (*lua_pcall_weak) (lua_State *L, int nargs, int nresults, int errfunc);
#define lua_pcall lua_pcall_weak
extern int (*luaL_error_weak) (lua_State *L, const char *fmt, ...);
#define luaL_error luaL_error_weak
extern void  (*lua_pushnil_weak) (lua_State *L);
#define lua_pushnil lua_pushnil_weak
extern int (*luaL_argerror_weak) (lua_State *L, int numarg, const char *extramsg);
#define luaL_argerror luaL_argerror_weak
extern const void *(*lua_topointer_weak) (lua_State *L, int index);
#define lua_topointer lua_topointer_weak
extern void  (*lua_gettable_weak) (lua_State *L, int idx);
#define lua_gettable lua_gettable_weak
extern void  (*lua_pushinteger_weak) (lua_State *L, lua_Integer n);
#define lua_pushinteger lua_pushinteger_weak
extern const char *(*luaL_checklstring_weak) (lua_State *L, int narg, size_t *l);
#define luaL_checklstring luaL_checklstring_weak
extern void  (*lua_createtable_weak) (lua_State *L, int narr, int nrec);
#define lua_createtable lua_createtable_weak
extern lua_Integer (*luaL_optinteger_weak)(lua_State *L, int nArg, lua_Integer def);
#define luaL_optinteger luaL_optinteger_weak
extern void  (*lua_settable_weak) (lua_State *L, int idx);
#define lua_settable lua_settable_weak
extern int (*luaL_ref_weak) (lua_State *L, int t);
#define luaL_ref luaL_ref_weak
extern int (*lua_toboolean_weak) (lua_State *L, int index);
#define lua_toboolean lua_toboolean_weak
extern const char *(*lua_tolstring_weak) (lua_State *L, int index, size_t *len);
#define lua_tolstring lua_tolstring_weak
extern lua_Number (*lua_tonumber_weak) (lua_State *L, int index);
#define lua_tonumber lua_tonumber_weak
extern const char *(*luaL_optlstring_weak) (lua_State *L, int narg, const char *d, size_t *l);
#define luaL_optlstring luaL_optlstring_weak
extern int             (*lua_type_weak) (lua_State *L, int idx);
#define lua_type lua_type_weak
extern int             (*lua_isnumber_weak) (lua_State *L, int idx);
#define lua_isnumber lua_isnumber_weak
extern int   (*luaL_newmetatable_weak) (lua_State *L, const char *tname);
#define luaL_newmetatable luaL_newmetatable_weak
extern void  (*lua_settop_weak) (lua_State *L, int idx);
#define lua_settop lua_settop_weak
extern void *(*lua_newuserdata_weak) (lua_State *L, size_t sz);
#define lua_newuserdata lua_newuserdata_weak
extern void  (*lua_getfield_weak) (lua_State *L, int idx, const char *k);
#define lua_getfield lua_getfield_weak
extern int             (*lua_isstring_weak) (lua_State *L, int idx);
#define lua_isstring lua_isstring_weak
extern void (*luaL_register_weak) (lua_State *L, const char *libname, const luaL_Reg *l);
#define luaL_register luaL_register_weak
extern lua_Integer (*luaL_checkinteger_weak) (lua_State *L, int narg);
#define luaL_checkinteger luaL_checkinteger_weak
extern void *(*luaL_checkudata_weak) (lua_State *L, int ud, const char *tname);
#define luaL_checkudata luaL_checkudata_weak
extern int   (*lua_setmetatable_weak) (lua_State *L, int objindex);
#define lua_setmetatable lua_setmetatable_weak
extern void  (*lua_rawgeti_weak) (lua_State *L, int idx, int n);
#define lua_rawgeti lua_rawgeti_weak
extern void  (*lua_rawseti_weak) (lua_State *L, int idx, int n);
#define lua_rawseti lua_rawseti_weak
extern void  (*lua_pushvalue_weak) (lua_State *L, int idx);
#define lua_pushvalue lua_pushvalue_weak
extern void (*lua_pushlightuserdata_weak) (lua_State *L, void *p);
#define lua_pushlightuserdata lua_pushlightuserdata_weak
#endif
extern void load_weak_lua_symbols();

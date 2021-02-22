#include "lua-weak.h"
#ifdef WIN32
void   (*lua_call_weak) (lua_State *L, int nargs, int nresults) = NULL;
const char     *(*lua_typename_weak) (lua_State *L, int tp) = NULL;
void  (*lua_pushstring_weak) (lua_State *L, const char *s) = NULL;
void  (*lua_pushnumber_weak) (lua_State *L, lua_Number n) = NULL;
void  (*lua_pushboolean_weak) (lua_State *L, int b) = NULL;
lua_Number (*luaL_checknumber_weak) (lua_State *L, int numArg) = NULL;
void (*lua_pushlstring_weak) (lua_State *L, const char *s, size_t len) = NULL;
void (*luaL_unref_weak) (lua_State *L, int t, int ref) = NULL;
int   (*lua_pcall_weak) (lua_State *L, int nargs, int nresults, int errfunc) = NULL;
int (*luaL_error_weak) (lua_State *L, const char *fmt, ...) = NULL;
void  (*lua_pushnil_weak) (lua_State *L) = NULL;
int (*luaL_argerror_weak) (lua_State *L, int numarg, const char *extramsg) = NULL;
void  (*lua_gettable_weak) (lua_State *L, int idx) = NULL;
void  (*lua_pushinteger_weak) (lua_State *L, lua_Integer n) = NULL;
const char *(*luaL_checklstring_weak) (lua_State *L, int narg, size_t *l) = NULL;
void  (*lua_createtable_weak) (lua_State *L, int narr, int nrec) = NULL;
void  (*lua_settable_weak) (lua_State *L, int idx) = NULL;
int (*lua_toboolean_weak) (lua_State *L, int index) = NULL;
const char *(*luaL_optlstring_weak) (lua_State *L, int narg, const char *d, size_t *l) = NULL;
int             (*lua_type_weak) (lua_State *L, int idx) = NULL;
int             (*lua_isnumber_weak) (lua_State *L, int idx) = NULL;
int   (*luaL_newmetatable_weak) (lua_State *L, const char *tname) = NULL;
void  (*lua_settop_weak) (lua_State *L, int idx) = NULL;
void *(*lua_newuserdata_weak) (lua_State *L, size_t sz) = NULL;
void  (*lua_getfield_weak) (lua_State *L, int idx, const char *k) = NULL;
int             (*lua_isstring_weak) (lua_State *L, int idx) = NULL;
void (*luaL_register_weak) (lua_State *L, const char *libname, const luaL_Reg *l) = NULL;
lua_Integer (*luaL_checkinteger_weak) (lua_State *L, int narg) = NULL;
void *(*luaL_checkudata_weak) (lua_State *L, int ud, const char *tname) = NULL;
int   (*lua_setmetatable_weak) (lua_State *L, int objindex) = NULL;
void  (*lua_rawgeti_weak) (lua_State *L, int idx, int n) = NULL;
void  (*lua_rawseti_weak) (lua_State *L, int idx, int n) = NULL;
void  (*lua_pushvalue_weak) (lua_State *L, int idx) = NULL;
int (*luaL_ref_weak) (lua_State *L, int t) = NULL;
#include <Windows.h>
void load_weak_lua_symbols() 
{
	LPCSTR candidateModules[] = {
		NULL,
		"lua.dll",
		"luajit.dll"
	};
	HMODULE mod;
	for (int i = 0, c = sizeof(candidateModules); i < c; i++) {
		mod = GetModuleHandle(candidateModules[i]);
		if (mod == NULL) continue;
		FARPROC testproc = GetProcAddress(mod, "luaL_register");
		if (testproc != NULL) break;
	}
	if(!mod) {
		fprintf(stderr, "Failed to load weak lua symbols");
		return;
	}
	
	lua_call_weak = (void   (*) (lua_State *L, int nargs, int nresults))GetProcAddress(mod, "lua_call");
	lua_typename_weak = (const char     *(*) (lua_State *L, int tp))GetProcAddress(mod, "lua_typename");
	lua_pushstring_weak = (void  (*) (lua_State *L, const char *s))GetProcAddress(mod, "lua_pushstring");
	lua_pushnumber_weak = (void  (*) (lua_State *L, lua_Number n))GetProcAddress(mod, "lua_pushnumber");
	lua_pushboolean_weak = (void  (*) (lua_State *L, int b))GetProcAddress(mod, "lua_pushboolean");
	luaL_checknumber_weak = (lua_Number (*) (lua_State *L, int numArg))GetProcAddress(mod, "luaL_checknumber");
	lua_pushlstring_weak = (void (*) (lua_State *L, const char *s, size_t len))GetProcAddress(mod, "lua_pushlstring");
	luaL_unref_weak = (void (*) (lua_State *L, int t, int ref))GetProcAddress(mod, "luaL_unref");
	lua_pcall_weak = (int   (*) (lua_State *L, int nargs, int nresults, int errfunc))GetProcAddress(mod, "lua_pcall");
	luaL_error_weak = (int (*) (lua_State *L, const char *fmt, ...))GetProcAddress(mod, "luaL_error");
	lua_pushnil_weak = (void  (*) (lua_State *L))GetProcAddress(mod, "lua_pushnil");
	luaL_argerror_weak = (int (*) (lua_State *L, int numarg, const char *extramsg))GetProcAddress(mod, "luaL_argerror");
	lua_gettable_weak = (void  (*) (lua_State *L, int idx))GetProcAddress(mod, "lua_gettable");
	lua_pushinteger_weak = (void  (*) (lua_State *L, lua_Integer n))GetProcAddress(mod, "lua_pushinteger");
	luaL_checklstring_weak = (const char *(*) (lua_State *L, int narg, size_t *l))GetProcAddress(mod, "luaL_checklstring");
	lua_createtable_weak = (void  (*) (lua_State *L, int narr, int nrec))GetProcAddress(mod, "lua_createtable");
	lua_settable_weak = (void  (*) (lua_State *L, int idx))GetProcAddress(mod, "lua_settable");
	lua_toboolean_weak = (int (*) (lua_State *L, int index))GetProcAddress(mod, "lua_toboolean");
	luaL_optlstring_weak = (const char *(*) (lua_State *L, int narg, const char *d, size_t *l))GetProcAddress(mod, "luaL_optlstring");
	lua_type_weak = (int             (*) (lua_State *L, int idx))GetProcAddress(mod, "lua_type");
	lua_isnumber_weak = (int             (*) (lua_State *L, int idx))GetProcAddress(mod, "lua_isnumber");
	luaL_newmetatable_weak = (int   (*) (lua_State *L, const char *tname))GetProcAddress(mod, "luaL_newmetatable");
	lua_settop_weak = (void  (*) (lua_State *L, int idx))GetProcAddress(mod, "lua_settop");
	lua_newuserdata_weak = (void *(*) (lua_State *L, size_t sz))GetProcAddress(mod, "lua_newuserdata");
	lua_getfield_weak = (void  (*) (lua_State *L, int idx, const char *k))GetProcAddress(mod, "lua_getfield");
	lua_isstring_weak = (int             (*) (lua_State *L, int idx))GetProcAddress(mod, "lua_isstring");
	luaL_register_weak = (void (*) (lua_State *L, const char *libname, const luaL_Reg *l))GetProcAddress(mod, "luaL_register");
	luaL_checkinteger_weak = (lua_Integer (*) (lua_State *L, int narg))GetProcAddress(mod, "luaL_checkinteger");
	luaL_checkudata_weak = (void *(*) (lua_State *L, int ud, const char *tname))GetProcAddress(mod, "luaL_checkudata");
	lua_setmetatable_weak = (int   (*) (lua_State *L, int objindex))GetProcAddress(mod, "lua_setmetatable");
	lua_rawgeti_weak = (void  (*) (lua_State *L, int idx, int n))GetProcAddress(mod, "lua_rawgeti");
	lua_rawseti_weak = (void  (*) (lua_State *L, int idx, int n))GetProcAddress(mod, "lua_rawseti");
	lua_pushvalue_weak = (void  (*) (lua_State *L, int idx))GetProcAddress(mod, "lua_pushvalue");
	luaL_ref_weak = (int (*) (lua_State *L, int t))GetProcAddress(mod, "luaL_ref");
}
#else
void load_weak_lua_symbols() {}
#endif

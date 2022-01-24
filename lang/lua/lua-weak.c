#include "lua-weak.h"
#ifdef __APPLE__
void  (*lua_rawgeti_weak) (lua_State *L, int idx, int n) = NULL;
void   (*lua_call_weak) (lua_State *L, int nargs, int nresults) = NULL;
const char *(*luaL_optlstring_weak) (lua_State *L, int narg, const char *d, size_t *l) = NULL;
int (*luaL_ref_weak) (lua_State *L, int t) = NULL;
int (*lua_gettop_weak) (lua_State *L) = NULL;
void *(*luaL_checkudata_weak) (lua_State *L, int ud, const char *tname) = NULL;
int   (*lua_pcall_weak) (lua_State *L, int nargs, int nresults, int errfunc) = NULL;
lua_Number (*luaL_checknumber_weak) (lua_State *L, int numArg) = NULL;
void  (*lua_settop_weak) (lua_State *L, int idx) = NULL;
void  (*lua_pushstring_weak) (lua_State *L, const char *s) = NULL;
int (*lua_gc_weak)(lua_State *L, int what, int data) = NULL;
void  (*lua_pushvalue_weak) (lua_State *L, int idx) = NULL;
lua_Integer (*luaL_optinteger_weak)(lua_State *L, int nArg, lua_Integer def) = NULL;
void  (*lua_pushinteger_weak) (lua_State *L, lua_Integer n) = NULL;
int (*lua_toboolean_weak) (lua_State *L, int index) = NULL;
void  (*lua_createtable_weak) (lua_State *L, int narr, int nrec) = NULL;
void  (*lua_rawseti_weak) (lua_State *L, int idx, int n) = NULL;
int             (*lua_isstring_weak) (lua_State *L, int idx) = NULL;
int             (*lua_isnumber_weak) (lua_State *L, int idx) = NULL;
int (*luaL_argerror_weak) (lua_State *L, int numarg, const char *extramsg) = NULL;
int             (*lua_type_weak) (lua_State *L, int idx) = NULL;
void  (*lua_pushnil_weak) (lua_State *L) = NULL;
lua_Integer (*luaL_checkinteger_weak) (lua_State *L, int narg) = NULL;
void (*lua_pushlightuserdata_weak) (lua_State *L, void *p) = NULL;
const void *(*lua_topointer_weak) (lua_State *L, int index) = NULL;
void  (*lua_settable_weak) (lua_State *L, int idx) = NULL;
lua_Number (*lua_tonumber_weak) (lua_State *L, int index) = NULL;
int   (*luaL_newmetatable_weak) (lua_State *L, const char *tname) = NULL;
int (*luaL_error_weak) (lua_State *L, const char *fmt, ...) = NULL;
void  (*lua_pushnumber_weak) (lua_State *L, lua_Number n) = NULL;
void  (*lua_gettable_weak) (lua_State *L, int idx) = NULL;
void (*luaL_unref_weak) (lua_State *L, int t, int ref) = NULL;
void  (*lua_getfield_weak) (lua_State *L, int idx, const char *k) = NULL;
const char     *(*lua_typename_weak) (lua_State *L, int tp) = NULL;
void (*luaL_register_weak) (lua_State *L, const char *libname, const luaL_Reg *l) = NULL;
void  (*lua_pushboolean_weak) (lua_State *L, int b) = NULL;
const char *(*lua_tolstring_weak) (lua_State *L, int index, size_t *len) = NULL;
const char *(*luaL_checklstring_weak) (lua_State *L, int narg, size_t *l) = NULL;
int   (*lua_setmetatable_weak) (lua_State *L, int objindex) = NULL;
void *(*lua_newuserdata_weak) (lua_State *L, size_t sz) = NULL;
void (*lua_pushlstring_weak) (lua_State *L, const char *s, size_t len) = NULL;
#include <dlfcn.h>
#include <assert.h>
void load_weak_lua_symbols() 
{
	void* proc = dlopen(NULL, RTLD_NOW);
	assert(proc && "proc not found");

	lua_rawgeti_weak = (void  (*) (lua_State *L, int idx, int n))dlsym(proc, "lua_rawgeti");
	lua_call_weak = (void   (*) (lua_State *L, int nargs, int nresults))dlsym(proc, "lua_call");
	luaL_optlstring_weak = (const char *(*) (lua_State *L, int narg, const char *d, size_t *l))dlsym(proc, "luaL_optlstring");
	luaL_ref_weak = (int (*) (lua_State *L, int t))dlsym(proc, "luaL_ref");
	lua_gettop_weak = (int (*) (lua_State *L))dlsym(proc, "lua_gettop");
	luaL_checkudata_weak = (void *(*) (lua_State *L, int ud, const char *tname))dlsym(proc, "luaL_checkudata");
	lua_pcall_weak = (int   (*) (lua_State *L, int nargs, int nresults, int errfunc))dlsym(proc, "lua_pcall");
	luaL_checknumber_weak = (lua_Number (*) (lua_State *L, int numArg))dlsym(proc, "luaL_checknumber");
	lua_settop_weak = (void  (*) (lua_State *L, int idx))dlsym(proc, "lua_settop");
	lua_pushstring_weak = (void  (*) (lua_State *L, const char *s))dlsym(proc, "lua_pushstring");
	lua_gc_weak = (int (*)(lua_State *L, int what, int data))dlsym(proc, "lua_gc");
	lua_pushvalue_weak = (void  (*) (lua_State *L, int idx))dlsym(proc, "lua_pushvalue");
	luaL_optinteger_weak = (lua_Integer (*)(lua_State *L, int nArg, lua_Integer def))dlsym(proc, "luaL_optinteger");
	lua_pushinteger_weak = (void  (*) (lua_State *L, lua_Integer n))dlsym(proc, "lua_pushinteger");
	lua_toboolean_weak = (int (*) (lua_State *L, int index))dlsym(proc, "lua_toboolean");
	lua_createtable_weak = (void  (*) (lua_State *L, int narr, int nrec))dlsym(proc, "lua_createtable");
	lua_rawseti_weak = (void  (*) (lua_State *L, int idx, int n))dlsym(proc, "lua_rawseti");
	lua_isstring_weak = (int             (*) (lua_State *L, int idx))dlsym(proc, "lua_isstring");
	lua_isnumber_weak = (int             (*) (lua_State *L, int idx))dlsym(proc, "lua_isnumber");
	luaL_argerror_weak = (int (*) (lua_State *L, int numarg, const char *extramsg))dlsym(proc, "luaL_argerror");
	lua_type_weak = (int             (*) (lua_State *L, int idx))dlsym(proc, "lua_type");
	lua_pushnil_weak = (void  (*) (lua_State *L))dlsym(proc, "lua_pushnil");
	luaL_checkinteger_weak = (lua_Integer (*) (lua_State *L, int narg))dlsym(proc, "luaL_checkinteger");
	lua_pushlightuserdata_weak = (void (*) (lua_State *L, void *p))dlsym(proc, "lua_pushlightuserdata");
	lua_topointer_weak = (const void *(*) (lua_State *L, int index))dlsym(proc, "lua_topointer");
	lua_settable_weak = (void  (*) (lua_State *L, int idx))dlsym(proc, "lua_settable");
	lua_tonumber_weak = (lua_Number (*) (lua_State *L, int index))dlsym(proc, "lua_tonumber");
	luaL_newmetatable_weak = (int   (*) (lua_State *L, const char *tname))dlsym(proc, "luaL_newmetatable");
	luaL_error_weak = (int (*) (lua_State *L, const char *fmt, ...))dlsym(proc, "luaL_error");
	lua_pushnumber_weak = (void  (*) (lua_State *L, lua_Number n))dlsym(proc, "lua_pushnumber");
	lua_gettable_weak = (void  (*) (lua_State *L, int idx))dlsym(proc, "lua_gettable");
	luaL_unref_weak = (void (*) (lua_State *L, int t, int ref))dlsym(proc, "luaL_unref");
	lua_getfield_weak = (void  (*) (lua_State *L, int idx, const char *k))dlsym(proc, "lua_getfield");
	lua_typename_weak = (const char     *(*) (lua_State *L, int tp))dlsym(proc, "lua_typename");
	luaL_register_weak = (void (*) (lua_State *L, const char *libname, const luaL_Reg *l))dlsym(proc, "luaL_register");
	lua_pushboolean_weak = (void  (*) (lua_State *L, int b))dlsym(proc, "lua_pushboolean");
	lua_tolstring_weak = (const char *(*) (lua_State *L, int index, size_t *len))dlsym(proc, "lua_tolstring");
	luaL_checklstring_weak = (const char *(*) (lua_State *L, int narg, size_t *l))dlsym(proc, "luaL_checklstring");
	lua_setmetatable_weak = (int   (*) (lua_State *L, int objindex))dlsym(proc, "lua_setmetatable");
	lua_newuserdata_weak = (void *(*) (lua_State *L, size_t sz))dlsym(proc, "lua_newuserdata");
	lua_pushlstring_weak = (void (*) (lua_State *L, const char *s, size_t len))dlsym(proc, "lua_pushlstring");
}
#elif defined(WIN32)
void  (*lua_rawgeti_weak) (lua_State *L, int idx, int n) = NULL;
void   (*lua_call_weak) (lua_State *L, int nargs, int nresults) = NULL;
const char *(*luaL_optlstring_weak) (lua_State *L, int narg, const char *d, size_t *l) = NULL;
int (*luaL_ref_weak) (lua_State *L, int t) = NULL;
int (*lua_gettop_weak) (lua_State *L) = NULL;
void *(*luaL_checkudata_weak) (lua_State *L, int ud, const char *tname) = NULL;
int   (*lua_pcall_weak) (lua_State *L, int nargs, int nresults, int errfunc) = NULL;
lua_Number (*luaL_checknumber_weak) (lua_State *L, int numArg) = NULL;
void  (*lua_settop_weak) (lua_State *L, int idx) = NULL;
void  (*lua_pushstring_weak) (lua_State *L, const char *s) = NULL;
int (*lua_gc_weak)(lua_State *L, int what, int data) = NULL;
void  (*lua_pushvalue_weak) (lua_State *L, int idx) = NULL;
lua_Integer (*luaL_optinteger_weak)(lua_State *L, int nArg, lua_Integer def) = NULL;
void  (*lua_pushinteger_weak) (lua_State *L, lua_Integer n) = NULL;
int (*lua_toboolean_weak) (lua_State *L, int index) = NULL;
void  (*lua_createtable_weak) (lua_State *L, int narr, int nrec) = NULL;
void  (*lua_rawseti_weak) (lua_State *L, int idx, int n) = NULL;
int             (*lua_isstring_weak) (lua_State *L, int idx) = NULL;
int             (*lua_isnumber_weak) (lua_State *L, int idx) = NULL;
int (*luaL_argerror_weak) (lua_State *L, int numarg, const char *extramsg) = NULL;
int             (*lua_type_weak) (lua_State *L, int idx) = NULL;
void  (*lua_pushnil_weak) (lua_State *L) = NULL;
lua_Integer (*luaL_checkinteger_weak) (lua_State *L, int narg) = NULL;
void (*lua_pushlightuserdata_weak) (lua_State *L, void *p) = NULL;
const void *(*lua_topointer_weak) (lua_State *L, int index) = NULL;
void  (*lua_settable_weak) (lua_State *L, int idx) = NULL;
lua_Number (*lua_tonumber_weak) (lua_State *L, int index) = NULL;
int   (*luaL_newmetatable_weak) (lua_State *L, const char *tname) = NULL;
int (*luaL_error_weak) (lua_State *L, const char *fmt, ...) = NULL;
void  (*lua_pushnumber_weak) (lua_State *L, lua_Number n) = NULL;
void  (*lua_gettable_weak) (lua_State *L, int idx) = NULL;
void (*luaL_unref_weak) (lua_State *L, int t, int ref) = NULL;
void  (*lua_getfield_weak) (lua_State *L, int idx, const char *k) = NULL;
const char     *(*lua_typename_weak) (lua_State *L, int tp) = NULL;
void (*luaL_register_weak) (lua_State *L, const char *libname, const luaL_Reg *l) = NULL;
void  (*lua_pushboolean_weak) (lua_State *L, int b) = NULL;
const char *(*lua_tolstring_weak) (lua_State *L, int index, size_t *len) = NULL;
const char *(*luaL_checklstring_weak) (lua_State *L, int narg, size_t *l) = NULL;
int   (*lua_setmetatable_weak) (lua_State *L, int objindex) = NULL;
void *(*lua_newuserdata_weak) (lua_State *L, size_t sz) = NULL;
void (*lua_pushlstring_weak) (lua_State *L, const char *s, size_t len) = NULL;
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
	
	lua_rawgeti_weak = (void  (*) (lua_State *L, int idx, int n))GetProcAddress(mod, "lua_rawgeti");
	lua_call_weak = (void   (*) (lua_State *L, int nargs, int nresults))GetProcAddress(mod, "lua_call");
	luaL_optlstring_weak = (const char *(*) (lua_State *L, int narg, const char *d, size_t *l))GetProcAddress(mod, "luaL_optlstring");
	luaL_ref_weak = (int (*) (lua_State *L, int t))GetProcAddress(mod, "luaL_ref");
	lua_gettop_weak = (int (*) (lua_State *L))GetProcAddress(mod, "lua_gettop");
	luaL_checkudata_weak = (void *(*) (lua_State *L, int ud, const char *tname))GetProcAddress(mod, "luaL_checkudata");
	lua_pcall_weak = (int   (*) (lua_State *L, int nargs, int nresults, int errfunc))GetProcAddress(mod, "lua_pcall");
	luaL_checknumber_weak = (lua_Number (*) (lua_State *L, int numArg))GetProcAddress(mod, "luaL_checknumber");
	lua_settop_weak = (void  (*) (lua_State *L, int idx))GetProcAddress(mod, "lua_settop");
	lua_pushstring_weak = (void  (*) (lua_State *L, const char *s))GetProcAddress(mod, "lua_pushstring");
	lua_gc_weak = (int (*)(lua_State *L, int what, int data))GetProcAddress(mod, "lua_gc");
	lua_pushvalue_weak = (void  (*) (lua_State *L, int idx))GetProcAddress(mod, "lua_pushvalue");
	luaL_optinteger_weak = (lua_Integer (*)(lua_State *L, int nArg, lua_Integer def))GetProcAddress(mod, "luaL_optinteger");
	lua_pushinteger_weak = (void  (*) (lua_State *L, lua_Integer n))GetProcAddress(mod, "lua_pushinteger");
	lua_toboolean_weak = (int (*) (lua_State *L, int index))GetProcAddress(mod, "lua_toboolean");
	lua_createtable_weak = (void  (*) (lua_State *L, int narr, int nrec))GetProcAddress(mod, "lua_createtable");
	lua_rawseti_weak = (void  (*) (lua_State *L, int idx, int n))GetProcAddress(mod, "lua_rawseti");
	lua_isstring_weak = (int             (*) (lua_State *L, int idx))GetProcAddress(mod, "lua_isstring");
	lua_isnumber_weak = (int             (*) (lua_State *L, int idx))GetProcAddress(mod, "lua_isnumber");
	luaL_argerror_weak = (int (*) (lua_State *L, int numarg, const char *extramsg))GetProcAddress(mod, "luaL_argerror");
	lua_type_weak = (int             (*) (lua_State *L, int idx))GetProcAddress(mod, "lua_type");
	lua_pushnil_weak = (void  (*) (lua_State *L))GetProcAddress(mod, "lua_pushnil");
	luaL_checkinteger_weak = (lua_Integer (*) (lua_State *L, int narg))GetProcAddress(mod, "luaL_checkinteger");
	lua_pushlightuserdata_weak = (void (*) (lua_State *L, void *p))GetProcAddress(mod, "lua_pushlightuserdata");
	lua_topointer_weak = (const void *(*) (lua_State *L, int index))GetProcAddress(mod, "lua_topointer");
	lua_settable_weak = (void  (*) (lua_State *L, int idx))GetProcAddress(mod, "lua_settable");
	lua_tonumber_weak = (lua_Number (*) (lua_State *L, int index))GetProcAddress(mod, "lua_tonumber");
	luaL_newmetatable_weak = (int   (*) (lua_State *L, const char *tname))GetProcAddress(mod, "luaL_newmetatable");
	luaL_error_weak = (int (*) (lua_State *L, const char *fmt, ...))GetProcAddress(mod, "luaL_error");
	lua_pushnumber_weak = (void  (*) (lua_State *L, lua_Number n))GetProcAddress(mod, "lua_pushnumber");
	lua_gettable_weak = (void  (*) (lua_State *L, int idx))GetProcAddress(mod, "lua_gettable");
	luaL_unref_weak = (void (*) (lua_State *L, int t, int ref))GetProcAddress(mod, "luaL_unref");
	lua_getfield_weak = (void  (*) (lua_State *L, int idx, const char *k))GetProcAddress(mod, "lua_getfield");
	lua_typename_weak = (const char     *(*) (lua_State *L, int tp))GetProcAddress(mod, "lua_typename");
	luaL_register_weak = (void (*) (lua_State *L, const char *libname, const luaL_Reg *l))GetProcAddress(mod, "luaL_register");
	lua_pushboolean_weak = (void  (*) (lua_State *L, int b))GetProcAddress(mod, "lua_pushboolean");
	lua_tolstring_weak = (const char *(*) (lua_State *L, int index, size_t *len))GetProcAddress(mod, "lua_tolstring");
	luaL_checklstring_weak = (const char *(*) (lua_State *L, int narg, size_t *l))GetProcAddress(mod, "luaL_checklstring");
	lua_setmetatable_weak = (int   (*) (lua_State *L, int objindex))GetProcAddress(mod, "lua_setmetatable");
	lua_newuserdata_weak = (void *(*) (lua_State *L, size_t sz))GetProcAddress(mod, "lua_newuserdata");
	lua_pushlstring_weak = (void (*) (lua_State *L, const char *s, size_t len))GetProcAddress(mod, "lua_pushlstring");
}
#else
void load_weak_lua_symbols() {}
#endif

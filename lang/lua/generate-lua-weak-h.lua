-- We want to use Lua functions without depending on them, so that we don't create a runtime
-- dependnecy on a lua library. This way, we can include Lua support in the binary even if
-- it isn't used in the destination app. Also, lua is often embedded in weird ways, so all we
-- have to care about here is that lua symbols is available _somehow_.

-- usage: Run from the same directory as lua-utils.h. It will replace the contents of lua-weak.{h|c}

local h = ""
function header(l)
	h = h..l.."\n"
end
header("// Do not modify. Generated by generate-lua-weak-h.lua")
header("#include \"lua.h\"")
header("#include \"lauxlib.h\"")
header("#include \"lualib.h\"")

local s = "#include \"lua-weak.h\"\n"
function source(l)
	s = s..l.."\n"
end

-- if you want to use more lua functions, add them here.
local funcs = {
	luaL_checkinteger = "lua_Integer luaL_checkinteger (lua_State *L, int narg)",
	lua_settop = "void  lua_settop (lua_State *L, int idx)",
	luaL_argerror = "int luaL_argerror (lua_State *L, int numarg, const char *extramsg)",
	lua_pcall = "int   lua_pcall (lua_State *L, int nargs, int nresults, int errfunc)",
	luaL_ref = "int luaL_ref (lua_State *L, int t)",
	lua_createtable = "void  lua_createtable (lua_State *L, int narr, int nrec)",
	luaL_checknumber = "lua_Number luaL_checknumber (lua_State *L, int numArg)",
	lua_rawgeti = "void  lua_rawgeti (lua_State *L, int idx, int n)",
	luaL_register = "void luaL_register (lua_State *L, const char *libname, const luaL_Reg *l)",
	lua_pushinteger = "void  lua_pushinteger (lua_State *L, lua_Integer n)",
	lua_isnumber = "int             lua_isnumber (lua_State *L, int idx)",
	luaL_checklstring = "const char *luaL_checklstring (lua_State *L, int narg, size_t *l)",
 	lua_pushvalue = "void  lua_pushvalue (lua_State *L, int idx)",
 	lua_gettable = "void  lua_gettable (lua_State *L, int idx)",
 	lua_newuserdata = "void *lua_newuserdata (lua_State *L, size_t sz)",
 	lua_setmetatable = "int   lua_setmetatable (lua_State *L, int objindex)",
 	lua_pushnumber = "void  lua_pushnumber (lua_State *L, lua_Number n)",
 	lua_settable = "void  lua_settable (lua_State *L, int idx)",
 	lua_pushboolean = "void  lua_pushboolean (lua_State *L, int b)",
 	luaL_checkudata = "void *luaL_checkudata (lua_State *L, int ud, const char *tname)",
 	luaL_error = "int luaL_error (lua_State *L, const char *fmt, ...)",
 	lua_pushnil = "void  lua_pushnil (lua_State *L)",
 	lua_type = "int             lua_type (lua_State *L, int idx)",
 	lua_typename = "const char     *lua_typename (lua_State *L, int tp)",
 	luaL_newmetatable = "int   luaL_newmetatable (lua_State *L, const char *tname)",
 	luaL_unref = "void luaL_unref (lua_State *L, int t, int ref)",
 	lua_pushstring = "void  lua_pushstring (lua_State *L, const char *s)",
 	lua_getfield = "void  lua_getfield (lua_State *L, int idx, const char *k)",
 	lua_isstring = "int             lua_isstring (lua_State *L, int idx)"
}

-- MacOS: Uses -Wl,-undefined,dynamic_lookup, so we can just use regular old lua.h
header("#ifdef __APPLE__")
-- do nothing
header("#endif")

-- Linux: I can't get undefined dynamic lookup to work, so we need explict weak prototypes
header("#if __clang__")
header("#define WEAK_ATTRIBUTE __attribute__((weak))")
for name, signature in pairs(funcs) do
	header(signature.." WEAK_ATTRIBUTE;")
end
header("#endif")

-- Windows: Doesn't have weak linking, so we need to frickin manually GetProcAddress for each
-- lua function unless we want to depend on lua.dll (which is its own nightmare)
header("#ifdef WIN32")
source("#ifdef WIN32")
for name, signature in pairs(funcs) do
	
	header("extern "..signature:gsub(name, "(*"..name.."_weak)")..";")
	header("#define "..name.." "..name.."_weak")
	source(signature:gsub(name, "(*"..name.."_weak)").." = NULL;")
end

source([[
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
	]])
for name, signature in pairs(funcs) do
	source("	" .. name .. "_weak = (" .. signature:gsub(name, "(*)") .. ")GetProcAddress(mod, \"" .. name .. "\");")
end
source("}")

header("#endif")
source("#else")
source("void load_weak_lua_symbols() {}") -- always available	
source("#endif")
header("extern void load_weak_lua_symbols();") -- always available

f = io.open("lua-weak.h", "w+")
f:write(h)
f:close()
f = io.open("lua-weak.c", "w+")
f:write(s)
f:close()
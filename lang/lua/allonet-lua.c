#include <stdio.h>
#include <string.h>
#include <math.h>
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

// Todo: figure out how to do this in Windows without requiring lua symbols in the running host
#ifndef _WIN32

#define WEAK_ATTR __attribute__((weak))

// Weak link Lua so that we can expose a Lua API without requiring to link lua
// in all h ost processes.
extern lua_Number lua_tonumber (lua_State *L, int index) WEAK_ATTR;
extern void lua_pushnumber (lua_State *L, double n) WEAK_ATTR;
void luaL_register (lua_State *L,
                    const char *libname,
                    const luaL_Reg *l) WEAK_ATTR;

int l_sin (lua_State *L) {
    double d = lua_tonumber(L, 1);  /* get argument */
    lua_pushnumber(L, sin(d));  /* push result */
    return 1;  /* number of results */
}

static const struct luaL_reg allonet [] = {
      {"sin", l_sin},
      {NULL, NULL}  /* sentinel */
    };

int luaopen_allonet (lua_State *L) {
    luaL_register (L, "allonet", allonet);
    return 1;
}

#endif
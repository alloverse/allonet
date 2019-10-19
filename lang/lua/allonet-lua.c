#include <stdio.h>
#include <string.h>
#include <math.h>
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

// Weak link Lua so that we can expose a Lua API without requiring to link lua
// in all h ost processes.
extern lua_Number lua_tonumber (lua_State *L, int index) __attribute__((weak));
extern void lua_pushnumber (lua_State *L, double n) __attribute__((weak));
void luaL_register (lua_State *L,
                    const char *libname,
                    const luaL_Reg *l) __attribute__((weak));

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

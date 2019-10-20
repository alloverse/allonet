#include <stdio.h>
#include <string.h>
#include <math.h>
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

// Todo: figure out how reference lua symbols weakly in Windows (see LUA_LINKER_FLAGS for mac/linux)
// so that this library can contain a Lua API without requiring linking with Lua (e g when used from
// C or Python or whatev)
#ifndef _WIN32

int l_sin (lua_State *L) {
    double d = lua_tonumber(L, 1);  /* get argument */
    lua_pushnumber(L, sin(d));  /* push result */
    return 1;  /* number of results */
}

static const struct luaL_reg allonet [] = {
      {"sin", l_sin},
      {NULL, NULL}  /* sentinel */
    };

int luaopen_liballonet (lua_State *L) {
    luaL_register (L, "allonet", allonet);
    return 1;
}

#endif
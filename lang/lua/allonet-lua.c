#include <stdio.h>
#include <string.h>
#include <math.h>
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

static int l_sin (lua_State *L) {
    double d = lua_tonumber(L, 1);  /* get argument */
    lua_pushnumber(L, sin(d));  /* push result */
    return 1;  /* number of results */
}

static const struct luaL_reg allonet [] = {
      {"sin", l_sin},
      {NULL, NULL}  /* sentinel */
    };

int luaopen_allonet (lua_State *L) {
    luaL_openlib(L, "allonet", allonet, 0);
    return 1;
}

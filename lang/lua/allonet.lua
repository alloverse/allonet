local path = "../../build/liballonet.so"
local f = assert(loadlib(path, "luaopen_allonet"))
f()=
# Allonet Lua bridge

You can use Allonet from your Lua 5.1 script or application, just as you
would from C, with the same API. Here's how:

1. Copy liballonet.so or liballonet.dll to next to your lua script
2. `require "liballonet"` (no extension needed)
3. Use the functions from `allonet`, like `allonet.connect(...)`.

See `sample.lua` in this directory for an example. Here's a snippet to
get you started:

    require("liballonet")
    local json = require("json")
    local client = allonet.connect(
        "alloplace://nevyn.places.alloverse.com",
        json.encode({display_name = "lua-sample"}),
        json.encode({})
    )
    while true do
        client:poll()
        sleep(1.0/20.0) -- poll at 20hz
    end

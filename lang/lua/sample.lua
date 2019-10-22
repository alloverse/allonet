require("liballonet")
local json = require("json")

-- Connect is synchronous, and returns a client
-- if connected or raises an error on failure to connect.
local client = allonet.connect(
    "alloplace://nevyn.places.alloverse.com",
    json.encode({display_name = "lua-sample"}),
    json.encode({
        geometry = {
            type = "hardcoded-model"
        }
    })
)
local me = ""

-- This callback is important to set to notice when there's
-- no active connection. Do reconnection logic, or maybe quit your app.
client:set_disconnected_callback(function()
    print("Lost connection :(")
    exit()
end)

-- Some other entity is trying to interact with you.
client:set_interaction_callback(function(inter)
    print("Got interaction: " .. json.encode(inter))
    local body = json.decode(inter.body)
    if body[1] == "announce" then
        me = body[2]
        print("Hey look, it's me! " .. me)
    end
end)

-- The world has changed somehow.
client:set_state_callback(function(state)
    print("State: " .. json.encode(state))
end)

-- In an ideal app, poll() at 20hz, not as fast as you can :)
-- But you have to poll regularly, or nothing works.
while true do
    client:poll()
end

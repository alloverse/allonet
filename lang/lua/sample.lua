require("liballonet")
local json = require("json")
function dump(o, l, ll)
    l = l or 0
    ll = ll or l
    if type(o) == 'table' then
       local s = string.rep("\t",ll) .. '{ \n'
       for k,v in pairs(o) do
          if type(k) ~= 'number' then k = '"'..k..'"' end
          s = s .. string.rep("\t",l+1) .. ''..k..' = ' .. dump(v, l+1, 0) .. '\n'
       end
       return s .. string.rep("\t",l) .. '} \n'
    else
       return string.rep("\t",ll) .. tostring(o)
    end
 end


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

client:set_disconnected_callback(function()
    print("Lost connection :(")
    exit()
end)

client:set_interaction_callback(function(inter)
    print("Got interaction: " .. dump(inter))
    local body = json.decode(inter.body)
    if body[1] == "announce" then
        me = body[2]
        print("Hey look, it's me! " .. me)
    end
end)

client:set_state_callback(function(state)
    print("State: " .. dump(state))
end)

while true do
    client:poll()
    print("State: " .. dump(client:get_state()))
end

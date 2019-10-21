require("liballonet")
local json = require("json")
function dump(o)
    if type(o) == 'table' then
       local s = '{ '
       for k,v in pairs(o) do
          if type(k) ~= 'number' then k = '"'..k..'"' end
          s = s .. '['..k..'] = ' .. dump(v) .. ','
       end
       return s .. '} '
    else
       return tostring(o)
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

while true do
    client:poll()
end

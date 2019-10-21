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

while true do
    client:poll()
    local inter = client:pop_interaction()
    if inter ~= nil then
        print(dump(inter))
    end
end

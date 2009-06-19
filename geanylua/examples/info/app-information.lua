--[[
  Shows some user prefs and application information 
--]]

local info={}



function read_table(t, m)
  if not t then return end
  for k,v in pairs(t) do
    if type(v)=="table" then
       read_table(v, k.."."); -- recurse into sub-table
    else
       table.insert(info,m..k..": "..tostring(v))
    end
  end
end

read_table(geany.appinfo(), "")

table.sort(info)

geany.message("App info:", table.concat(info, "\n"))


--[[
  Shows some information about the current document
--]]

local info={}

for k,v in pairs(geany.fileinfo() or {})
do
  table.insert(info, "  "..k..": "..tostring(v))
end

table.sort(info)

geany.message( "File info:", ( (#info>0) and table.concat(info,"\n") ) or "No file!" )

--[[
  Open a file from the Lua scripts folder.
--]]

local folder=geany.appinfo().scriptdir

local filter="Lua files|*.lua|All files|*"
local filename=geany.pickfile("open", folder, filter)

if (filename) then
  geany.open(filename)
end

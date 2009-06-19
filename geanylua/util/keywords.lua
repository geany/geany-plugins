#!/usr/bin/env lua


--[[
  Create a compatible list of keywords for use in Geany's
   "filetypes.lua" syntax-highlighter file.
--]]

require("libgeanylua")



io.write(
  "\n"..
  "# Replace these lines in your ~/.geany/filedefs/filetypes.lua\ file\n"..
  "# to enable orange syntax highlighting for the geanylua keywords...\n"..
  "\n"..
  "\n"..
  "## Put this in the [styling] section:\n"..
  "word5=0xf0a000;0xffffff;false;false\n"..
  "\n"..
  "## Put this in the [keywords] section:\n"
)





function write_keywords(module_name, module_table)
  local names={}
  for name,value in pairs(module_table)
  do
    table.insert(names,name)
  end
  table.sort(names)
  for i,name in ipairs(names)
  do
    io.write(module_name.."."..name.." ")
  end
end

io.write("user1=")

write_keywords("geany",geany)
write_keywords("dialog",dialog)
write_keywords("keyfile",keyfile)
print()

--[[
  Copies the test script to the name and location of your choice
--]]

local fn, fh, txt, ok, err

fn=geany.dirname(geany.script)..geany.dirsep.."02.run-test-script.lua"

fh, err = io.open(fn, "r")

if not fh then
  geany.message("Failed to open source file!", fn.."\n"..(err or "unknown error"))
  return
end

txt, err = fh:read("*a")

if not txt then
  geany.message("Failed to read source file!", fn.."\n"..(err or "unknown error"))
  return
end

fh:close()



fn=geany.pickfile("save", geany.appinfo().scriptdir,
  "Lua files|*.lua|All files|*.*")


if not fn then
  return
end

fh, err = io.open(fn, "w")
if not fh then
  geany.message("Failed to create new file!", fn.."\n"..(err or "unknown error"))
  return
end


ok, err=fh:write(txt)
if not ok then
  geany.message("Failed writing new file!", fn.."\n"..(err or "unknown error"))
  fh:close()
  return
end


ok, err=fh:close()
if not ok then
  geany.message("Failed saving new file!", fn.."\n"..(err or "unknown error"))
  return
end

geany.rescan()


--[[
  Trim all trailing whitespace from the current document
--]]


local s=geany.text()

if (s and string.match(s, "[ \t]\n") )
then
  geany.text(string.gsub(s,"[ \t]+\n", "\n"))
else
  geany.message("Right trim:", "Match not found.")
end

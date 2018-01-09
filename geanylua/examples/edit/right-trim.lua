--[[
  Trim all trailing whitespace from the current document
--]]


local s=geany.text()

if (s and string.match(s, "[ \t][\r\n]") )
then
  geany.text(string.gsub(s,"([ \t]+)([\r\n]+)", "%2"))
else
  geany.message("Right trim:", "Match not found.")
end

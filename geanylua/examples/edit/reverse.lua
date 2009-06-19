--[[
  Silly example script that reverses the text of
  the current document or selection.
--]]


local s=geany.selection()

if (s)
then
  if ( s ~= "" )
  then
    geany.selection(string.reverse(s))
  else
    geany.text(string.reverse(geany.text()))
  end
end

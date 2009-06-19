--[[
  Select the innermost "block" of text from the current caret position.
  Block boundaries are a pair of: () [] {} <>
--]]


local pos=geany.caret()
local start=pos
local match

while (pos>=0)
do
  match=geany.match(pos)
  if (match>=start) then break end
  pos=pos-1
end

if (match >= 0 ) 
then
  if (match>pos)
  then
    geany.select(pos,match+1)
  else
    geany.select(pos+1,match)
  end
else
  geany.message("Can't find matching brace!")
end





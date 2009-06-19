--[[
  Convert the document text or selection to "Proper Case"
  e.g. "john doe" becomes "John Doe"
--]]


function proper_case(s)
  local s=string.gsub(string.lower(s),"^(%w)", string.upper)
  return string.gsub(s,"([^%w]%w)", string.upper)
end



local s=geany.selection()

if (s)
then
  if ( s ~= "" )
  then
    geany.selection(proper_case(s))
  else
    geany.text(proper_case(geany.text()))
  end
end


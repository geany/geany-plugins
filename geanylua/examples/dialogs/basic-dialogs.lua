--[[
  Demo script to test the various dialog functions
--]]

geany.banner="Dialog Test"
while true do
  local s
  s=geany.input("Please enter your name:", "anonymous") 
  if (s) 
    then geany.message( "Hello, " .. s .. "!" ) 
    else geany.message("Cancelled!")
  end
  s=geany.choose("Pick a color:", {"red","orange","yellow","green","blue","purple"})
  if (s) 
    then geany.message( "You picked \"" .. s .. "\"") 
    else geany.message("Nothing selected")
  end
  if not geany.confirm("Loop completed...", "Repeat the demo?", true)
  then 
    geany.message("*** Dialogs Demo ***", "Finished!")
    break
  end
end

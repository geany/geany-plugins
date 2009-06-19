--[[
  Show the path and filename of the current document in a message box
  The filename shown in the box can be selected and copied, so this script
  might actually come in handy when you need access to the filename.
--]]


geany.message("Current filename:", geany.filename() or "No File!")


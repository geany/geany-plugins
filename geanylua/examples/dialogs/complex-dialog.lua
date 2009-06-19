--[[
  Sample script using the dialog module
--]]

-- Create the new dialog box
local dlg=dialog.new("Login", { "_Yes", "_No", "Cance_l" } )


dlg:label("\nThis is just a demo - it really doesn't do anything...\n")

-- Show some basic login stuff
dlg:heading("Credentials:")
dlg:text("name", "anonymous","Username:")
dlg:password("pass", nil,  "Password:")
dlg:checkbox("kept", false,"Remember me")



-- Create a radio group "auth" with default value "none"
dlg:group("auth", "none", "Authentication:")

-- Add some buttons to "auth" group...
dlg:radio("auth", "basic", "BASIC")
dlg:radio("auth", "ssl",   "SSL")
dlg:radio("auth", "none",  "NONE")



-- Create a drop-down list "proto" with default "http"
dlg:select("proto", "http",  "Protocol:")

-- Add some items to "proto" list...
dlg:option("proto", "dict",  "DICT")
dlg:option("proto", "file",  "FILE")
dlg:option("proto", "ftp",   "FTP")
dlg:option("proto", "http",  "HTTP")
dlg:option("proto", "scp",   "SSH")
dlg:option("proto", "tftp",  "TFTP")


-- Show off the other widgets
dlg:textarea("remarks", nil, "Comments: ")
dlg:color("color", nil, "Favorite color:");
dlg:font("font", nil, "Preferred font:");
dlg:file("filename", nil, "Upload file:")


-- Show the dialog
local button, results = dlg:run()


-- Display the results
if ( button == 1 ) and results then
  local msg=""
   -- Combine the results table back into a single string
  for key,value in pairs(results)
  do
    msg=msg.."\n"..key..":\t"..value
  end
  -- Show the results
  local msgbox=dialog.new("Results", {"OK"})
  msgbox:label("     ---  Results table  ---     ")
  msgbox:label(msg.."\n")
  msgbox:run()
else
   local errbox=dialog.new("Cancelled", {"OK"})
   errbox:label("     Cancelled with button #"..button.."     ")
   errbox:run()
end

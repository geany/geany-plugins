--[[
  Tries to open the GeanyLua help file index in the user's web browser.
--]]


local DirSep = geany.dirsep
local HelpFile = DirSep.."geanylua-index.html"
local AppInfo = geany.appinfo()

local OwnHelpDir=AppInfo.scriptdir..DirSep.."support"

-- FIXME: expose GeanyLua's $DOCDIR to Lua scripts.
local DocDir=geany.dirname(AppInfo.docdir:gsub("html", ""):gsub("geany", "geany-plugins"))
local SysHelpDir=DocDir..DirSep.."geanylua"

local Browsers={ AppInfo.tools.browser,
                 "firefox", "mozilla", "opera",
                 "konqueror", "netscape", "iexplore"
               }

local FullName=nil


-- Search in user and sytem directory for help file
for i,Dir in pairs( { OwnHelpDir, SysHelpDir } )
do
  FullName=Dir..HelpFile
	local st=geany.stat(FullName)
	if (st) and (st.type == "r") --it exists and  it's a regular file
	then
    break
  else
    FullName=nil
	end
end



if FullName then
  if (DirSep=="\\") then
    -- windows hack
    OldDir=geany.wkdir()
    geany.wkdir(geany.dirname(FullName))
    local rv,msg=geany.launch(os.getenv("COMSPEC"),"/C","start",geany.basename(FullName))
    geany.wkdir(OldDir)
    if not rv then geany.message(msg) end
  else
    local url="file://"..FullName
    for i,Browser in pairs(Browsers) -- loop through possible browsers
    do
      rv,msg=geany.launch(Browser, url)
      if rv then break end
      geany.message(msg)
    end
  end
else
  geany.message("Error launching help",
     "Can't find help file in:\n "..OwnHelpDir.."\nor:\n"..SysHelpDir)
end


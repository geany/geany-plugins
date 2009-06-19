--[[
  Create a list of scripts from the examples directory
  and let the user select a script to open.
--]]

local filelist={}

local dirsep = geany.dirsep

local examples=geany.appinfo().scriptdir..dirsep.."examples"

function listfiles(dir)
  local stat=geany.stat(dir)
  if not (stat and stat.type=="d")
  then
   geany.message("Can't open folder:\n"..dir)
   return
  end
  for file in geany.dirlist(dir)
  do
    local path=dir..dirsep..file
    stat=geany.stat(path)
    if stat.type=="d"
    then
      listfiles(path) -- Recursive !
    else
     if stat.type=="r"
     then
      table.insert(filelist, path:sub(#examples+2))
     end
    end
  end
end


listfiles(examples)

if #filelist>0
then
  table.sort(filelist)
	geany.banner="Examples"
	local file=geany.choose("Choose a script to open:", filelist)

	if file then
		geany.open(examples..dirsep..file)
	end
end

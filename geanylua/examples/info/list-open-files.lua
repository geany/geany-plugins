--[[ 
  Creates a new, untitled document containing a list
  of all currently opened (and named) files.
--]]

local files={}

for file in geany.documents()
do
  table.insert(files,file)
end

if ( #files > 0 )
then
  if geany.confirm("Found "..#files.." files.", "Open list in new tab?", false )
  then
    local names = ""
    for i,file in pairs(files)
    do
      names=names..file.."\n"
    end
    geany.newfile("Files")
    geany.text(names)
  else
    file=geany.choose("Choose a tab to activate:", files)
    if (file)
    then
      geany.activate(file)
    end
  end
else
	geany.message("No files!")
end


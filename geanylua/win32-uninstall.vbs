set ws = WScript.CreateObject("WScript.Shell")
set fso = CreateObject("Scripting.FileSystemObject")


sub DeleteIfEmpty(FolderName)
  if not fso.FolderExists(FolderName) then exit sub
  set folder=fso.GetFolder(FolderName)
  set subdirs = folder.SubFolders
  set files = folder.Files
  if ( files.Count + subdirs.Count ) = 0 then fso.DeleteFolder(FolderName)
end sub


sub DoUninstall()
  plugins  = ws.SpecialFolders("AppData") & "\Geany\plugins"
  geanylua = plugins & "\geanylua"
  dllname  = geanylua & ".dll"
  libname  = geanylua & "\libgeanylua.dll"
  examples = geanylua & "\examples"
  support  = geanylua & "\support"
  docs=support & "\*.html"
  on error resume next
  if not fso.FolderExists(plugins) then
    exit sub
  end if
  if fso.FileExists(dllname) then
    fso.DeleteFile dllname,true
  end if
  if fso.FileExists(libname) then
    fso.DeleteFile libname,true
  end if
  if fso.FolderExists(examples) then
    fso.DeleteFolder examples,true
  end if
  if fso.FolderExists(support) then
    fso.DeleteFile docs,true
    DeleteIfEmpty(support)
  end if
  DeleteIfEmpty(geanylua)

  Msg="Uninstall Complete."

  if fso.FileExists(dllname) or fso.FolderExists(geanylua) then _
    Msg = Msg & vbCrLf & "( Some elements were not removed. )"

  MsgBox Msg,0,"GeanyLua Uninstall" 

end sub


RetVal = MsgBox(" -- This will uninstall GeanyLua for the current user --" & _
   vbCrLf & vbCrLf & _
   "Be sure to close all running instances of Geany before continuing!" & _
   vbCrLf & vbCrLf & _
   "Do you wish to continue?", 4, "GeanyLua Uninstall")

if RetVal = 6 then DoUninstall() else _
 MsgBox "Uninstall Cancelled.", 0, "GeanyLua Uninstall" 


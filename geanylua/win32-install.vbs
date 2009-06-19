' Script to install geanylua on Windows - tested on Win98 and Win2k

set ws = WScript.CreateObject("WScript.Shell")
set fso = CreateObject("Scripting.FileSystemObject")


sub FailMsg(Msg, FileName)
  MsgBox Msg & vbCrLf & FileName & vbCrLf & vbCrLf & "Installation failed.", _
    0 , "Installation error"
end sub



function MakeFolder(FolderName)
   RetVal=false
   if fso.FolderExists(FolderName) then
     RetVal=true
   else
     on error resume next
     RetVal= fso.CreateFolder(FolderName) <> vbNull
   end if
   if not RetVal then FailMsg  "Failed to create folder:" , FolderName 
   MakeFolder=RetVal
end function




function InstallFolder(srcname,trgname)
  if not fso.FolderExists(srcname) then
    FailMsg "Can't find source folder:'" , srcname
    InstallFolder=false
    exit function
  end if
  set src = fso.GetFolder(srcname)
  on error resume next
  src.Copy(trgname)
  RetVal=fso.FolderExists(trgname)
  if not RetVal then FailMsg "Failed to create folder:" , trgname
  InstallFolder=RetVal
end function




function InstallFile(srcname,trgname)
  InstallFile=false
  if not fso.FileExists(srcname) then
    FailMsg "Can't find install file:" , srcname
    exit function
  end if
  if fso.FileExists(trgname) then
    on error resume next
    fso.DeleteFile(trgname)
  end if
  if fso.FileExists(trgname) then
    FailMsg "Failed to overwite existing file:" , trgname
    exit function
  end if
  set src = fso.GetFile(srcname)
  on error resume next
  src.Copy(trgname)
  if not fso.FileExists(trgname) then
    FailMsg "Failed to create file:" , trgname
    exit function
  end if
  InstallFile=true
end function




sub DoInstall ()

  dest = ws.SpecialFolders("AppData")
  if not MakeFolder(dest) then exit sub

  dest=dest & "\Geany"
  if not MakeFolder(dest) then exit sub

  dest=dest & "\plugins"
  if not MakeFolder(dest) then exit sub

  dllname="geanylua.dll"
  if fso.FileExists(dllname) then
    srcname = dllname
  elseif fso.FileExists(".libs\" & dllname) then
    srcname = ".libs\" & dllname
  else
    FailMsg "Can't find install file:" , dllname 
    exit sub
  end if
  if not InstallFile(srcname, dest & "\" & dllname) then exit sub       

  dest=dest & "\geanylua"
  if not MakeFolder(dest) then exit sub

  libname="libgeanylua.dll"
  if fso.FileExists(libname) then
    srcname = libname
  elseif fso.FileExists(".libs\" & libname) then
    srcname = ".libs\" & libname
  else
    FailMsg "Can't find install file:" , libname 
    exit sub
  end if
  if not InstallFile(srcname, dest & "\" & libname) then exit sub       


  if not InstallFolder("examples", dest & "\examples") then exit sub

  if not InstallFolder("docs", dest & "\support") then exit sub

  MsgBox "Installation Complete!", 0, "GeanyLua Installation"

end sub




RetVal = MsgBox(" -- This will install GeanyLua for the current user --" & _
   vbCrLf & vbCrLf & _
   "Be sure to close all running instances of Geany before continuing!" & _
   vbCrLf & vbCrLf & _
   "Do you wish to continue?", 4, "GeanyLua Installation")

if RetVal = 6 then DoInstall() else _
 MsgBox "Installation Cancelled.", 0, "GeanyLua Installation" 


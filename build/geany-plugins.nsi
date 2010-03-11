;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; geany-plugins.nsi - this file is part of the geany-plugins project
;
; Copyright 2009-2010 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
; Copyright 2009-2010 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
;
; This program is free software; you can redistribute it and/or modify
; it under the terms of the GNU General Public License as published by
; the Free Software Foundation; either version 2 of the License, or
; (at your option) any later version.
;
; This program is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public License
; along with this program; if not, write to the Free Software
; Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
;
; Installer script for geany-plugins (Windows Installer), based on Geany's installer script
;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;


; Do a Cyclic Redundancy Check to make sure the installer was not corrupted by the download
CRCCheck force
RequestExecutionLevel user ; set execution level for Windows Vista

;;;;;;;;;;;;;;;;;;;
; helper defines  ;
;;;;;;;;;;;;;;;;;;;
!define PRODUCT_NAME "Geany-Plugins"
!define PRODUCT_VERSION "0.19"
!define PRODUCT_VERSION_ID "0.19.0.0"
!define PRODUCT_PUBLISHER "The Geany developer team"
!define PRODUCT_WEB_SITE "http://www.geany.org/"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
!define PRODUCT_DIR_REGKEY "Software\Geany-Plugins"
!define GEANY_DIR_REGKEY "Software\Geany"
!define REQUIRED_GEANY_VERSION "0.19"
!define RESOURCEDIR "geany-plugins-${PRODUCT_VERSION}"

;;;;;;;;;;;;;;;;;;;;;
; Version resource  ;
;;;;;;;;;;;;;;;;;;;;;
VIProductVersion "${PRODUCT_VERSION_ID}"
VIAddVersionKey "ProductName" "${PRODUCT_NAME}"
VIAddVersionKey "FileVersion" "${PRODUCT_VERSION}"
VIAddVersionKey "ProductVersion" "${PRODUCT_VERSION}"
VIAddVersionKey "LegalCopyright" "Copyright 2009-2010 by the Geany developer team"
VIAddVersionKey "FileDescription" "${PRODUCT_NAME} Installer"

BrandingText "$(^NAME) installer (NSIS 2.45)"
Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"
SetCompressor /SOLID lzma
ShowInstDetails hide
ShowUnInstDetails hide
XPStyle on
OutFile "geany-plugins-${PRODUCT_VERSION}_setup.exe"

Var Answer
Var UserName
Var UNINSTDIR

;;;;;;;;;;;;;;;;
; MUI Settings ;
;;;;;;;;;;;;;;;;
!include "MUI2.nsh"

!define MUI_ABORTWARNING
; FIXME hard-coded path...should we add geany.ico to the geany-plugins repo?
!define MUI_ICON "\geany_svn\icons\geany.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall-full.ico"

; Welcome page
!insertmacro MUI_PAGE_WELCOME
; License page
; FIXME
!insertmacro MUI_PAGE_LICENSE "${RESOURCEDIR}\doc\plugins\addons\Copying.txt"
; Components page
!insertmacro MUI_PAGE_COMPONENTS
; Directory page
!define MUI_PAGE_CUSTOMFUNCTION_LEAVE OnDirLeave
!insertmacro MUI_PAGE_DIRECTORY
; Instfiles page
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_INSTFILES ; Uninstaller page
!insertmacro MUI_LANGUAGE "English" ; Language file

;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Sections and InstTypes  ;
;;;;;;;;;;;;;;;;;;;;;;;;;;;
InstType "Full"

Section "!Program Files" SEC01
	SectionIn RO 1 2
	SetOverwrite ifnewer

	SetOutPath "$INSTDIR\"
	File "${RESOURCEDIR}\ReadMe.Windows.txt"

	SetOutPath "$INSTDIR\lib"
	File /r "${RESOURCEDIR}\lib\*"

	SetOutPath "$INSTDIR\share\geany-plugins"
	File /r "${RESOURCEDIR}\share\geany-plugins\*"
SectionEnd

Section "Language Files" SEC02
	SectionIn 1
	SetOutPath "$INSTDIR\share\locale"
	File /r "${RESOURCEDIR}\share\locale\*"
SectionEnd

Section "Documentation" SEC03
	SectionIn 1
	SetOverwrite ifnewer
	SetOutPath "$INSTDIR"
	File /r "${RESOURCEDIR}\doc"
SectionEnd

Section "Dependencies" SEC04
	SectionIn 1
	SetOverwrite ifnewer
	SetOutPath "$INSTDIR"
	File /r "contrib\"
SectionEnd

Section -Post
	WriteUninstaller "$INSTDIR\uninst-plugins.exe"
	WriteRegStr SHCTX "${PRODUCT_DIR_REGKEY}" Path "$INSTDIR"
	${if} $Answer == "yes" ; if user is admin
		WriteRegStr SHCTX "${PRODUCT_UNINST_KEY}" "DisplayName" "$(^Name)"
		WriteRegStr SHCTX "${PRODUCT_UNINST_KEY}" "UninstallString" "$INSTDIR\uninst-plugins.exe"
		WriteRegStr SHCTX "${PRODUCT_UNINST_KEY}" "DisplayIcon" "$INSTDIR\bin\Geany.exe"
		WriteRegStr SHCTX "${PRODUCT_UNINST_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
		WriteRegStr SHCTX "${PRODUCT_UNINST_KEY}" "URLInfoAbout" "${PRODUCT_WEB_SITE}"
		WriteRegStr SHCTX "${PRODUCT_UNINST_KEY}" "URLUpdateInfo" "${PRODUCT_WEB_SITE}"
		WriteRegStr SHCTX "${PRODUCT_UNINST_KEY}" "Publisher" "${PRODUCT_PUBLISHER}"
		WriteRegDWORD SHCTX "${PRODUCT_UNINST_KEY}" "NoModify" 0x00000001
		WriteRegDWORD SHCTX "${PRODUCT_UNINST_KEY}" "NoRepair" 0x00000001
	${endif}
SectionEnd

Section Uninstall
	Delete "$INSTDIR\ReadMe.Windows.txt"
	Delete "$INSTDIR\uninst-plugins.exe"
	Delete "$INSTDIR\lib\addons.dll"
	Delete "$INSTDIR\lib\geanyinsertnum.dll"
	Delete "$INSTDIR\lib\geanylatex.dll"
	Delete "$INSTDIR\lib\geanylipsum.dll"
	Delete "$INSTDIR\lib\geanylua.dll"
	Delete "$INSTDIR\lib\geanysendmail.dll"
	Delete "$INSTDIR\lib\geanyvc.dll"
	Delete "$INSTDIR\lib\shiftcolumn.dll"
	Delete "$INSTDIR\lib\spellcheck.dll"

	Delete "$INSTDIR\bin\libenchant.dll"
	Delete "$INSTDIR\bin\lua5.1.dll"

	RMDir /r "$INSTDIR\doc\plugins"
	RMDir /r "$INSTDIR\lib\geany-plugins"
	RMDir /r "$INSTDIR\share\geany-plugins"
	RMDir /r "$INSTDIR\lib\enchant"
	RMDir /r "$INSTDIR\share\enchant"

	FindFirst $0 $1 "$INSTDIR\share\locale\*"
	loop:
		StrCmp $1 "" done
		Delete "$INSTDIR\share\locale\$1\LC_MESSAGES\geany-plugins.mo"
		RMDir "$INSTDIR\share\locale\$1\LC_MESSAGES"
		RMDir "$INSTDIR\share\locale\$1"
		FindNext $0 $1
		Goto loop
	done:
	FindClose $0

	; only if empty
	RMDir "$INSTDIR\doc"
	RMDir "$INSTDIR\lib"
	RMDir "$INSTDIR\share\locale"
	RMDir "$INSTDIR\share"
	RMDir "$INSTDIR"

	DeleteRegKey SHCTX "${PRODUCT_DIR_REGKEY}"
	DeleteRegKey HKCU "${PRODUCT_DIR_REGKEY}"
	DeleteRegKey SHCTX "${PRODUCT_UNINST_KEY}"
	DeleteRegKey HKCU "${PRODUCT_UNINST_KEY}"

	SetAutoClose true
SectionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;
; Section descriptions  ;
;;;;;;;;;;;;;;;;;;;;;;;;;
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
!insertmacro MUI_DESCRIPTION_TEXT ${SEC01} "Required plugin files. You cannot skip these files."
!insertmacro MUI_DESCRIPTION_TEXT ${SEC02} "Various translations for the included plugins."
!insertmacro MUI_DESCRIPTION_TEXT ${SEC03} "Various documentation files for the included plugins."
!insertmacro MUI_DESCRIPTION_TEXT ${SEC04} "Dependency files for various plugins (currently libenchant for Spell Check, Lua for GeanyLua and libxml2 for PrettyPrinter)."
!insertmacro MUI_FUNCTION_DESCRIPTION_END

;;;;;;;;;;;;;;;;;;;;;
; helper functions  ;
;;;;;;;;;;;;;;;;;;;;;

; (from http://jabref.svn.sourceforge.net/viewvc/jabref/trunk/jabref/src/windows/nsis/setup.nsi)
!macro IsUserAdmin Result UName
	ClearErrors
	UserInfo::GetName
	IfErrors Win9x
	Pop $0
	StrCpy ${UName} $0
	UserInfo::GetAccountType
	Pop $1
	${if} $1 == "Admin"
		StrCpy ${Result} "yes"
	${else}
		StrCpy ${Result} "no"
	${endif}
	Goto done

Win9x:
	StrCpy ${Result} "yes"
done:
!macroend

Function CheckForGeany
	; find and read Geany's installation directory and use it as our installation directory
	ReadRegStr $INSTDIR SHCTX "${GEANY_DIR_REGKEY}" "Path"
	StrCmp $INSTDIR "" 0 +3
	MessageBox MB_OK|MB_ICONSTOP "Geany could not be found. Please install Geany first." /SD IDOK
	Abort

	; check Geany's version
	GetDLLVersion "$INSTDIR\bin\geany.exe" $R0 $R1
	IntOp $R2 $R0 >> 16
	IntOp $R2 $R2 & 0x0000FFFF ; $R2 now contains major version
	IntOp $R3 $R0 & 0x0000FFFF ; $R3 now contains minor version
	StrCpy $0 "$R2.$R3"
	StrCmp $0 ${REQUIRED_GEANY_VERSION} version_check_done 0
	MessageBox MB_YESNO|MB_ICONEXCLAMATION \
		"You have Geany $0 installed but you need Geany ${REQUIRED_GEANY_VERSION}.$\nDo you really want to continue?" \
		/SD IDNO IDNO stop IDYES ignore
stop:
	Abort
ignore:
	MessageBox MB_OK|MB_ICONEXCLAMATION \
		"Using another version than Geany ${REQUIRED_GEANY_VERSION} may cause unloadable plugins or crashes." \
		/SD IDOK

version_check_done:
FunctionEnd

Function .onInit
	; (from http://jabref.svn.sourceforge.net/viewvc/jabref/trunk/jabref/src/windows/nsis/setup.nsi)
	; If the user does *not* have administrator privileges, abort
	StrCpy $Answer ""
	StrCpy $UserName ""
	!insertmacro IsUserAdmin $Answer $UserName ; macro from LyXUtils.nsh
	${if} $Answer == "yes"
		SetShellVarContext all ; set that e.g. shortcuts will be created for all users
	${else}
		SetShellVarContext current
		; TODO is this really what we want? $PROGRAMFILES is not much better because
		; probably the unprivileged user can't write it anyways
		StrCpy $INSTDIR "$PROFILE\$(^Name)"
	${endif}

	; prevent running multiple instances of the installer
	System::Call 'kernel32::CreateMutexA(i 0, i 0, t "geany_plugins_installer") i .r1 ?e'
	Pop $R0
	StrCmp $R0 0 +3
	MessageBox MB_OK|MB_ICONEXCLAMATION "The installer is already running." /SD IDOK
	Abort

	Call CheckForGeany

	; warn about a new install over an existing installation
	ReadRegStr $R0 SHCTX "${PRODUCT_UNINST_KEY}" "UninstallString"
	StrCmp $R0 "" finish

	MessageBox MB_YESNO|MB_ICONEXCLAMATION \
	"Geany-Plugins has already been installed. $\nDo you want to remove the previous version before installing $(^Name) ?" \
		/SD IDYES IDYES remove IDNO finish

remove:
	; run the uninstaller
	ClearErrors
	; we read the installation path of the old installation from the Registry
	ReadRegStr $UNINSTDIR SHCTX "${PRODUCT_DIR_REGKEY}" "Path"
	IfSilent dosilent nonsilent
dosilent:
	ExecWait '$R0 /S _?=$UNINSTDIR' ;Do not copy the uninstaller to a temp file
	Goto finish
nonsilent:
	ExecWait '$R0 _?=$UNINSTDIR' ;Do not copy the uninstaller to a temp file
finish:
FunctionEnd

Function un.onUninstSuccess
	HideWindow
	MessageBox MB_ICONINFORMATION|MB_OK "$(^Name) was successfully removed from your computer." \
		/SD IDOK
FunctionEnd

Function un.onInit
	; If the user does *not* have administrator privileges, abort
	StrCpy $Answer ""
	!insertmacro IsUserAdmin $Answer $UserName
	${if} $Answer == "yes"
		SetShellVarContext all
	${else}
		; check if the Geany has been installed with admin permisions
		ReadRegStr $0 HKLM "${PRODUCT_UNINST_KEY}" "Publisher"
		${if} $0 != ""
			MessageBox MB_OK|MB_ICONSTOP \
				"You need administrator privileges to uninstall Geany-Plugins!" /SD IDOK
			Abort
		${endif}
		SetShellVarContext current
	${endif}

	MessageBox MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2 \
		"Are you sure you want to completely remove $(^Name) and all of its components?" \
		/SD IDYES IDYES +2
	Abort
FunctionEnd

Function OnDirLeave
	ClearErrors
	SetOutPath "$INSTDIR" ; what about IfError creating $INSTDIR?
	GetTempFileName $1 "$INSTDIR" ; creates tmp file (or fails)
	FileOpen $0 "$1" "w" ; error to open?
	FileWriteByte $0 "0"
	IfErrors notPossible possible

notPossible:
	RMDir "$INSTDIR" ; removes folder if empty
	MessageBox MB_OK "The given directory is not writeable. Please choose another one!" /SD IDOK
	Abort
possible:
	FileClose $0
	Delete "$1"
FunctionEnd

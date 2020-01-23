;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; geany-plugins.nsi - this file is part of the geany-plugins project
;
; Copyright 2009-2011 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
; Copyright 2009-2011 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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
RequestExecutionLevel highest ; set execution level for Windows Vista
; NSIS 3 Unicode support
Unicode true

;;;;;;;;;;;;;;;;;;;
; helper defines  ;
;;;;;;;;;;;;;;;;;;;
!define PRODUCT_NAME "Geany-Plugins"
!define PRODUCT_VERSION "1.37"
!define PRODUCT_VERSION_ID "1.37.0.0"
!define PRODUCT_PUBLISHER "The Geany developer team"
!define PRODUCT_WEB_SITE "https://www.geany.org/"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
!define PRODUCT_DIR_REGKEY "Software\Geany-Plugins"
!define GEANY_DIR_REGKEY "Software\Geany"
!define REQUIRED_GEANY_VERSION "1.37.0"
!define RESOURCEDIR "geany-plugins-${PRODUCT_VERSION}"

;;;;;;;;;;;;;;;;;;;;;
; Version resource  ;
;;;;;;;;;;;;;;;;;;;;;
VIProductVersion "${PRODUCT_VERSION_ID}"
VIAddVersionKey "ProductName" "${PRODUCT_NAME}"
VIAddVersionKey "FileVersion" "${PRODUCT_VERSION}"
VIAddVersionKey "ProductVersion" "${PRODUCT_VERSION}"
VIAddVersionKey "LegalCopyright" "Copyright 2009-2019 by the Geany developer team"
VIAddVersionKey "FileDescription" "${PRODUCT_NAME} Installer"

BrandingText "$(^NAME) installer (NSIS 3.04)"
Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"
SetCompressor /SOLID lzma
ShowInstDetails hide
ShowUnInstDetails hide
XPStyle on
ManifestSupportedOS all
OutFile "geany-plugins-${PRODUCT_VERSION}_setup.exe"

Var Answer
Var UserName
Var UNINSTDIR

;;;;;;;;;;;;;;;;
; MUI Settings ;
;;;;;;;;;;;;;;;;
!include "MUI2.nsh"

;Reserve files used in .onInit, for faster start-up
ReserveFile "${NSISDIR}\Plugins\x86-unicode\System.dll"
ReserveFile "${NSISDIR}\Plugins\x86-unicode\UserInfo.dll"
ReserveFile "${NSISDIR}\Plugins\x86-unicode\InstallOptions.dll"
ReserveFile "${NSISDIR}\Plugins\x86-unicode\LangDLL.dll"

!define MUI_ABORTWARNING
; FIXME hard-coded path...should we add geany.ico to the geany-plugins repo?
!define MUI_ICON "..\geany\icons\geany.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall-full.ico"

; Welcome page
!insertmacro MUI_PAGE_WELCOME
; License page
; FIXME
!insertmacro MUI_PAGE_LICENSE "${RESOURCEDIR}\share\doc\geany-plugins\addons\Copying"
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

	SetOutPath "$INSTDIR\bin"
	File /r "${RESOURCEDIR}\bin\libgeanypluginutils-0.dll"

	SetOutPath "$INSTDIR\lib"
	File /r "${RESOURCEDIR}\lib\*.dll"

	SetOutPath "$INSTDIR\share\geany-plugins"
	File /r "${RESOURCEDIR}\share\geany-plugins\*"
SectionEnd

Section "Language Files" SEC02
	SectionIn 1
	SetOutPath "$INSTDIR\share\locale"
	File /r "${RESOURCEDIR}\share\locale\*"
	; dependency translations
	SetOutPath "$INSTDIR\share\locale"
	File /r "contrib\share\locale\*"
SectionEnd

Section "Documentation" SEC03
	SectionIn 1
	SetOverwrite ifnewer
	SetOutPath "$INSTDIR\share\doc\geany-plugins"
	File /r "${RESOURCEDIR}\share\doc\geany-plugins\*"
SectionEnd

Section "Dependencies" SEC04
	SectionIn 1
	SetOverwrite ifnewer
	SetOutPath "$INSTDIR"
	File /r /x "*.mo" "contrib\"
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
	Delete "$INSTDIR\ReadMe.Dependencies.Geany-Plugins.txt"
	Delete "$INSTDIR\uninst-plugins.exe"
	Delete "$INSTDIR\bin\libgeanypluginutils-0.dll"
	Delete "$INSTDIR\lib\geany\addons.dll"
	Delete "$INSTDIR\lib\geany\autoclose.dll"
	Delete "$INSTDIR\lib\geany\automark.dll"
	Delete "$INSTDIR\lib\geany\codenav.dll"
	Delete "$INSTDIR\lib\geany\commander.dll"
	Delete "$INSTDIR\lib\geany\defineformat.dll"
	Delete "$INSTDIR\lib\geany\geanydoc.dll"
	Delete "$INSTDIR\lib\geany\geanyctags.dll"
	Delete "$INSTDIR\lib\geany\geanyextrasel.dll"
	Delete "$INSTDIR\lib\geany\geanygendoc.dll"
	Delete "$INSTDIR\lib\geany\geanyinsertnum.dll"
	Delete "$INSTDIR\lib\geany\geanylatex.dll"
	Delete "$INSTDIR\lib\geany\latex.dll"
	; Keep for geanylipsum propper deleting old dll some time
	Delete "$INSTDIR\lib\geany\geanylipsum.dll"
	Delete "$INSTDIR\lib\geany\geanylua.dll"
	Delete "$INSTDIR\lib\geany\geanymacro.dll"
	Delete "$INSTDIR\lib\geany\geanyminiscript.dll"
	Delete "$INSTDIR\lib\geany\geanynumberedbookmarks.dll"
	Delete "$INSTDIR\lib\geany\geanypg.dll"
	Delete "$INSTDIR\lib\geany\geanyprj.dll"
	; Keep for geanysendmail propper deleting old dll some time
	Delete "$INSTDIR\lib\geany\geanysendmail.dll"
	Delete "$INSTDIR\lib\geany\geanyvc.dll"
	Delete "$INSTDIR\lib\geany\geniuspaste.dll"
	Delete "$INSTDIR\lib\geany\git-changebar.dll"
	Delete "$INSTDIR\lib\geany\keyrecord.dll"
	Delete "$INSTDIR\lib\geany\lipsum.dll"
	Delete "$INSTDIR\lib\geany\lineoperations.dll"
	Delete "$INSTDIR\lib\geany\markdown.dll"
	Delete "$INSTDIR\lib\geany\overview.dll"
	Delete "$INSTDIR\lib\geany\pairtaghighlighter.dll"
	Delete "$INSTDIR\lib\geany\pohelper.dll"
	Delete "$INSTDIR\lib\geany\pretty-printer.dll"
	Delete "$INSTDIR\lib\geany\projectorganizer.dll"
	Delete "$INSTDIR\lib\geany\scope.dll"
	Delete "$INSTDIR\lib\geany\sendmail.dll"
	Delete "$INSTDIR\lib\geany\shiftcolumn.dll"
	Delete "$INSTDIR\lib\geany\spellcheck.dll"
	Delete "$INSTDIR\lib\geany\tableconvert.dll"
	Delete "$INSTDIR\lib\geany\treebrowser.dll"
	Delete "$INSTDIR\lib\geany\updatechecker.dll"
	Delete "$INSTDIR\lib\geany\vimode.dll"
	Delete "$INSTDIR\lib\geany\webhelper.dll"
	Delete "$INSTDIR\lib\geany\workbench.dll"
	Delete "$INSTDIR\lib\geany\xmlsnippets.dll"

	Delete "$INSTDIR\bin\ctags.exe"
	Delete "$INSTDIR\bin\gpg2.exe"
	Delete "$INSTDIR\bin\gpgconf.exe"
	Delete "$INSTDIR\bin\gpgme-tool.exe"
	Delete "$INSTDIR\bin\gpgme-w32spawn.exe"
	Delete "$INSTDIR\bin\libassuan-0.dll"
	Delete "$INSTDIR\bin\libbrotlicommon.dll"
	Delete "$INSTDIR\bin\libbrotlidec.dll"
	Delete "$INSTDIR\bin\libbrotlienc.dll"
	Delete "$INSTDIR\bin\libctpl-2.dll"
	Delete "$INSTDIR\bin\libcrypto-1_1.dll"
	Delete "$INSTDIR\bin\libcurl-4.dll"
	Delete "$INSTDIR\bin\libdbus-1-3.dll"
	Delete "$INSTDIR\bin\libdbus-glib-1-2.dll"
	Delete "$INSTDIR\bin\libeay32.dll"
	Delete "$INSTDIR\bin\libenchant-2.dll"
	Delete "$INSTDIR\bin\libexslt-0.dll"
	Delete "$INSTDIR\bin\libgcrypt-20.dll"
	Delete "$INSTDIR\bin\libgeoclue-0.dll"
	Delete "$INSTDIR\bin\libgif-7.dll"
	Delete "$INSTDIR\bin\libgit2.dll"
	Delete "$INSTDIR\bin\libgmp-10.dll"
	Delete "$INSTDIR\bin\libgmpxx-4.dll"
	Delete "$INSTDIR\bin\libgnutls-30.dll"
	Delete "$INSTDIR\bin\libgnutlsxx-28.dll"
	Delete "$INSTDIR\bin\libgpg-error-0.dll"
	Delete "$INSTDIR\bin\libgpgme-11.dll"
	Delete "$INSTDIR\bin\libgpgme-glib-11.dll"
	Delete "$INSTDIR\bin\libqgpgme-7.dll"
	Delete "$INSTDIR\bin\libgpgmepp-6.dll"
	Delete "$INSTDIR\bin\libgstallocators-1.0-0.dll"
	Delete "$INSTDIR\bin\libgstapp-1.0-0.dll"
	Delete "$INSTDIR\bin\libgstaudio-1.0-0.dll"
	Delete "$INSTDIR\bin\libgstbase-1.0-0.dll"
	Delete "$INSTDIR\bin\libgstcheck-1.0-0.dll"
	Delete "$INSTDIR\bin\libgstcontroller-1.0-0.dll"
	Delete "$INSTDIR\bin\libgstfft-1.0-0.dll"
	Delete "$INSTDIR\bin\libgstgl-1.0-0.dll"
	Delete "$INSTDIR\bin\libgstnet-1.0-0.dll"
	Delete "$INSTDIR\bin\libgstpbutils-1.0-0.dll"
	Delete "$INSTDIR\bin\libgstreamer-1.0-0.dll"
	Delete "$INSTDIR\bin\libgstriff-1.0-0.dll"
	Delete "$INSTDIR\bin\libgstrtp-1.0-0.dll"
	Delete "$INSTDIR\bin\libgstrtsp-1.0-0.dll"
	Delete "$INSTDIR\bin\libgstsdp-1.0-0.dll"
	Delete "$INSTDIR\bin\libgsttag-1.0-0.dll"
	Delete "$INSTDIR\bin\libgstvideo-1.0-0.dll"
	Delete "$INSTDIR\bin\libgtkspell-0.dll"
	Delete "$INSTDIR\bin\libgtkspell3-*.dll"
	Delete "$INSTDIR\bin\libhistory8.dll"
	Delete "$INSTDIR\bin\libhogweed-5.dll"
	Delete "$INSTDIR\bin\libhttp_parser-2.dll"
	Delete "$INSTDIR\bin\libhunspell-1.7-0.dll"
	Delete "$INSTDIR\bin\libicudt61.dll"
	Delete "$INSTDIR\bin\libicutu61.dll"
	Delete "$INSTDIR\bin\libicuuc61.dll"
	Delete "$INSTDIR\bin\libicuin61.dll"
	Delete "$INSTDIR\bin\libicuio61.dll"
	Delete "$INSTDIR\bin\libicule61.dll"
	Delete "$INSTDIR\bin\libiculx61.dll"
	Delete "$INSTDIR\bin\libicutest61.dll"
	Delete "$INSTDIR\bin\libicutu64.dll"
	Delete "$INSTDIR\bin\libicuuc64.dll"
	Delete "$INSTDIR\bin\libicudt64.dll"
	Delete "$INSTDIR\bin\libicuin64.dll"
	Delete "$INSTDIR\bin\libicuio64.dll"
	Delete "$INSTDIR\bin\libicule64.dll"
	Delete "$INSTDIR\bin\libiculx64.dll"
	Delete "$INSTDIR\bin\libicutest64.dll"
	Delete "$INSTDIR\bin\libicutu64.dll"
	Delete "$INSTDIR\bin\libicuuc64.dll"
	Delete "$INSTDIR\bin\libidn2-0.dll"
	Delete "$INSTDIR\bin\libjavascriptcoregtk-1.0-0.dll"
	Delete "$INSTDIR\bin\libjavascriptcoregtk-3.0-0.dll"
	Delete "$INSTDIR\bin\libjpeg-8.dll"
	Delete "$INSTDIR\bin\liblzma-5.dll"
	Delete "$INSTDIR\bin\libnettle-7.dll"
	Delete "$INSTDIR\bin\libnghttp2-14.dll"
	Delete "$INSTDIR\bin\libogg-0.dll"
	Delete "$INSTDIR\bin\liborc-0.4-0.dll"
	Delete "$INSTDIR\bin\liborc-test-0.4-0.dll"
	Delete "$INSTDIR\bin\libp11-kit-0.dll"
	Delete "$INSTDIR\bin\libproxy-1.dll"
	Delete "$INSTDIR\bin\libpsl-5.dll"
	Delete "$INSTDIR\bin\libreadline8.dll"
	Delete "$INSTDIR\bin\librtmp-1.dll"
	Delete "$INSTDIR\bin\libsoup-2.4-1.dll"
	Delete "$INSTDIR\bin\libsoup-gnome-2.4-1.dll"
	Delete "$INSTDIR\bin\libsqlite3-0.dll"
	Delete "$INSTDIR\bin\libssh2-1.dll"
	Delete "$INSTDIR\bin\libssl-1_1.dll"
	Delete "$INSTDIR\bin\libsystre-0.dll"
	Delete "$INSTDIR\bin\libtasn1-6.dll"
	Delete "$INSTDIR\bin\libtermcap-0.dll"
	Delete "$INSTDIR\bin\libtheora-0.dll"
	Delete "$INSTDIR\bin\libtheoradec-1.dll"
	Delete "$INSTDIR\bin\libtheoraenc-1.dll"
	Delete "$INSTDIR\bin\libtiff-5.dll"
	Delete "$INSTDIR\bin\libtiffxx-5.dll"
	Delete "$INSTDIR\bin\libtre-5.dll"
	Delete "$INSTDIR\bin\libturbojpeg.dll"
	Delete "$INSTDIR\bin\libunistring-2.dll"
	Delete "$INSTDIR\bin\libvorbis-0.dll"
	Delete "$INSTDIR\bin\libvorbisenc-2.dll"
	Delete "$INSTDIR\bin\libvorbisfile-3.dll"
	Delete "$INSTDIR\bin\libvorbisidec-1.dll"
	Delete "$INSTDIR\bin\libwebkitgtk-1.0-0.dll"
	Delete "$INSTDIR\bin\libwebkitgtk-3.0-0.dll"
	Delete "$INSTDIR\bin\libwebp-7.dll"
	Delete "$INSTDIR\bin\libwebpdecoder-3.dll"
	Delete "$INSTDIR\bin\libwebpdemux-2.dll"
	Delete "$INSTDIR\bin\libwebpextras-0.dll"
	Delete "$INSTDIR\bin\libwebpmux-3.dll"
	Delete "$INSTDIR\bin\libxml2-2.dll"
	Delete "$INSTDIR\bin\libxslt-1.dll"
	Delete "$INSTDIR\bin\lua51.dll"
	Delete "$INSTDIR\bin\ssleay32.dll"

	RMDir /r "$INSTDIR\etc\pki"
	RMDir /r "$INSTDIR\lib\enchant-2"
	RMDir /r "$INSTDIR\lib\engines"
	RMDir /r "$INSTDIR\lib\engines-1_1"
	RMDir /r "$INSTDIR\lib\gio"
	RMDir /r "$INSTDIR\lib\gstreamer-1.0"
	RMDir /r "$INSTDIR\lib\pkcs11"
	RMDir /r "$INSTDIR\lib\sqlite3.29.0"
	RMDir /r "$INSTDIR\lib\geany-plugins"
	RMDir /r "$INSTDIR\libexec\p11-kit"
	RMDir /r "$INSTDIR\share\doc\geany-plugins"
	RMDir /r "$INSTDIR\share\geany-plugins"
	RMDir /r "$INSTDIR\share\enchant"
	RMDir /r "$INSTDIR\share\libgpg-error"
	RMDir /r "$INSTDIR\share\p11-kit"
	RMDir /r "$INSTDIR\share\pki"
	RMDir /r "$INSTDIR\share\vala"
	RMDir /r "$INSTDIR\share\webkitgtk-1.0"
	RMDir /r "$INSTDIR\share\webkitgtk-3.0"
	RMDir /r "$INSTDIR\share\xml\dbus-1"
	RMDir /r "$INSTDIR\ssl\certs"

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
	RMDir "$INSTDIR\bin"
	RMDir "$INSTDIR\etc"
	RMDir "$INSTDIR\lib\geany"
	RMDir "$INSTDIR\lib"
	RMDir "$INSTDIR\libexec"
	RMDir "$INSTDIR\share\doc"
	RMDir "$INSTDIR\share\icons\hicolor\16x16\apps"
	RMDir "$INSTDIR\share\icons\hicolor\16x16"
	RMDir "$INSTDIR\share\icons\hicolor\24x24\apps"
	RMDir "$INSTDIR\share\icons\hicolor\24x24"
	RMDir "$INSTDIR\share\icons\hicolor\32x32\apps"
	RMDir "$INSTDIR\share\icons\hicolor\32x32"
	RMDir "$INSTDIR\share\icons\hicolor\48x48\apps"
	RMDir "$INSTDIR\share\icons\hicolor\48x48"
	RMDir "$INSTDIR\share\icons\hicolor\scalable\apps"
	RMDir "$INSTDIR\share\icons\hicolor\scalable"
	RMDir "$INSTDIR\share\icons\hicolor"
	RMDir "$INSTDIR\share\icons"
	RMDir "$INSTDIR\share\locale"
	RMDir "$INSTDIR\share\xml"
	RMDir "$INSTDIR\share"
	RMDir "$INSTDIR\ssl\certs"
	RMDir "$INSTDIR\ssl"
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
!insertmacro MUI_DESCRIPTION_TEXT ${SEC04} "Dependency files for various plugins (GeanyCTags, GeanyGenDoc, SpellCheck, WebHelper, Markdown, GeanyLua, GeanyPG, UpdateChecker, GitChangeBar, PrettyPrinter, GeanyVC)."
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
	IntOp $R4 $R1 >> 16
	IntOp $R4 $R4 & 0x0000FFFF ; $R4 now contains release
	StrCpy $0 "$R2.$R3.$R4"
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

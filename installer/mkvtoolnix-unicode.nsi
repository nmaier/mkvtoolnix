!define PRODUCT_NAME "MKVtoolnix"
!define PRODUCT_VERSION "2.9.8"
!define PRODUCT_VERSION_BUILD ""  # Intentionally left empty for releases
!define PRODUCT_PUBLISHER "Moritz Bunkus"
!define PRODUCT_WEB_SITE "http://www.bunkus.org/videotools/mkvtoolnix/"
!define PRODUCT_DIR_REGKEY "Software\Microsoft\Windows\CurrentVersion\App Paths\mmg.exe"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
!define PRODUCT_UNINST_ROOT_KEY "HKLM"
!define PRODUCT_STARTMENU_REGVAL "NSIS:StartMenuDir"

!define MTX_REGKEY "Software\mkvmergeGUI"

SetCompressor /SOLID lzma

!include "MUI2.nsh"

# MUI Settings
!define MUI_ABORTWARNING
!define MUI_ICON "matroskalogo_big.ico"

# Language Selection Dialog Settings
!define MUI_LANGDLL_REGISTRY_ROOT "${PRODUCT_UNINST_ROOT_KEY}"
!define MUI_LANGDLL_REGISTRY_KEY "${PRODUCT_UNINST_KEY}"
!define MUI_LANGDLL_REGISTRY_VALUENAME "NSIS:Language"

# Settings for the start menu group page
var ICONS_GROUP
!define MUI_STARTMENUPAGE_NODISABLE
!define MUI_STARTMENUPAGE_DEFAULTFOLDER "MKVtoolnix"
!define MUI_STARTMENUPAGE_REGISTRY_ROOT "${PRODUCT_UNINST_ROOT_KEY}"
!define MUI_STARTMENUPAGE_REGISTRY_KEY "${PRODUCT_UNINST_KEY}"
!define MUI_STARTMENUPAGE_REGISTRY_VALUENAME "${PRODUCT_STARTMENU_REGVAL}"

# Settings for the finish page (offer to run mmg.exe)
!define MUI_FINISHPAGE_RUN "$INSTDIR\mmg.exe"

# Welcome page
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_STARTMENU Application $ICONS_GROUP
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

# Uninstaller pages
!insertmacro MUI_UNPAGE_INSTFILES

# Language files
!insertmacro MUI_LANGUAGE "English"

# MUI end ------

!include "WinVer.nsh"
!include "EnvVarUpdate.nsh"
!include "LogicLib.nsh"

Name "${PRODUCT_NAME} ${PRODUCT_VERSION}${PRODUCT_VERSION_BUILD}"
BrandingText "${PRODUCT_NAME} ${PRODUCT_VERSION}${PRODUCT_VERSION_BUILD} by ${PRODUCT_PUBLISHER}"
OutFile "mkvtoolnix-unicode-${PRODUCT_VERSION}-setup.exe"
InstallDir "$PROGRAMFILES\MKVtoolnix"
InstallDirRegKey HKLM "${PRODUCT_DIR_REGKEY}" ""
ShowInstDetails show
ShowUnInstDetails show

Function .onInit
  # Check if we're running on a Unicode capable Windows.
  # If not, abort.
  ${IfNot} ${AtLeastWinNT4}
    MessageBox MB_OK|MB_ICONSTOP "You are trying to install MKVToolNix on a Windows version that does not support Unicode (95, 98 or ME). These old Windows versions are not supported anymore. You can still get an older version (v2.2.0) for Windows 95, 98 and ME from http://www.bunkus.org/videotools/mkvtoolnix/"
    Quit
  ${EndIf}
FunctionEnd

Section "Program files" SEC01
  SetShellVarContext all

  SetOutPath "$INSTDIR"
  SetOverwrite ifnewer
;   File "libebml.dll"
  File "libiconv-2.dll"
  File "libintl-8.dll"
;   File "libmatroska.dll"
  File "magic1.dll"
  File "regex2.dll"
  File "zlib1.dll"
  File "matroskalogo_big.ico"
  File "mkvextract.exe"
  File "mkvinfo.exe"
  File "mkvmerge.exe"
  File "mkvpropedit.exe"
  File "mmg.exe"
  File "wxbase28u_gcc_custom.dll"
  File "wxmsw28u_core_gcc_custom.dll"
  File "wxmsw28u_html_gcc_custom.dll"
  SetOutPath "$INSTDIR\data"
  File "data\magic"
  File "data\magic.mgc"
  SetOutPath "$INSTDIR\doc"
  File "doc\ChangeLog.txt"
  File "doc\COPYING.txt"
  File "doc\mkvextract.html"
  File "doc\mkvinfo.html"
  File "doc\mkvmerge-gui.hhc"
  File "doc\mkvmerge-gui.hhk"
  File "doc\mkvmerge-gui.hhp"
  File "doc\mkvmerge-gui.html"
  File "doc\mkvmerge.html"
  File "doc\mkvpropedit.html"
  File "doc\mmg.html"
  File "doc\README.txt"
  File "doc\README.Windows.txt"
  SetOutPath "$INSTDIR\doc\images"
  File "doc\images\addingremovingattachments.gif"
  File "doc\images\addremovefiles.gif"
  File "doc\images\attachmentoptions.gif"
  File "doc\images\audiotrackoptions.gif"
  File "doc\images\chaptereditor.gif"
  File "doc\images\generaltrackoptions.gif"
  File "doc\images\jobmanager.gif"
  File "doc\images\movietitle.gif"
  File "doc\images\muxingwindow.gif"
  File "doc\images\selectmkvmergeexecutable.gif"
  File "doc\images\splitting.gif"
  File "doc\images\textsubtitlestrackoptions.gif"
  File "doc\images\trackselection.gif"
  File "doc\images\videotrackoptions.gif"
  SetOutPath "$INSTDIR\examples"
  File "examples\example-chapters-1.xml"
  File "examples\example-chapters-2.xml"
  File "examples\example-cue-sheet-1.cue"
  File "examples\example-segmentinfo-1.xml"
  File "examples\example-tags-2.xml"
  File "examples\example-timecodes-v1.txt"
  File "examples\example-timecodes-v2.txt"
  File "examples\matroskachapters.dtd"
  File "examples\matroskasegmentinfo.dtd"
  File "examples\matroskatags.dtd"
  SetOutPath "$INSTDIR\locale\de\LC_MESSAGES"
  File "/oname=mkvtoolnix.mo" "po\de.mo"
  SetOutPath "$INSTDIR\locale\ja\LC_MESSAGES"
  File "/oname=mkvtoolnix.mo" "po\ja.mo"
  SetOutPath "$INSTDIR\locale\zh_CN\LC_MESSAGES"
  File "/oname=mkvtoolnix.mo" "po\zh_CN.mo"
  SetOutPath "$INSTDIR\locale\zh_TW\LC_MESSAGES"
  File "/oname=mkvtoolnix.mo" "po\zh_TW.mo"

  # Delete files that might be present from older installation
  # if this is just an upgrade.
  Delete "$INSTDIR\base64tool.exe"
  Delete "$INSTDIR\doc\base64tool.html"
  Delete "$INSTDIR\cygz.dll"
  Delete "$INSTDIR\libcharset.dll"
  Delete "$INSTDIR\libiconv.dll"

  Delete "$INSTDIR\locale\german\LC_MESSAGES\mkvtoolnix.mo"
  RMDir "$INSTDIR\locale\german\LC_MESSAGES"
  RMDir "$INSTDIR\locale\german"

  Delete "$INSTDIR\locale\zh\LC_MESSAGES\mkvtoolnix.mo"
  RMDir "$INSTDIR\locale\zh\LC_MESSAGES"
  RMDir "$INSTDIR\locale\zh"

  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\App Paths\AppMainExe.exe"
  Delete "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\base64tool CLI reference.lnk"
  SetShellVarContext current
  Delete "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\base64tool CLI reference.lnk"
  SetShellVarContext all

  # Shortcuts
  !insertmacro MUI_STARTMENU_WRITE_BEGIN Application
  CreateDirectory "$SMPROGRAMS\$ICONS_GROUP"
  SetOutPath "$INSTDIR"
  CreateShortCut "$SMPROGRAMS\$ICONS_GROUP\mkvmerge GUI.lnk" "$INSTDIR\mmg.exe" "" "$INSTDIR\matroskalogo_big.ico"
  CreateShortCut "$SMPROGRAMS\$ICONS_GROUP\mkvinfo GUI.lnk" "$INSTDIR\mkvinfo.exe" "-g" "$INSTDIR\matroskalogo_big.ico"
  SetOutPath "$INSTDIR\Doc"
  CreateDirectory "$SMPROGRAMS\$ICONS_GROUP\Documentation"
  CreateShortCut "$SMPROGRAMS\$ICONS_GROUP\Documentation\mkvmerge GUI guide.lnk" "$INSTDIR\doc\mkvmerge-gui.html"
  CreateDirectory "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference"
  CreateShortCut "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\mkvmerge CLI reference.lnk" "$INSTDIR\doc\mkvmerge.html"
  CreateShortCut "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\mkvinfo CLI reference.lnk" "$INSTDIR\doc\mkvinfo.html"
  CreateShortCut "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\mkvextract CLI reference.lnk" "$INSTDIR\doc\mkvextract.html"
  CreateShortCut "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\mkvpropedit CLI reference.lnk" "$INSTDIR\doc\mkvpropedit.html"
  CreateDirectory "$SMPROGRAMS\$ICONS_GROUP\Documentation\Other documentation"
  CreateShortCut "$SMPROGRAMS\$ICONS_GROUP\Documentation\Other documentation\ChangeLog - What is new.lnk" "$INSTDIR\doc\ChangeLog.txt"
  CreateShortCut "$SMPROGRAMS\$ICONS_GROUP\Documentation\Other documentation\README.lnk" "$INSTDIR\doc\README.txt"
  CreateShortCut "$SMPROGRAMS\$ICONS_GROUP\Documentation\Other documentation\The GNU GPL.lnk" "$INSTDIR\doc\Copying.txt"
  !insertmacro MUI_STARTMENU_WRITE_END

  SetOutPath "$INSTDIR"
  IfSilent +3 0
  MessageBox MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2 "Should a shortcut be placed on the desktop?" IDNO +2
  CreateShortCut "$DESKTOP\mkvmerge GUI.lnk" "$INSTDIR\mmg.exe" "" "$INSTDIR\matroskalogo_big.ico"
SectionEnd

Section -AdditionalIcons
  !insertmacro MUI_STARTMENU_WRITE_BEGIN Application
  WriteIniStr "$INSTDIR\${PRODUCT_NAME}.url" "InternetShortcut" "URL" "${PRODUCT_WEB_SITE}"
  CreateShortCut "$SMPROGRAMS\$ICONS_GROUP\Website.lnk" "$INSTDIR\${PRODUCT_NAME}.url"
  CreateShortCut "$SMPROGRAMS\$ICONS_GROUP\Uninstall.lnk" "$INSTDIR\uninst.exe"
  !insertmacro MUI_STARTMENU_WRITE_END
SectionEnd

Section -Post
  WriteUninstaller "$INSTDIR\uninst.exe"
  WriteRegStr HKLM "${PRODUCT_DIR_REGKEY}" "" "$INSTDIR\mmg.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayName" "$(^Name)"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "UninstallString" "$INSTDIR\uninst.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayIcon" "$INSTDIR\mmg.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "URLInfoAbout" "${PRODUCT_WEB_SITE}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "Publisher" "${PRODUCT_PUBLISHER}"
  WriteRegStr HKLM "${MTX_REGKEY}\GUI" "installation_path" "$INSTDIR"

  ${EnvVarUpdate} $0 "PATH" "A" "HKLM" "$INSTDIR"
SectionEnd

var unRemoveJobs

Function un.onUninstSuccess
  HideWindow
  MessageBox MB_ICONINFORMATION|MB_OK "$(^Name) was successfully uninstalled."
FunctionEnd

Function un.onInit
  !insertmacro MUI_UNGETLANGUAGE
  MessageBox MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2 "Do you really want to remove $(^Name) and all of its components?" IDYES +2
  Abort
  StrCpy $unRemoveJobs "No"
  IfFileExists "$INSTDIR\jobs\*.*" +2
  Return
  MessageBox MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2 "Should job files created by the GUI be deleted as well?" IDYES +2
  Return
  StrCpy $unRemoveJobs "Yes"
FunctionEnd

Section Uninstall
  SetShellVarContext all

  !insertmacro MUI_STARTMENU_GETFOLDER "Application" $ICONS_GROUP
  Delete "$SMPROGRAMS\$ICONS_GROUP\Uninstall.lnk"
  Delete "$SMPROGRAMS\$ICONS_GROUP\Website.lnk"

  Delete "$SMPROGRAMS\$ICONS_GROUP\mkvmerge GUI.lnk"
  Delete "$SMPROGRAMS\$ICONS_GROUP\mkvinfo GUI.lnk"
  Delete "$SMPROGRAMS\$ICONS_GROUP\Documentation\mkvmerge GUI guide.lnk"
  Delete "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\mkvmerge CLI reference.lnk"
  Delete "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\mkvinfo CLI reference.lnk"
  Delete "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\mkvextract CLI reference.lnk"
  Delete "$SMPROGRAMS\$ICONS_GROUP\Documentation\Other documentation\ChangeLog - What is new.lnk"
  Delete "$SMPROGRAMS\$ICONS_GROUP\Documentation\Other documentation\README.lnk"
  Delete "$SMPROGRAMS\$ICONS_GROUP\Documentation\Other documentation\The GNU GPL.lnk"

  RMDir "$SMPROGRAMS\$ICONS_GROUP\Documentation\Other documentation"
  RMDir "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference"
  RMDir "$SMPROGRAMS\$ICONS_GROUP\Documentation"
  RMDir "$SMPROGRAMS\$ICONS_GROUP"

  Delete "$DESKTOP\mkvmerge GUI.lnk"

  Delete "$INSTDIR\${PRODUCT_NAME}.url"
  Delete "$INSTDIR\uninst.exe"
  Delete "$INSTDIR\libcharset.dll"
  Delete "$INSTDIR\libebml.dll"
  Delete "$INSTDIR\libiconv-2.dll"
  Delete "$INSTDIR\libintl-8.dll"
  Delete "$INSTDIR\libmatroska.dll"
  Delete "$INSTDIR\matroskalogo_big.ico"
  Delete "$INSTDIR\mkvextract.exe"
  Delete "$INSTDIR\mkvinfo.exe"
  Delete "$INSTDIR\mkvmerge.exe"
  Delete "$INSTDIR\mkvpropedit.exe"
  Delete "$INSTDIR\mmg.exe"
  Delete "$INSTDIR\magic1.dll"
  Delete "$INSTDIR\regex2.dll"
  Delete "$INSTDIR\wxbase28u_gcc_custom.dll"
  Delete "$INSTDIR\wxmsw28u_core_gcc_custom.dll"
  Delete "$INSTDIR\wxmsw28u_html_gcc_custom.dll"
  Delete "$INSTDIR\zlib1.dll"

  Delete "$INSTDIR\data\magic"
  Delete "$INSTDIR\data\magic.mgc"

  Delete "$INSTDIR\doc\ChangeLog.txt"
  Delete "$INSTDIR\doc\COPYING.txt"
  Delete "$INSTDIR\doc\mkvpropedit.html"
  Delete "$INSTDIR\doc\mkvextract.html"
  Delete "$INSTDIR\doc\mkvinfo.html"
  Delete "$INSTDIR\doc\mkvmerge-gui.hhc"
  Delete "$INSTDIR\doc\mkvmerge-gui.hhk"
  Delete "$INSTDIR\doc\mkvmerge-gui.hhp"
  Delete "$INSTDIR\doc\mkvmerge-gui.html"
  Delete "$INSTDIR\doc\mkvmerge.html"
  Delete "$INSTDIR\doc\mmg.html"
  Delete "$INSTDIR\doc\README.txt"
  Delete "$INSTDIR\doc\README.Windows.txt"

  Delete "$INSTDIR\doc\images\addingremovingattachments.gif"
  Delete "$INSTDIR\doc\images\addremovefiles.gif"
  Delete "$INSTDIR\doc\images\attachmentoptions.gif"
  Delete "$INSTDIR\doc\images\audiotrackoptions.gif"
  Delete "$INSTDIR\doc\images\chaptereditor.gif"
  Delete "$INSTDIR\doc\images\generaltrackoptions.gif"
  Delete "$INSTDIR\doc\images\jobmanager.gif"
  Delete "$INSTDIR\doc\images\movietitle.gif"
  Delete "$INSTDIR\doc\images\muxingwindow.gif"
  Delete "$INSTDIR\doc\images\selectmkvmergeexecutable.gif"
  Delete "$INSTDIR\doc\images\splitting.gif"
  Delete "$INSTDIR\doc\images\textsubtitlestrackoptions.gif"
  Delete "$INSTDIR\doc\images\trackselection.gif"
  Delete "$INSTDIR\doc\images\videotrackoptions.gif"

  Delete "$INSTDIR\examples\example-chapters-1.xml"
  Delete "$INSTDIR\examples\example-chapters-2.xml"
  Delete "$INSTDIR\examples\example-cue-sheet-1.cue"
  Delete "$INSTDIR\examples\example-segmentinfo-1.xml"
  Delete "$INSTDIR\examples\example-tags-2.xml"
  Delete "$INSTDIR\examples\example-timecodes-v1.txt"
  Delete "$INSTDIR\examples\example-timecodes-v2.txt"
  Delete "$INSTDIR\examples\matroskachapters.dtd"
  Delete "$INSTDIR\examples\matroskasegmentinfo.dtd"
  Delete "$INSTDIR\examples\matroskatags.dtd"

  Delete "$INSTDIR\locale\de\LC_MESSAGES\mkvtoolnix.mo"
  RMDir "$INSTDIR\locale\de\LC_MESSAGES"
  RMDir "$INSTDIR\locale\de"

  Delete "$INSTDIR\locale\ja\LC_MESSAGES\mkvtoolnix.mo"
  RMDir "$INSTDIR\locale\ja\LC_MESSAGES"
  RMDir "$INSTDIR\locale\ja"

  Delete "$INSTDIR\locale\zh_CN\LC_MESSAGES\mkvtoolnix.mo"
  RMDir "$INSTDIR\locale\zh_CN\LC_MESSAGES"
  RMDir "$INSTDIR\locale\zh_CN"

  Delete "$INSTDIR\locale\zh_TW\LC_MESSAGES\mkvtoolnix.mo"
  RMDir "$INSTDIR\locale\zh_TW\LC_MESSAGES"
  RMDir "$INSTDIR\locale\zh_TW"

  # From previous versions of mkvtoolnix: translation to Simplified Chinese
  Delete "$INSTDIR\locale\zh\LC_MESSAGES\mkvtoolnix.mo"
  RMDir "$INSTDIR\locale\zh\LC_MESSAGES"
  RMDir "$INSTDIR\locale\zh"

  RMDir "$INSTDIR\locale"
  RMDir "$INSTDIR\data"
  RMDir "$INSTDIR\doc\images"
  RMDir "$INSTDIR\doc"
  RMDir "$INSTDIR\examples"

  StrCmp $unRemoveJobs "Yes" 0 +2
  Delete "$INSTDIR\jobs\*.mmg"
  RMDir "$INSTDIR\jobs"

  RMDir "$INSTDIR"

  DeleteRegKey ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}"
  DeleteRegKey HKLM "${PRODUCT_DIR_REGKEY}"
  DeleteRegKey HKLM "${MTX_REGKEY}"
  DeleteRegKey HKCU "${MTX_REGKEY}"

  ${un.EnvVarUpdate} $0 "PATH" "R" "HKLM" "$INSTDIR"

  SetAutoClose true
SectionEnd

!define PRODUCT_NAME "MKVToolNix"
!define PRODUCT_VERSION "6.5.0"
!define PRODUCT_VERSION_BUILD ""  # Intentionally left empty for releases
!define PRODUCT_PUBLISHER "Moritz Bunkus"
!define PRODUCT_WEB_SITE "http://www.bunkus.org/videotools/mkvtoolnix/"
!define PRODUCT_DIR_REGKEY "Software\Microsoft\Windows\CurrentVersion\App Paths\mmg.exe"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
!define PRODUCT_UNINST_ROOT_KEY "HKLM"
!define PRODUCT_STARTMENU_REGVAL "NSIS:StartMenuDir"

!define MTX_REGKEY "Software\mkvmergeGUI"

SetCompressor /SOLID lzma
#SetCompress off
SetCompressorDictSize 64

!include "MUI2.nsh"

# MUI Settings
!define MUI_ABORTWARNING
!define MUI_ICON "mkvmergeGUI.ico"

# Language Selection Dialog Settings
!define MUI_LANGDLL_REGISTRY_ROOT "HKCU"
!define MUI_LANGDLL_REGISTRY_KEY "${MTX_REGKEY}"
!define MUI_LANGDLL_REGISTRY_VALUENAME "Installer Language"

# Settings for the start menu group page
var ICONS_GROUP
!define MUI_STARTMENUPAGE_DEFAULTFOLDER "${PRODUCT_NAME}"
!define MUI_STARTMENUPAGE_REGISTRY_ROOT "${PRODUCT_UNINST_ROOT_KEY}"
!define MUI_STARTMENUPAGE_REGISTRY_KEY "${PRODUCT_UNINST_KEY}"
!define MUI_STARTMENUPAGE_REGISTRY_VALUENAME "${PRODUCT_STARTMENU_REGVAL}"

!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_RIGHT
!define MUI_HEADERIMAGE_BITMAP "header_image.bmp"

# Settings for the finish page
!define MUI_FINISHPAGE_NOREBOOTSUPPORT
!define MUI_FINISHPAGE_TITLE_3LINES

# Welcome page
!define MUI_WELCOMEFINISHPAGE_BITMAP "welcome_finish_page.bmp"
!define MUI_WELCOMEPAGE_TITLE_3LINES

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_STARTMENU Application $ICONS_GROUP
!insertmacro MUI_PAGE_INSTFILES
Page custom showExternalLinks
!insertmacro MUI_PAGE_FINISH

# Uninstaller pages
!insertmacro MUI_UNPAGE_INSTFILES

# Language files
!macro LANG_LOAD LANGLOAD
  !insertmacro MUI_LANGUAGE "${LANGLOAD}"
  !include "translations\${LANGLOAD}.nsh"
  !undef LANG
!macroend

!macro LANG_STRING NAME VALUE
  LangString "${NAME}" "${LANG_${LANG}}" "${VALUE}"
!macroend

!macro LANG_UNSTRING NAME VALUE
  !insertmacro LANG_STRING "un.${NAME}" "${VALUE}"
!macroend

!insertmacro LANG_LOAD "Czech"
!insertmacro LANG_LOAD "Dutch"
!insertmacro LANG_LOAD "English"
!insertmacro LANG_LOAD "French"
!insertmacro LANG_LOAD "German"
!insertmacro LANG_LOAD "Italian"
!insertmacro LANG_LOAD "Japanese"
!insertmacro LANG_LOAD "Lithuanian"
!insertmacro LANG_LOAD "Polish"
!insertmacro LANG_LOAD "Portuguese"
!insertmacro LANG_LOAD "Russian"
!insertmacro LANG_LOAD "Spanish"
!insertmacro LANG_LOAD "SimpChinese"
!insertmacro LANG_LOAD "TradChinese"
!insertmacro LANG_LOAD "Ukrainian"
!define MUI_LANGDLL_ALLLANGUAGES

!insertmacro MUI_RESERVEFILE_LANGDLL

# MUI end ------

!include "WinVer.nsh"
!include "LogicLib.nsh"

Name "${PRODUCT_NAME} ${PRODUCT_VERSION}${PRODUCT_VERSION_BUILD}"
BrandingText "${PRODUCT_NAME} ${PRODUCT_VERSION}${PRODUCT_VERSION_BUILD} by ${PRODUCT_PUBLISHER}"
OutFile "mkvtoolnix-unicode-${PRODUCT_VERSION}-setup.exe"
InstallDir "$PROGRAMFILES\${PRODUCT_NAME}"
InstallDirRegKey HKLM "${PRODUCT_DIR_REGKEY}" ""
ShowInstDetails show
ShowUnInstDetails show

VIProductVersion "${PRODUCT_VERSION}.0"
VIAddVersionKey /LANG=${LANG_ENGLISH} "ProductName" "${PRODUCT_NAME}"
VIAddVersionKey /LANG=${LANG_ENGLISH} "ProductVersion" "${PRODUCT_VERSION}${PRODUCT_VERSION_BUILD}"
VIAddVersionKey /LANG=${LANG_ENGLISH} "Comments" "${PRODUCT_NAME} is a set of tools to create, alter and inspect Matroska files under Linux, other Unices and Windows."
VIAddVersionKey /LANG=${LANG_ENGLISH} "CompanyName" "${PRODUCT_PUBLISHER}"
VIAddVersionKey /LANG=${LANG_ENGLISH} "LegalCopyright" "${PRODUCT_PUBLISHER} ${PRODUCT_WEB_SITE}"
VIAddVersionKey /LANG=${LANG_ENGLISH} "FileDescription" "${PRODUCT_NAME} ${PRODUCT_VERSION}${PRODUCT_VERSION_BUILD}"
VIAddVersionKey /LANG=${LANG_ENGLISH} "FileVersion" "${PRODUCT_VERSION}${PRODUCT_VERSION_BUILD}"

RequestExecutionLevel none

Function .onInit
  !insertmacro MUI_LANGDLL_DISPLAY

  InitPluginsDir
  File /oname=$PLUGINSDIR\external_links.ini "external_links.ini"
FunctionEnd

Section "Program files" SEC01
  SetShellVarContext all

  SetOutPath "$INSTDIR"
  File "mkvextract.exe"
  File "mkvinfo.exe"
  File "mkvmerge.exe"
  File "mkvpropedit.exe"
  File "mmg.exe"
  SetOutPath "$INSTDIR\data"
  File "data\magic.mgc"
  SetOutPath "$INSTDIR\doc"
  File "doc\ChangeLog.txt"
  File "doc\COPYING.txt"
  File "doc\README.txt"
  File "doc\README.Windows.txt"
  SetOutPath "$INSTDIR\doc\en"
  File "doc\en\mkvextract.html"
  File "doc\en\mkvinfo.html"
  File "doc\en\mkvmerge.html"
  File "doc\en\mkvpropedit.html"
  File "doc\en\mmg.html"
  File "doc\en\mkvtoolnix-doc.css"
  SetOutPath "$INSTDIR\doc\de"
  File "doc\de\mkvextract.html"
  File "doc\de\mkvinfo.html"
  File "doc\de\mkvmerge.html"
  File "doc\de\mkvpropedit.html"
  File "doc\de\mmg.html"
  File "doc\de\mkvtoolnix-doc.css"
  SetOutPath "$INSTDIR\doc\ja"
  File "doc\ja\mkvextract.html"
  File "doc\ja\mkvinfo.html"
  File "doc\ja\mkvmerge.html"
  File "doc\ja\mkvpropedit.html"
  File "doc\ja\mmg.html"
  File "doc\ja\mkvtoolnix-doc.css"
  SetOutPath "$INSTDIR\doc\nl"
  File "doc\nl\mkvextract.html"
  File "doc\nl\mkvinfo.html"
  File "doc\nl\mkvmerge.html"
  File "doc\nl\mkvpropedit.html"
  File "doc\nl\mmg.html"
  File "doc\nl\mkvtoolnix-doc.css"
  SetOutPath "$INSTDIR\doc\uk"
  File "doc\uk\mkvextract.html"
  File "doc\uk\mkvinfo.html"
  File "doc\uk\mkvmerge.html"
  File "doc\uk\mkvpropedit.html"
  File "doc\uk\mmg.html"
  File "doc\uk\mkvtoolnix-doc.css"
  SetOutPath "$INSTDIR\doc\zh_CN"
  File "doc\zh_CN\mkvextract.html"
  File "doc\zh_CN\mkvinfo.html"
  File "doc\zh_CN\mkvmerge.html"
  File "doc\zh_CN\mkvpropedit.html"
  File "doc\zh_CN\mmg.html"
  File "doc\zh_CN\mkvtoolnix-doc.css"
  SetOutPath "$INSTDIR\doc\guide\en"
  File "doc\guide\en\mkvmerge-gui.hhc"
  File "doc\guide\en\mkvmerge-gui.hhk"
  File "doc\guide\en\mkvmerge-gui.hhp"
  File "doc\guide\en\mkvmerge-gui.html"
  SetOutPath "$INSTDIR\doc\guide\en\images"
  File "doc\guide\en\images\addingremovingattachments.gif"
  File "doc\guide\en\images\addremovefiles.gif"
  File "doc\guide\en\images\attachmentoptions.gif"
  File "doc\guide\en\images\audiotrackoptions.gif"
  File "doc\guide\en\images\chaptereditor.gif"
  File "doc\guide\en\images\generaltrackoptions.gif"
  File "doc\guide\en\images\jobmanager.gif"
  File "doc\guide\en\images\movietitle.gif"
  File "doc\guide\en\images\muxingwindow.gif"
  File "doc\guide\en\images\selectmkvmergeexecutable.gif"
  File "doc\guide\en\images\splitting.gif"
  File "doc\guide\en\images\textsubtitlestrackoptions.gif"
  File "doc\guide\en\images\trackselection.gif"
  File "doc\guide\en\images\videotrackoptions.gif"
  SetOutPath "$INSTDIR\doc\guide\es"
  File "doc\guide\es\mkvmerge-gui.hhc"
  File "doc\guide\es\mkvmerge-gui.hhk"
  File "doc\guide\es\mkvmerge-gui.hhp"
  File "doc\guide\es\mkvmerge-gui.html"
  SetOutPath "$INSTDIR\doc\guide\es\images"
  File "doc\guide\es\images\addingremovingattachments.gif"
  File "doc\guide\es\images\addremovefiles.gif"
  File "doc\guide\es\images\attachmentoptions.gif"
  File "doc\guide\es\images\audiotrackoptions.gif"
  File "doc\guide\es\images\chaptereditor.gif"
  File "doc\guide\es\images\figura2.png"
  File "doc\guide\es\images\figura11.png"
  File "doc\guide\es\images\figura12.png"
  File "doc\guide\es\images\figura13.png"
  File "doc\guide\es\images\figura14.png"
  File "doc\guide\es\images\figura15.png"
  File "doc\guide\es\images\figura16.png"
  File "doc\guide\es\images\figura17.png"
  File "doc\guide\es\images\figura18.png"
  File "doc\guide\es\images\figura19.png"
  File "doc\guide\es\images\figura1.png"
  File "doc\guide\es\images\figura3.png"
  File "doc\guide\es\images\figura4.png"
  File "doc\guide\es\images\figura5.png"
  File "doc\guide\es\images\figura6.png"
  File "doc\guide\es\images\figura7.png"
  File "doc\guide\es\images\figura8.png"
  File "doc\guide\es\images\figura9.png"
  File "doc\guide\es\images\generaltrackoptions.gif"
  File "doc\guide\es\images\jobmanager.gif"
  File "doc\guide\es\images\movietitle.gif"
  File "doc\guide\es\images\muxingwindow.gif"
  File "doc\guide\es\images\selectmkvmergeexecutable.gif"
  File "doc\guide\es\images\splitting.gif"
  File "doc\guide\es\images\textsubtitlestrackoptions.gif"
  File "doc\guide\es\images\trackselection.gif"
  File "doc\guide\es\images\videotrackoptions.gif"
  File "doc\guide\es\images\figura10.png"
  SetOutPath "$INSTDIR\doc\guide\eu"
  File "doc\guide\eu\mkvmerge-gui.hhc"
  File "doc\guide\eu\mkvmerge-gui.hhk"
  File "doc\guide\eu\mkvmerge-gui.hhp"
  File "doc\guide\eu\mkvmerge-gui.html"
  SetOutPath "$INSTDIR\doc\guide\eu\images"
  File "doc\guide\eu\images\addingremovingattachments.gif"
  File "doc\guide\eu\images\addremovefiles.gif"
  File "doc\guide\eu\images\attachmentoptions.gif"
  File "doc\guide\eu\images\audiotrackoptions.gif"
  File "doc\guide\eu\images\chaptereditor.gif"
  File "doc\guide\eu\images\generaltrackoptions.gif"
  File "doc\guide\eu\images\headereditor.gif"
  File "doc\guide\eu\images\jobmanager.gif"
  File "doc\guide\eu\images\movietitle.gif"
  File "doc\guide\eu\images\muxingwindow.gif"
  File "doc\guide\eu\images\selectmkvmergeexecutable.gif"
  File "doc\guide\eu\images\splitting.gif"
  File "doc\guide\eu\images\textsubtitlestrackoptions.gif"
  File "doc\guide\eu\images\trackselection.gif"
  File "doc\guide\eu\images\videotrackoptions.gif"
  SetOutPath "$INSTDIR\doc\guide\nl"
  File "doc\guide\nl\mkvmerge-gui.hhc"
  File "doc\guide\nl\mkvmerge-gui.hhk"
  File "doc\guide\nl\mkvmerge-gui.hhp"
  File "doc\guide\nl\mkvmerge-gui.html"
  SetOutPath "$INSTDIR\doc\guide\nl\images"
	File "doc\guide\nl\images\addingremovingattachments.png"
	File "doc\guide\nl\images\addremovefiles.png"
	File "doc\guide\nl\images\attachmentoptions.png"
	File "doc\guide\nl\images\audiotrackoptions.png"
	File "doc\guide\nl\images\chaptereditor.png"
	File "doc\guide\nl\images\extraoptions.png"
	File "doc\guide\nl\images\generaltrackoptions.png"
	File "doc\guide\nl\images\jobmanager.png"
	File "doc\guide\nl\images\jobmanager_done.png"
	File "doc\guide\nl\images\jobmanagerprogress.png"
	File "doc\guide\nl\images\movietitle.png"
	File "doc\guide\nl\images\muxingwindow.png"
	File "doc\guide\nl\images\selectmkvmergeexecutable.png"
	File "doc\guide\nl\images\splitting.png"
	File "doc\guide\nl\images\textsubtitlestrackoptions.png"
	File "doc\guide\nl\images\trackselection.png"
	File "doc\guide\nl\images\videotrackoptions.png"
  SetOutPath "$INSTDIR\doc\guide\zh_CN"
  File "doc\guide\zh_CN\mkvmerge-gui.hhc"
  File "doc\guide\zh_CN\mkvmerge-gui.hhk"
  File "doc\guide\zh_CN\mkvmerge-gui.hhp"
  File "doc\guide\zh_CN\mkvmerge-gui.html"
  SetOutPath "$INSTDIR\doc\guide\zh_CN\images"
  File "doc\guide\zh_CN\images\addingremovingattachments.gif"
  File "doc\guide\zh_CN\images\addremovefiles.gif"
  File "doc\guide\zh_CN\images\attachmentoptions.gif"
  File "doc\guide\zh_CN\images\audiotrackoptions.gif"
  File "doc\guide\zh_CN\images\chaptereditor.gif"
  File "doc\guide\zh_CN\images\generaltrackoptions.gif"
  File "doc\guide\zh_CN\images\jobmanager.gif"
  File "doc\guide\zh_CN\images\movietitle.gif"
  File "doc\guide\zh_CN\images\muxingwindow.gif"
  File "doc\guide\zh_CN\images\selectmkvmergeexecutable.gif"
  File "doc\guide\zh_CN\images\splitting.gif"
  File "doc\guide\zh_CN\images\textsubtitlestrackoptions.gif"
  File "doc\guide\zh_CN\images\trackselection.gif"
  File "doc\guide\zh_CN\images\videotrackoptions.gif"
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
  SetOutPath "$INSTDIR\locale\cs\LC_MESSAGES"
  File "/oname=mkvtoolnix.mo" "po\cs.mo"
  File "/oname=wxstd.mo" "wxWidgets-po\cs\LC_MESSAGES\wxstd.mo"
  SetOutPath "$INSTDIR\locale\de\LC_MESSAGES"
  File "/oname=mkvtoolnix.mo" "po\de.mo"
  File "/oname=wxstd.mo" "wxWidgets-po\de\LC_MESSAGES\wxstd.mo"
  SetOutPath "$INSTDIR\locale\es\LC_MESSAGES"
  File "/oname=mkvtoolnix.mo" "po\es.mo"
  File "/oname=wxstd.mo" "wxWidgets-po\es\LC_MESSAGES\wxstd.mo"
  SetOutPath "$INSTDIR\locale\eu\LC_MESSAGES"
  File "/oname=mkvtoolnix.mo" "po\eu.mo"
  File "/oname=wxstd.mo" "wxWidgets-po\eu\LC_MESSAGES\wxstd.mo"
  SetOutPath "$INSTDIR\locale\fr\LC_MESSAGES"
  File "/oname=mkvtoolnix.mo" "po\fr.mo"
  File "/oname=wxstd.mo" "wxWidgets-po\fr\LC_MESSAGES\wxstd.mo"
  SetOutPath "$INSTDIR\locale\it\LC_MESSAGES"
  File "/oname=mkvtoolnix.mo" "po\it.mo"
  File "/oname=wxstd.mo" "wxWidgets-po\it\LC_MESSAGES\wxstd.mo"
  File "/oname=wxmsw.mo" "wxWidgets-po\it\LC_MESSAGES\wxmsw.mo"
  SetOutPath "$INSTDIR\locale\ja\LC_MESSAGES"
  File "/oname=mkvtoolnix.mo" "po\ja.mo"
  File "/oname=wxstd.mo" "wxWidgets-po\ja\LC_MESSAGES\wxstd.mo"
  SetOutPath "$INSTDIR\locale\lt\LC_MESSAGES"
  File "/oname=mkvtoolnix.mo" "po\lt.mo"
  # File "/oname=wxstd.mo" "wxWidgets-po\lt\LC_MESSAGES\wxstd.mo"
  SetOutPath "$INSTDIR\locale\nl\LC_MESSAGES"
  File "/oname=mkvtoolnix.mo" "po\nl.mo"
  File "/oname=wxstd.mo" "wxWidgets-po\nl\LC_MESSAGES\wxstd.mo"
  SetOutPath "$INSTDIR\locale\pl\LC_MESSAGES"
  File "/oname=mkvtoolnix.mo" "po\pl.mo"
  File "/oname=wxstd.mo" "wxWidgets-po\pl\LC_MESSAGES\wxstd.mo"
  SetOutPath "$INSTDIR\locale\pt\LC_MESSAGES"
  File "/oname=mkvtoolnix.mo" "po\pt.mo"
  # File "/oname=wxstd.mo" "wxWidgets-po\pt\LC_MESSAGES\wxstd.mo"
  SetOutPath "$INSTDIR\locale\ru\LC_MESSAGES"
  File "/oname=mkvtoolnix.mo" "po\ru.mo"
  File "/oname=wxstd.mo" "wxWidgets-po\ru\LC_MESSAGES\wxstd.mo"
  SetOutPath "$INSTDIR\locale\tr\LC_MESSAGES"
  File "/oname=mkvtoolnix.mo" "po\tr.mo"
  File "/oname=wxstd.mo" "wxWidgets-po\tr\LC_MESSAGES\wxstd.mo"
  SetOutPath "$INSTDIR\locale\uk\LC_MESSAGES"
  File "/oname=mkvtoolnix.mo" "po\uk.mo"
  File "/oname=wxstd.mo" "wxWidgets-po\uk\LC_MESSAGES\wxstd.mo"
  SetOutPath "$INSTDIR\locale\zh_CN\LC_MESSAGES"
  File "/oname=mkvtoolnix.mo" "po\zh_CN.mo"
  File "/oname=wxstd.mo" "wxWidgets-po\zh_CN\LC_MESSAGES\wxstd.mo"
  SetOutPath "$INSTDIR\locale\zh_TW\LC_MESSAGES"
  File "/oname=mkvtoolnix.mo" "po\zh_TW.mo"
  File "/oname=wxstd.mo" "wxWidgets-po\zh_TW\LC_MESSAGES\wxstd.mo"

  # Delete files that might be present from older installation
  # if this is just an upgrade.
  Delete "$INSTDIR\base64tool.exe"
  Delete "$INSTDIR\cygz.dll"
  Delete "$INSTDIR\data\magic"
  Delete "$INSTDIR\doc\base64tool.html"
  Delete "$INSTDIR\libcharset.dll"
  Delete "$INSTDIR\libcharset.dll"
  Delete "$INSTDIR\libcurl-4.dll"
  Delete "$INSTDIR\libebml.dll"
  Delete "$INSTDIR\libiconv-2.dll"
  Delete "$INSTDIR\libiconv.dll"
  Delete "$INSTDIR\libintl-8.dll"
  Delete "$INSTDIR\libmatroska.dll"
  Delete "$INSTDIR\magic1.dll"
  Delete "$INSTDIR\matroskalogo_big.ico"
  Delete "$INSTDIR\mingwm10.dll"
  Delete "$INSTDIR\regex2.dll"
  Delete "$INSTDIR\wxbase28u_gcc_custom.dll"
  Delete "$INSTDIR\wxmsw28u_adv_gcc_custom.dll"
  Delete "$INSTDIR\wxmsw28u_core_gcc_custom.dll"
  Delete "$INSTDIR\wxmsw28u_html_gcc_custom.dll"
  Delete "$INSTDIR\zlib1.dll"

  Delete "$INSTDIR\locale\german\LC_MESSAGES\mkvtoolnix.mo"
  RMDir "$INSTDIR\locale\german\LC_MESSAGES"
  RMDir "$INSTDIR\locale\german"

  Delete "$INSTDIR\locale\zh\LC_MESSAGES\mkvtoolnix.mo"
  RMDir "$INSTDIR\locale\zh\LC_MESSAGES"
  RMDir "$INSTDIR\locale\zh"

  # The docs have been moved to locale specific subfolders.
  Delete "$INSTDIR\doc\mkvextract.html"
  Delete "$INSTDIR\doc\mkvinfo.html"
  Delete "$INSTDIR\doc\mkvmerge.html"
  Delete "$INSTDIR\doc\mkvpropedit.html"
  Delete "$INSTDIR\doc\mmg.html"

  Delete "$INSTDIR\doc\mkvmerge-gui.*"
  Delete "$INSTDIR\doc\images\*.gif"
  RMDir "$INSTDIR\doc\images"

  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\App Paths\AppMainExe.exe"
  Delete "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\base64tool CLI reference.lnk"
  SetShellVarContext current
  Delete "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\base64tool CLI reference.lnk"
  SetShellVarContext all

  # Shortcuts
  !insertmacro MUI_STARTMENU_WRITE_BEGIN Application
  Delete "$SMPROGRAMS\$ICONS_GROUP\Documentation\mkvmerge GUI guide.lnk"

  CreateDirectory "$SMPROGRAMS\$ICONS_GROUP"
  SetOutPath "$INSTDIR"
  CreateShortCut "$SMPROGRAMS\$ICONS_GROUP\mkvmerge GUI.lnk" "$INSTDIR\mmg.exe" "" "$INSTDIR\mmg.exe"
  CreateShortCut "$SMPROGRAMS\$ICONS_GROUP\mkvinfo GUI.lnk" "$INSTDIR\mkvinfo.exe" "-g" "$INSTDIR\mkvinfo.exe"
  SetOutPath "$INSTDIR\Doc"
  CreateDirectory "$SMPROGRAMS\$ICONS_GROUP\Documentation"
  CreateDirectory "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference"
  CreateShortCut "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\mkvmerge CLI reference.lnk" "$INSTDIR\doc\en\mkvmerge.html"
  CreateShortCut "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\mkvinfo CLI reference.lnk" "$INSTDIR\doc\en\mkvinfo.html"
  CreateShortCut "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\mkvextract CLI reference.lnk" "$INSTDIR\doc\en\mkvextract.html"
  CreateShortCut "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\mkvpropedit CLI reference.lnk" "$INSTDIR\doc\en\mkvpropedit.html"
  CreateDirectory "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\Chinese Simplified"
  CreateShortCut "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\Chinese Simplified\mkvmerge CLI reference.lnk" "$INSTDIR\doc\zh_CN\mkvmerge.html"
  CreateShortCut "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\Chinese Simplified\mkvinfo CLI reference.lnk" "$INSTDIR\doc\zh_CN\mkvinfo.html"
  CreateShortCut "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\Chinese Simplified\mkvextract CLI reference.lnk" "$INSTDIR\doc\zh_CN\mkvextract.html"
  CreateShortCut "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\Chinese Simplified\mkvpropedit CLI reference.lnk" "$INSTDIR\doc\zh_CN\mkvpropedit.html"
  CreateDirectory "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\Dutch"
  CreateShortCut "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\Dutch\mkvmerge CLI reference.lnk" "$INSTDIR\doc\nl\mkvmerge.html"
  CreateShortCut "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\Dutch\mkvinfo CLI reference.lnk" "$INSTDIR\doc\nl\mkvinfo.html"
  CreateShortCut "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\Dutch\mkvextract CLI reference.lnk" "$INSTDIR\doc\nl\mkvextract.html"
  CreateShortCut "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\Dutch\mkvpropedit CLI reference.lnk" "$INSTDIR\doc\nl\mkvpropedit.html"
  CreateDirectory "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\German"
  CreateShortCut "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\German\mkvmerge CLI reference.lnk" "$INSTDIR\doc\de\mkvmerge.html"
  CreateShortCut "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\German\mkvinfo CLI reference.lnk" "$INSTDIR\doc\de\mkvinfo.html"
  CreateShortCut "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\German\mkvextract CLI reference.lnk" "$INSTDIR\doc\de\mkvextract.html"
  CreateShortCut "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\German\mkvpropedit CLI reference.lnk" "$INSTDIR\doc\de\mkvpropedit.html"
  CreateDirectory "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\Japanese"
  CreateShortCut "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\Japanese\mkvmerge CLI reference.lnk" "$INSTDIR\doc\ja\mkvmerge.html"
  CreateShortCut "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\Japanese\mkvinfo CLI reference.lnk" "$INSTDIR\doc\ja\mkvinfo.html"
  CreateShortCut "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\Japanese\mkvextract CLI reference.lnk" "$INSTDIR\doc\ja\mkvextract.html"
  CreateShortCut "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\Japanese\mkvpropedit CLI reference.lnk" "$INSTDIR\doc\ja\mkvpropedit.html"
  CreateDirectory "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\Ukrainian"
  CreateShortCut "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\Ukrainian\mkvmerge CLI reference.lnk" "$INSTDIR\doc\uk\mkvmerge.html"
  CreateShortCut "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\Ukrainian\mkvinfo CLI reference.lnk" "$INSTDIR\doc\uk\mkvinfo.html"
  CreateShortCut "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\Ukrainian\mkvextract CLI reference.lnk" "$INSTDIR\doc\uk\mkvextract.html"
  CreateShortCut "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\Ukrainian\mkvpropedit CLI reference.lnk" "$INSTDIR\doc\uk\mkvpropedit.html"
  CreateDirectory "$SMPROGRAMS\$ICONS_GROUP\Documentation\mkvmerge GUI guide"
  CreateShortCut "$SMPROGRAMS\$ICONS_GROUP\Documentation\mkvmerge GUI guide\mkvmerge GUI guide.lnk" "$INSTDIR\doc\guide\en\mkvmerge-gui.html"
  CreateDirectory "$SMPROGRAMS\$ICONS_GROUP\Documentation\mkvmerge GUI guide\Basque"
  CreateShortCut "$SMPROGRAMS\$ICONS_GROUP\Documentation\mkvmerge GUI guide\Basque\mkvmerge GUI guide.lnk" "$INSTDIR\doc\guide\eu\mkvmerge-gui.html"
  CreateDirectory "$SMPROGRAMS\$ICONS_GROUP\Documentation\mkvmerge GUI guide\Chinese Simplified"
  CreateShortCut "$SMPROGRAMS\$ICONS_GROUP\Documentation\mkvmerge GUI guide\Chinese Simplified\mkvmerge GUI guide.lnk" "$INSTDIR\doc\guide\zh_CN\mkvmerge-gui.html"
  CreateDirectory "$SMPROGRAMS\$ICONS_GROUP\Documentation\mkvmerge GUI guide\Dutch"
  CreateShortCut "$SMPROGRAMS\$ICONS_GROUP\Documentation\mkvmerge GUI guide\Dutch\mkvmerge GUI guide.lnk" "$INSTDIR\doc\guide\nl\mkvmerge-gui.html"
  CreateDirectory "$SMPROGRAMS\$ICONS_GROUP\Documentation\mkvmerge GUI guide\Spanish"
  CreateShortCut "$SMPROGRAMS\$ICONS_GROUP\Documentation\mkvmerge GUI guide\Spanish\mkvmerge GUI guide.lnk" "$INSTDIR\doc\guide\es\mkvmerge-gui.html"
  CreateDirectory "$SMPROGRAMS\$ICONS_GROUP\Documentation\Other documentation"
  CreateShortCut "$SMPROGRAMS\$ICONS_GROUP\Documentation\Other documentation\ChangeLog - What is new.lnk" "$INSTDIR\doc\ChangeLog.txt"
  CreateShortCut "$SMPROGRAMS\$ICONS_GROUP\Documentation\Other documentation\README.lnk" "$INSTDIR\doc\README.txt"
  CreateShortCut "$SMPROGRAMS\$ICONS_GROUP\Documentation\Other documentation\The GNU GPL.lnk" "$INSTDIR\doc\Copying.txt"
  !insertmacro MUI_STARTMENU_WRITE_END

  SetOutPath "$INSTDIR"
  IfSilent +3 0
  MessageBox MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2 "$(STRING_SHORTCUT_ON_DESKTOP)" IDNO +2
  CreateShortCut "$DESKTOP\mkvmerge GUI.lnk" "$INSTDIR\mmg.exe" "" "$INSTDIR\mmg.exe"
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
SectionEnd

Function showExternalLinks
  Push $R0
  InstallOptions::dialog $PLUGINSDIR\external_links.ini
  Pop $R0
FunctionEnd

var unRemoveJobs

Function un.onUninstSuccess
  HideWindow
  MessageBox MB_ICONINFORMATION|MB_OK "$(STRING_UNINSTALLED_OK)"
FunctionEnd

Function un.onInit
  !insertmacro MUI_UNGETLANGUAGE
  MessageBox MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2 "$(STRING_REMOVE_PROGRAM_QUESTION)" IDYES +2
  Abort
  StrCpy $unRemoveJobs "No"
  IfFileExists "$INSTDIR\jobs\*.*" +2
  Return
  MessageBox MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2 "$(STRING_REMOVE_JOB_FILES_QUESTION)" IDYES +2
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
  Delete "$SMPROGRAMS\$ICONS_GROUP\Documentation\mkvmerge GUI guide\mkvmerge GUI guide.lnk"
  Delete "$SMPROGRAMS\$ICONS_GROUP\Documentation\mkvmerge GUI guide\Basque\mkvmerge GUI guide.lnk"
  Delete "$SMPROGRAMS\$ICONS_GROUP\Documentation\mkvmerge GUI guide\Chinese Simplified\mkvmerge GUI guide.lnk"
  Delete "$SMPROGRAMS\$ICONS_GROUP\Documentation\mkvmerge GUI guide\Dutch\mkvmerge GUI guide.lnk"
  Delete "$SMPROGRAMS\$ICONS_GROUP\Documentation\mkvmerge GUI guide\Spanish\mkvmerge GUI guide.lnk"

  Delete "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\Chinese Simplified\mkvpropedit CLI reference.lnk"
  Delete "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\Chinese Simplified\mkvmerge CLI reference.lnk"
  Delete "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\Chinese Simplified\mkvinfo CLI reference.lnk"
  Delete "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\Chinese Simplified\mkvextract CLI reference.lnk"
  Delete "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\Dutch\mkvpropedit CLI reference.lnk"
  Delete "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\Dutch\mkvmerge CLI reference.lnk"
  Delete "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\Dutch\mkvinfo CLI reference.lnk"
  Delete "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\Dutch\mkvextract CLI reference.lnk"
  Delete "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\German\mkvpropedit CLI reference.lnk"
  Delete "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\German\mkvmerge CLI reference.lnk"
  Delete "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\German\mkvinfo CLI reference.lnk"
  Delete "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\German\mkvextract CLI reference.lnk"
  Delete "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\Japanese\mkvpropedit CLI reference.lnk"
  Delete "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\Japanese\mkvmerge CLI reference.lnk"
  Delete "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\Japanese\mkvinfo CLI reference.lnk"
  Delete "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\Japanese\mkvextract CLI reference.lnk"
  Delete "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\Ukrainian\mkvpropedit CLI reference.lnk"
  Delete "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\Ukrainian\mkvmerge CLI reference.lnk"
  Delete "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\Ukrainian\mkvinfo CLI reference.lnk"
  Delete "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\Ukrainian\mkvextract CLI reference.lnk"
  Delete "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\mkvpropedit CLI reference.lnk"
  Delete "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\mkvmerge CLI reference.lnk"
  Delete "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\mkvinfo CLI reference.lnk"
  Delete "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\mkvextract CLI reference.lnk"
  Delete "$SMPROGRAMS\$ICONS_GROUP\Documentation\Other documentation\ChangeLog - What is new.lnk"
  Delete "$SMPROGRAMS\$ICONS_GROUP\Documentation\Other documentation\README.lnk"
  Delete "$SMPROGRAMS\$ICONS_GROUP\Documentation\Other documentation\The GNU GPL.lnk"

  RMDir "$SMPROGRAMS\$ICONS_GROUP\Documentation\mkvmerge GUI guide\Basque"
  RMDir "$SMPROGRAMS\$ICONS_GROUP\Documentation\mkvmerge GUI guide\Chinese Simplified"
  RMDir "$SMPROGRAMS\$ICONS_GROUP\Documentation\mkvmerge GUI guide\Dutch"
  RMDir "$SMPROGRAMS\$ICONS_GROUP\Documentation\mkvmerge GUI guide\Spanish"
  RMDir "$SMPROGRAMS\$ICONS_GROUP\Documentation\mkvmerge GUI guide"
  RMDir "$SMPROGRAMS\$ICONS_GROUP\Documentation\Other documentation"
  RMDir "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\Chinese Simplified"
  RMDir "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\Dutch"
  RMDir "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\German"
  RMDir "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\Japanese"
  RMDir "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference\Ukrainian"
  RMDir "$SMPROGRAMS\$ICONS_GROUP\Documentation\Command line reference"
  RMDir "$SMPROGRAMS\$ICONS_GROUP\Documentation"
  RMDir "$SMPROGRAMS\$ICONS_GROUP"

  Delete "$DESKTOP\mkvmerge GUI.lnk"

  Delete "$INSTDIR\${PRODUCT_NAME}.url"
  Delete "$INSTDIR\uninst.exe"
  Delete "$INSTDIR\mkvextract.exe"
  Delete "$INSTDIR\mkvinfo.exe"
  Delete "$INSTDIR\mkvmerge.exe"
  Delete "$INSTDIR\mkvpropedit.exe"
  Delete "$INSTDIR\mmg.exe"

  Delete "$INSTDIR\data\magic.mgc"

  Delete "$INSTDIR\doc\en\*.*"
  Delete "$INSTDIR\doc\de\*.*"
  Delete "$INSTDIR\doc\ja\*.*"
  Delete "$INSTDIR\doc\nl\*.*"
  Delete "$INSTDIR\doc\uk\*.*"
  Delete "$INSTDIR\doc\zh_CN\*.*"

  RMDir "$INSTDIR\doc\en"
  RMDir "$INSTDIR\doc\de"
  RMDir "$INSTDIR\doc\ja"
  RMDir "$INSTDIR\doc\nl"
  RMDir "$INSTDIR\doc\uk"
  RMDir "$INSTDIR\doc\zh_CN"

  Delete "$INSTDIR\doc\guide\en\images\*.gif"
  Delete "$INSTDIR\doc\guide\en\mkvmerge-gui*.*"
  RMDir "$INSTDIR\doc\guide\en\images"
  RMDir "$INSTDIR\doc\guide\en"
  Delete "$INSTDIR\doc\guide\es\images\*.gif"
  Delete "$INSTDIR\doc\guide\es\images\*.png"
  Delete "$INSTDIR\doc\guide\es\mkvmerge-gui*.*"
  RMDir "$INSTDIR\doc\guide\es\images"
  RMDir "$INSTDIR\doc\guide\es"
  Delete "$INSTDIR\doc\guide\eu\images\*.gif"
  Delete "$INSTDIR\doc\guide\eu\images\*.png"
  Delete "$INSTDIR\doc\guide\eu\mkvmerge-gui*.*"
  RMDir "$INSTDIR\doc\guide\eu\images"
  RMDir "$INSTDIR\doc\guide\eu"
  Delete "$INSTDIR\doc\guide\nl\images\*.png"
  Delete "$INSTDIR\doc\guide\nl\mkvmerge-gui*.*"
  RMDir "$INSTDIR\doc\guide\nl\images"
  RMDir "$INSTDIR\doc\guide\nl"
  Delete "$INSTDIR\doc\guide\zh_CN\images\*.gif"
  Delete "$INSTDIR\doc\guide\zh_CN\mkvmerge-gui*.*"
  RMDir "$INSTDIR\doc\guide\zh_CN\images"
  RMDir "$INSTDIR\doc\guide\zh_CN"

  RMDir "$INSTDIR\doc\guide"

  Delete "$INSTDIR\doc\*.*"
  RMDir "$INSTDIR\doc"

  Delete "$INSTDIR\examples\*.*"
  RMDir "$INSTDIR\examples"

  Delete "$INSTDIR\locale\cs\LC_MESSAGES\*.*"
  RMDir "$INSTDIR\locale\cs\LC_MESSAGES"
  RMDir "$INSTDIR\locale\cs"

  Delete "$INSTDIR\locale\de\LC_MESSAGES\*.*"
  RMDir "$INSTDIR\locale\de\LC_MESSAGES"
  RMDir "$INSTDIR\locale\de"

  Delete "$INSTDIR\locale\es\LC_MESSAGES\*.*"
  RMDir "$INSTDIR\locale\es\LC_MESSAGES"
  RMDir "$INSTDIR\locale\es"

  Delete "$INSTDIR\locale\eu\LC_MESSAGES\*.*"
  RMDir "$INSTDIR\locale\eu\LC_MESSAGES"
  RMDir "$INSTDIR\locale\eu"

  Delete "$INSTDIR\locale\fr\LC_MESSAGES\*.*"
  RMDir "$INSTDIR\locale\fr\LC_MESSAGES"
  RMDir "$INSTDIR\locale\fr"

  Delete "$INSTDIR\locale\it\LC_MESSAGES\*.*"
  RMDir "$INSTDIR\locale\it\LC_MESSAGES"
  RMDir "$INSTDIR\locale\it"

  Delete "$INSTDIR\locale\ja\LC_MESSAGES\*.*"
  RMDir "$INSTDIR\locale\ja\LC_MESSAGES"
  RMDir "$INSTDIR\locale\ja"

  Delete "$INSTDIR\locale\lt\LC_MESSAGES\*.*"
  RMDir "$INSTDIR\locale\lt\LC_MESSAGES"
  RMDir "$INSTDIR\locale\lt"

  Delete "$INSTDIR\locale\nl\LC_MESSAGES\*.*"
  RMDir "$INSTDIR\locale\nl\LC_MESSAGES"
  RMDir "$INSTDIR\locale\nl"

  Delete "$INSTDIR\locale\pl\LC_MESSAGES\*.*"
  RMDir "$INSTDIR\locale\pl\LC_MESSAGES"
  RMDir "$INSTDIR\locale\pl"

  Delete "$INSTDIR\locale\pt\LC_MESSAGES\*.*"
  RMDir "$INSTDIR\locale\pt\LC_MESSAGES"
  RMDir "$INSTDIR\locale\pt"

  Delete "$INSTDIR\locale\ru\LC_MESSAGES\*.*"
  RMDir "$INSTDIR\locale\ru\LC_MESSAGES"
  RMDir "$INSTDIR\locale\ru"

  Delete "$INSTDIR\locale\tr\LC_MESSAGES\*.*"
  RMDir "$INSTDIR\locale\tr\LC_MESSAGES"
  RMDir "$INSTDIR\locale\tr"

  Delete "$INSTDIR\locale\uk\LC_MESSAGES\*.*"
  RMDir "$INSTDIR\locale\uk\LC_MESSAGES"
  RMDir "$INSTDIR\locale\uk"

  Delete "$INSTDIR\locale\zh_CN\LC_MESSAGES\*.*"
  RMDir "$INSTDIR\locale\zh_CN\LC_MESSAGES"
  RMDir "$INSTDIR\locale\zh_CN"

  Delete "$INSTDIR\locale\zh_TW\LC_MESSAGES\*.*"
  RMDir "$INSTDIR\locale\zh_TW\LC_MESSAGES"
  RMDir "$INSTDIR\locale\zh_TW"

  # From previous versions of mkvtoolnix: translation to Simplified Chinese
  Delete "$INSTDIR\locale\zh\LC_MESSAGES\*.*"
  RMDir "$INSTDIR\locale\zh\LC_MESSAGES"
  RMDir "$INSTDIR\locale\zh"

  RMDir "$INSTDIR\locale"
  RMDir "$INSTDIR\data"

  StrCmp $unRemoveJobs "Yes" 0 +8
  Delete "$INSTDIR\jobs\*.mmg"
  RMDir "$INSTDIR\jobs"
  SetShellVarContext current
  Delete "$APPDATA\mkvtoolnix\jobs\*.mmg"
  RMDir "$APPDATA\mkvtoolnix\jobs"
  SetShellVarContext all
  Delete "$APPDATA\mkvtoolnix\jobs\*.mmg"
  RMDir "$APPDATA\mkvtoolnix\jobs"

  RMDir "$INSTDIR"
  SetShellVarContext current
  RMDir "$APPDATA\mkvtoolnix"
  SetShellVarContext all
  RMDir "$APPDATA\mkvtoolnix"

  DeleteRegKey ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}"
  DeleteRegKey HKLM "${PRODUCT_DIR_REGKEY}"
  DeleteRegKey HKLM "${MTX_REGKEY}"
  DeleteRegKey HKCU "${MTX_REGKEY}"

  SetAutoClose true
SectionEnd

# Microsoft Developer Studio Generated NMAKE File, Based on mkvmerge.dsp
!IF "$(CFG)" == ""
CFG=mkvmerge_Debug
!MESSAGE No configuration specified. Defaulting to mkvmerge_Debug.
!ENDIF 

!IF "$(CFG)" != "mkvmerge_Release" && "$(CFG)" != "mkvmerge_Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "mkvmerge.mak" CFG="mkvmerge_Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "mkvmerge_Release" (based on "Win32 (x86) Console Application")
!MESSAGE "mkvmerge_Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

# ONLY edit these include and lib paths
AVILIB_INCPATH=".\avilib"
AVILIB_LIBPATH=".\avilib"
MATROSKA_INCPATH="..\..\matroska\libmatroska\src"
MATROSKA_LIBPATH="..\..\matroska\libmatroska\lib"
# end of edits

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "mkvmerge_Release"

OUTDIR=.
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

ALL : "$(OUTDIR)\mkvmerge.exe"


CLEAN :
	-@erase "$(INTDIR)\ac3_common.obj"
	-@erase "$(INTDIR)\common.obj"
	-@erase "$(INTDIR)\mkvmerge.obj"
	-@erase "$(INTDIR)\mp3_common.obj"
	-@erase "$(INTDIR)\p_ac3.obj"
	-@erase "$(INTDIR)\p_mp3.obj"
	-@erase "$(INTDIR)\p_pcm.obj"
	-@erase "$(INTDIR)\p_textsubs.obj"
	-@erase "$(INTDIR)\p_video.obj"
	-@erase "$(INTDIR)\pr_generic.obj"
	-@erase "$(INTDIR)\queue.obj"
	-@erase "$(INTDIR)\r_ac3.obj"
	-@erase "$(INTDIR)\r_avi.obj"
	-@erase "$(INTDIR)\r_mp3.obj"
	-@erase "$(INTDIR)\r_ogm.obj"
	-@erase "$(INTDIR)\r_srt.obj"
	-@erase "$(INTDIR)\r_wav.obj"
	-@erase "$(INTDIR)\subtitles.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\mkvmerge.exe"

"$(INTDIR)" :
    if not exist "$(INTDIR)/$(NULL)" mkdir "$(INTDIR)"
    
"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /ML /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /Fp"$(INTDIR)\mkvmerge.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\mkvmerge.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib  kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /incremental:no /pdb:"$(OUTDIR)\mkvmerge.pdb" /machine:I386 /out:"$(OUTDIR)\mkvmerge.exe" 
LINK32_OBJS= \
	"$(INTDIR)\mp3_common.obj" \
	"$(INTDIR)\common.obj" \
	"$(INTDIR)\mkvmerge.obj" \
	"$(INTDIR)\ac3_common.obj" \
	"$(INTDIR)\pr_generic.obj" \
	"$(INTDIR)\p_mp3.obj" \
	"$(INTDIR)\p_pcm.obj" \
	"$(INTDIR)\p_textsubs.obj" \
	"$(INTDIR)\p_video.obj" \
	"$(INTDIR)\p_ac3.obj" \
	"$(INTDIR)\subtitles.obj" \
	"$(INTDIR)\r_ac3.obj" \
	"$(INTDIR)\r_avi.obj" \
	"$(INTDIR)\r_mp3.obj" \
	"$(INTDIR)\r_ogm.obj" \
	"$(INTDIR)\r_srt.obj" \
	"$(INTDIR)\queue.obj" \
	"$(INTDIR)\r_wav.obj"

"$(OUTDIR)\mkvmerge.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "mkvmerge_Debug"

OUTDIR=.
INTDIR=.\Debug

ALL : ".\mkvmerge.exe"


CLEAN :
	-@erase "$(INTDIR)\ac3_common.obj"
	-@erase "$(INTDIR)\common.obj"
	-@erase "$(INTDIR)\mkvmerge.obj"
	-@erase "$(INTDIR)\mp3_common.obj"
	-@erase "$(INTDIR)\p_ac3.obj"
	-@erase "$(INTDIR)\p_mp3.obj"
	-@erase "$(INTDIR)\p_pcm.obj"
	-@erase "$(INTDIR)\p_textsubs.obj"
	-@erase "$(INTDIR)\p_video.obj"
	-@erase "$(INTDIR)\pr_generic.obj"
	-@erase "$(INTDIR)\queue.obj"
	-@erase "$(INTDIR)\r_ac3.obj"
	-@erase "$(INTDIR)\r_avi.obj"
	-@erase "$(INTDIR)\r_mp3.obj"
	-@erase "$(INTDIR)\r_ogm.obj"
	-@erase "$(INTDIR)\r_srt.obj"
	-@erase "$(INTDIR)\r_wav.obj"
	-@erase "$(INTDIR)\subtitles.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\mkvmerge.pdb"
	-@erase ".\mkvmerge.exe"
	-@erase ".\mkvmerge.ilk"

"$(INTDIR)" :
    if not exist "$(INTDIR)/$(NULL)" mkdir "$(INTDIR)"
    
"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MLd /W3 /Gm /GX /ZI /Od /I MATROSKA_INCPATH /I AVILIB_INCPATH /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /Fp"$(INTDIR)\mkvmerge.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ  /c 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\mkvmerge.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=avilib.lib matroska_lib_static_debug.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib  kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /incremental:yes /pdb:"$(OUTDIR)\mkvmerge.pdb" /debug /machine:I386 /nodefaultlib:"LIBCMTD" /out:"mkvmerge.exe" /pdbtype:sept /libpath:AVILIB_LIBPATH /libpath:MATROSKA_LIBPATH 
LINK32_OBJS= \
	"$(INTDIR)\mp3_common.obj" \
	"$(INTDIR)\common.obj" \
	"$(INTDIR)\mkvmerge.obj" \
	"$(INTDIR)\ac3_common.obj" \
	"$(INTDIR)\pr_generic.obj" \
	"$(INTDIR)\p_mp3.obj" \
	"$(INTDIR)\p_pcm.obj" \
	"$(INTDIR)\p_textsubs.obj" \
	"$(INTDIR)\p_video.obj" \
	"$(INTDIR)\p_ac3.obj" \
	"$(INTDIR)\subtitles.obj" \
	"$(INTDIR)\r_ac3.obj" \
	"$(INTDIR)\r_avi.obj" \
	"$(INTDIR)\r_mp3.obj" \
	"$(INTDIR)\r_ogm.obj" \
	"$(INTDIR)\r_srt.obj" \
	"$(INTDIR)\queue.obj" \
	"$(INTDIR)\r_wav.obj"

".\mkvmerge.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<


!IF "$(CFG)" == "mkvmerge_Release" || "$(CFG)" == "mkvmerge_Debug"
SOURCE=.\ac3_common.cpp

"$(INTDIR)\ac3_common.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\common.cpp

"$(INTDIR)\common.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\mkvmerge.cpp

"$(INTDIR)\mkvmerge.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\mp3_common.cpp

"$(INTDIR)\mp3_common.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\p_ac3.cpp

"$(INTDIR)\p_ac3.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\p_mp3.cpp

"$(INTDIR)\p_mp3.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\p_pcm.cpp

"$(INTDIR)\p_pcm.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\p_textsubs.cpp

"$(INTDIR)\p_textsubs.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\p_video.cpp

"$(INTDIR)\p_video.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\pr_generic.cpp

"$(INTDIR)\pr_generic.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\queue.cpp

"$(INTDIR)\queue.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\r_ac3.cpp

"$(INTDIR)\r_ac3.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\r_avi.cpp

"$(INTDIR)\r_avi.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\r_mp3.cpp

"$(INTDIR)\r_mp3.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\r_ogm.cpp

"$(INTDIR)\r_ogm.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\r_srt.cpp

"$(INTDIR)\r_srt.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\r_wav.cpp

"$(INTDIR)\r_wav.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\subtitles.cpp

"$(INTDIR)\subtitles.obj" : $(SOURCE) "$(INTDIR)"



!ENDIF 


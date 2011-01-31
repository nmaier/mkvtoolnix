# Microsoft Developer Studio Project File - Name="libebml" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=libebml - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "libebml.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "libebml.mak" CFG="libebml - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "libebml - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "libebml - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "libebml - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "../../../.." /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "WRITE_EVEN_UNSET_DATA" /YX /FD /c
# ADD BASE RSC /l 0x40c /d "NDEBUG"
# ADD RSC /l 0x809 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "libebml - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GR /GX /ZI /Od /I "../../../.." /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "WRITE_EVEN_UNSET_DATA" /YX /FD /GZ /c
# ADD BASE RSC /l 0x40c /d "_DEBUG"
# ADD RSC /l 0x809 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "libebml - Win32 Release"
# Name "libebml - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\..\..\src\Debug.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\EbmlBinary.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\EbmlContexts.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\EbmlCrc32.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\EbmlDate.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\EbmlDummy.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\EbmlElement.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\EbmlFloat.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\EbmlHead.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\EbmlMaster.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\EbmlSInteger.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\EbmlStream.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\EbmlString.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\EbmlSubHead.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\EbmlUInteger.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\EbmlUnicodeString.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\EbmlVersion.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\EbmlVoid.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\IOCallback.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\MemIOCallback.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\StdIOCallback.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\..\..\ebml\Debug.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\ebml\EbmlBinary.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\ebml\EbmlConfig.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\ebml\EbmlContexts.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\ebml\EbmlCrc32.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\ebml\EbmlDate.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\ebml\EbmlDummy.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\ebml\EbmlElement.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\ebml\EbmlEndian.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\ebml\EbmlFloat.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\ebml\EbmlHead.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\ebml\EbmlId.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\ebml\EbmlMaster.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\ebml\EbmlSInteger.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\ebml\EbmlStream.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\ebml\EbmlString.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\ebml\EbmlSubHead.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\ebml\EbmlTypes.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\ebml\EbmlUInteger.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\ebml\EbmlUnicodeString.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\ebml\EbmlVersion.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\ebml\EbmlVoid.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\ebml\IOCallback.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\ebml\MemIOCallback.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\ebml\StdIOCallback.h
# End Source File
# End Group
# End Target
# End Project

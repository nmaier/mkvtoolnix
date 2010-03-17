@echo off

copy /y config.windows.h config.h >NUL
echo This will build the dependencies for mkvtoolnix using Visual Studio 2008 (or newer, probably).
echo You can do these steps manually, just read this batch file.
echo If you don't have devenv.exe in your path, press CTRL-C now.
echo To set use (for example):
echo set PATH=%%PATH%%;C:\Program Files (x86)\Microsoft Visual Studio 9.0\Common7\IDE\
echo ----
echo After verifying the above, we're going to convert a project.
echo Visual Studio requires this to be done through the UI, so do the following:
echo Answer Yes to all for every conversion question.
echo Quit VS, and save when prompted.

pause

copy /y winbuild\bh.bat .. >NUL
copy /y winbuild\bh2.bat .. >NUL
cd ..
if exist mkvtoolnix-* move mkvtoolnix-* mkvtoolnix
if not exist mkvtoolnix goto badname

call bh2 boost
if not exist boost goto nolib
call bh expat
if not exist expat goto nolib
call bh libebml
if not exist libebml goto nolib
call bh libmatroska
if not exist libmatroska goto nolib
call bh libogg
if not exist libogg goto nolib
call bh libvorbis
if not exist libvorbis goto nolib
call bh zlib
if not exist zlib goto nolib


del bh.bat
del bh2.bat

md lib >NUL
md lib\debug >NUL
if not exist "expat\expat.sln" devenv expat\expat.dsw
copy /y mkvtoolnix\winbuild\zlib.* zlib\projects\visualc6\ >NUL

:build
echo --- Done manual conversion, ready to build.
echo There are currently quite a few warnings in these projects. It's safe to ignore any 64 bit and security warnings ("possible loss of data" and "May be unsafe")
pause
echo Building Expat
call devenv expat\expat.sln /build debug
call devenv expat\expat.sln /build Release
echo Building libebml
call devenv libebml\make\vc7\lib\libebml.v71.vcproj /upgrade
call devenv libebml\make\vc7\lib\libebml.v71.vcproj /build debug
call devenv libebml\make\vc7\lib\libebml.v71.vcproj /build release
echo Building libmatroska
call devenv libmatroska\make\vc7\lib\static\libmatroska.v71.vcproj /upgrade
call devenv libmatroska\make\vc7\lib\static\libmatroska.v71.vcproj /build debug
call devenv libmatroska\make\vc7\lib\static\libmatroska.v71.vcproj /build release
echo Building libogg_static
call devenv libogg\win32\VS2008\libogg_static.sln /build debug
call devenv libogg\win32\VS2008\libogg_static.sln /build Release_SSE2
echo Building vorbis_static
call devenv libvorbis\win32\VS2008\vorbis_static.sln /build debug
call devenv libvorbis\win32\VS2008\vorbis_static.sln /build Release_SSE2
echo Building zlib
call devenv zlib\projects\visualc6\zlib.sln /build "LIB debug"
call devenv zlib\projects\visualc6\zlib.sln /build "LIB Release"

cd boost
if not exist "bjam.exe" call bootstrap

echo Building boost
bjam --stagedir=.. --with-regex --with-filesystem runtime-link=static link=static

cd ..
move /y expat\win32\bin\Debug\* lib\debug\ >NUL
move /y libebml\make\vc7\lib\Debug\*.lib lib\debug\ >NUL
move /y libmatroska\make\vc7\lib\static\Debug\*.lib lib\debug\ >NUL
copy /y libogg\win32\VS2008\Win32\Debug\*.lib lib\debug\ >NUL
move /y libvorbis\win32\VS2008\Win32\Debug\* lib\debug\ >NUL
move /y libvorbis\win32\VS2008\Win32\Debug\* lib\debug\ >NUL
move /y zlib\projects\visualc6\Win32_LIB_Debug\*.lib lib\debug\ >NUL

move /y expat\win32\bin\Release\* lib\ >NUL
move /y libebml\make\vc7\lib\Release\*.lib lib\ >NUL
move /y libmatroska\make\vc7\lib\static\Release\*.lib lib\ >NUL
copy /y libogg\win32\VS2008\Win32\Release_SSE2\*.lib lib\ >NUL
move /y libvorbis\win32\VS2008\Win32\Release_SSE2\* lib\ >NUL
move /y libvorbis\win32\VS2008\Win32\Release\* lib\ >NUL
move /y zlib\projects\visualc6\Win32_LIB_Release\*.lib lib\ >NUL

move /y lib\libboost_*d.lib lib\debug\
move /y lib\libboost_*-sgd-*.lib lib\debug\

cd mkvtoolnix
mkdir Release >NUL
mkdir Debug>NUL
copy /y ..\lib\libExpat.dll Release\ >NUL
copy /y ..\lib\debug\libExpat.dll Debug\ >NUL
echo Building mkvtoolnix
devenv mkvtoolnix.sln /Build debug
devenv mkvtoolnix.sln /Build release
goto done

:nolib
echo aborting.
goto done

:badname
echo The build script doesn't know what directory mkvtoolnix is in. Please rename the folder to mkvtoolnix.
echo Please ensure that you're running this from the mkvtoolnix directory, as 
echo C:\...\mkvtoolnix>"winbuild\Build using VC8.bat"
goto done

:done

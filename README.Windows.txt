Building mkvtoolnix 3.2.0 on Windows
------------------------------------

Here are short instructions for building using Microsoft Visual
Studio.

1. Building third party libraries

Download the following libraries and place them in the same directory
which contains the mkvtoolnix source code directory. Please refer to
the mkvtoolnix website for versions and links.

boost
expat
libebml
libmatroska
libogg
libvorbis
zlib

2. Possible compiler errors

If you have VS 2008 (with no service pack), you may encounter compiler
error C2471. If so, please download the hotfix from Microsoft:
http://code.msdn.microsoft.com/KB946040

3. Prepare the source tree for building

From a command prompt with devenv.exe in the path, run the following
from your mkvtoolnix source code directory:

"winbuild\Build using VC8.bat"

If you do not have devenv.exe in the path, use this command from the
prompt before running "Build using VC8.bat":

set PATH=%PATH%;C:\program Files (x86)\Microsoft Visual Studio 9.0\Common7\IDE\

Adjust for different installation paths if neccessary.

4. Build mkvtoolnix.sln.

The author of mkvtoolnix is currently not maintaining this port. If
you, at some later date, need to recreate these project files, please
be aware that a number of files have the same file names, even within
a single project. If .obj files are clobbered by the compiler, you
will get linker errors. Also, zlib uses a dynamic C runtime.

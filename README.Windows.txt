Building mkvtoolnix 3.2.0 on Windows
====================================

There are currently two ways to build mkvtoolnix for Windows: building
it on Windows with Microsoft Visual Studio 8 (and maybe newer) or on
Linux with a mingw cross compiler. Section 1 describes building with
Visual Studio and section 2 describes the mingw cross compiler way.

Section 1 -- Building with Microsoft Visual Studio
--------------------------------------------------

1.1. Building third party libraries

Download the following libraries and place them in the same directory
which contains the mkvtoolnix source code directory. Please refer to
the mkvtoolnix website for versions and links.

boost
expat
libcurl
libebml
libmatroska
libogg
libvorbis
zlib

1.2. Possible compiler errors

If you have VS 2008 (with no service pack), you may encounter compiler
error C2471. If so, please download the hotfix from Microsoft:
http://code.msdn.microsoft.com/KB946040

1.3. Prepare the source tree for building

From a command prompt with devenv.exe in the path, run the following
from your mkvtoolnix source code directory:

"winbuild\Build using VC8.bat"

If you do not have devenv.exe in the path, use this command from the
prompt before running "Build using VC8.bat":

set PATH=%PATH%;C:\program Files (x86)\Microsoft Visual Studio 9.0\Common7\IDE\

Adjust for different installation paths if neccessary.

1.4. Build mkvtoolnix.sln.

The author of mkvtoolnix is currently not maintaining this port. If
you, at some later date, need to recreate these project files, please
be aware that a number of files have the same file names, even within
a single project. If .obj files are clobbered by the compiler, you
will get linker errors. Also, zlib uses a dynamic C runtime.



Section 2 -- Building with a mingw cross compiler
-------------------------------------------------

2.1. Preparations

You will need:

- a mingw cross compiler
- roughly 2 GB of free space available

Earlier versions of this document described in painful details how to
build each library. Luckily there's the "mingw-cross-env" project at
http://mingw-cross-env.nongnu.org/ that provides an easy-to-use way of
setting up everything we need.

First, download a release or the current development version; see
http://mingw-cross-env.nongnu.org/#latest-release to to retrieve
them. Then unpack the release.

The rest of this guide assumes that you've unpacked mingw-cross-env
into the directory $HOME/mingw-cross-env.

2.2. Automatic build script

mkvtoolnix contains a script that can download, compile and install
all required libraries into the directory $HOME/mingw-cross-env.

If the script does not work or you want to do everything yourself
you'll find instructions for manual compilation in section 2.3.

2.2.1. Script configuration

The script is called winbuild/setup_cross_compilation_env.sh. It
contains the following variables that can be adjusted to fit your
needs:

  TARGET=i686-pc-mingw32

The target specification for the standard configure scripts.

  INSTALL_DIR=$HOME/mingw-cross-env

Base installation directory

  PARALLEL=

Number of processes to execute in parallel. Will be set to the number
of cores available if left empty.

2.2.2. Execution

From the mkvtoolnix source directory run:

  ./winbuild/setup_cross_compilation_env.sh

If everything works fine you'll end up with a configured mkvtoolnix
source tree. You just have to run 'drake' afterwards.

2.3. Manual installation

First you will need the mingw-cross-env build scripts. Get them by
downloading them (see above) and unpacking them into
$HOME/mingw-cross-env.

Next, build the required libraries:

  cd $HOME/mingw-cross-env
  make gcc w32api mingwrt gettext boost bzip2 curl flac expat libiconv \
    ogg pthreads vorbis wxwidgets zlib

Append the installation directory to your PATH variable:

  export PATH=$PATH:$HOME/mingw-cross-env/usr/i686-pc-mingw32/bin

Finally, configure mkvtoolnix:

  cd $HOME/path/to/mkvtoolnix-source
  ./configure \
    --host=i686-pc-mingw32 \
    --with-boost=$HOME/mingw-cross-env/usr/i686-pc-mingw32 \
    --with-wx-config=$HOME/mingw-cross-env/usr/i686-pc-mingw32/bin/i686-pc-mingw32-wx-config

If everything works then build it:

  ./drake

You're done.

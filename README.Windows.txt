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

2.1.1. Prerequisites

You need:

- a mingw cross compiler
- roughly 2 GB of free space available
- the "bjam" build utility for the Boost library (see 2.1.3.)

You usually don't need root access unless you have to install mingw
packages (see 2.1.2.). The mkvtoolnix build process itself can be run
as any user.

2.1.2. Installing mingw itself

You need a mingw cross compiler with Unicode support enabled in its
Standard Template Library. If your Linux distribution comes with such
a compiler then you should use this. Otherwise you'll have to get or
compile one yourself. How to do the latter is beyond the scope of this
document.

Note that "Unicode enabled STL" means that you have to use mingw v4.0
or newer. The v3.x series does not contain support for wide
characters.

If you're using a Debian or Ubuntu based distribution then all you
need to do is install three packages:

  sudo apt-get install mingw32 mingw32-runtime mingw32-binutils

The rest of the document assumes that the mingw gcc executable is
called "i586-mingw32msvc-gcc" and can be found in the PATH.

2.1.3. Obtaining the "bjam" utility

Boost comes with its own build tool called "bjam". This binary must be
run on your build host, meaning that it must be a Linux executable and
not the "bjam.exe" for Windows.

Most Linux distributions come with the Boost development tools. For
Debian and Ubuntu based distributions "bjam" is located in its own
package that you can install easily:

  sudo apt-get install bjam

Other distributions might package it in packages named "boost-dev" or
"boost-devel" or similar.

2.2. Automatic build script

mkvtoolnix contains a script that can download, compile and install
all required libraries into the directory $HOME/mingw.

If the script does not work or you want to do everything yourself
you'll find instructions for manual compilation in section 2.3.

2.2.1. Script configuration

The script is called winbuild/setup_cross_compilation_env.sh. It
contains the following variables that can be adjusted to fit your
needs:

  BJAM=bjam

Path and name of the "bjam" Boost.Build system tool (see section
2.1.3.)

  HOST=i586-mingw32msvc

The 'host' specification for the standard configure scripts.

  MINGW_PREFIX=${HOST}-

Path and prefix of the cross compiler executables. Defaults to the
'host' specification followed by '-'. These settings are correct if
your cross compiler has a standard name, e.g. i586-mingw32msvc-gcc.

  INSTALL_DIR=$HOME/mingw

Base installation directory

  PARALLEL=1

Number of processes to execute in parallel. If you have a multi-core
CPU or multiple CPUs installed set this value to the total number of
cores you want to use, e.g. for a dual-core CPU use PARALLEL=2.

2.2.2. Execution

From the mkvtoolnix source directory run:

  ./winbuild/setup_cross_compilation_env.sh

If everything works fine you'll end up with a configured mkvtoolnix
source tree. You just have to run 'make' afterwards. Log files of
everything can be found in $INSTALL_DIR/logs.

2.3. Manual installation

This section mentions libraries with certain version numbers. Unless
stated otherwise newer (or slightly older) versions will work just as
well -- just alter the commands to match your actual library version
numbers.

This guide assumes that all libraries are downloaded to the
$HOME/mingw/src directory.

2.3.1. Preparing the directory tree

This guide assumes a certain directory structure. It consists of the
following directories:

$HOME/mingw         -- base directory for everything else
$HOME/mingw/src     -- contains source code during the build process
$HOME/mingw/include -- contains include files for installed libraries
$HOME/mingw/lib     -- contains libraries for installed libraries

Some libraries will not be installed into the .../include and .../lib
directories but into their own subdirectory, e.g. $HOME/mingw/boost.

Create the directories:

  mkdir $HOME/mingw $HOME/mingw/src $HOME/mingw/include $HOME/mingw/lib

2.3.2. libebml and libmatroska

Get the source code libraries from
http://dl.matroska.org/downloads/libebml/ and
http://dl.matroska.org/downloads/libmatroska/

  cd $HOME/mingw/src
  wget http://dl.matroska.org/downloads/libebml/libebml-1.0.0.tar.bz2 \
    http://dl.matroska.org/downloads/libmatroska/libmatroska-1.0.0.tar.bz2
  bunzip2 < libebml-1.0.0.tar.bz2 | tar xf -
  bunzip2 < libmatroska-1.0.0.tar.bz2 | tar xf -

  cd libebml-1.0.0/make/linux
  perl -pi -e 's/error/info/' Makefile
  make CXX=i586-mingw32msvc-g++ AR="i586-mingw32msvc-ar rcvu" RANLIB=i586-mingw32msvc-ranlib SHARED=no staticlib
  cp libebml.a $HOME/mingw/lib/
  cp -R ../../ebml $HOME/mingw/include/ebml

  cd ../../../libmatroska-1.0.0/make/linux
  perl -pi -e 's/error/info/' Makefile
  export CXXFLAGS=-I$HOME/mingw/include
  export LDFLAGS=-L$HOME/mingw/lib
  make CXX=i586-mingw32msvc-g++ AR="i586-mingw32msvc-ar rcvu" RANLIB=i586-mingw32msvc-ranlib SHARED=no staticlib
  cp libmatroska.a $HOME/mingw/lib/
  cp -R ../../matroska $HOME/mingw/include/matroska

2.3.3. expat

Get precompiled expat binaries for mingw from
http://sourceforge.net/projects/mingw/files/ You need both the
"libexpat...-dll-..." and the "libexpat...-dev-..." packages.

  cd $HOME/mingw/src
  wget 'http://downloads.sourceforge.net/project/mingw/MinGW%20expat/expat-2.0.1-1/libexpat-2.0.1-1-mingw32-dll-1.tar.gz?use_mirror=heanet' \
    'http://downloads.sourceforge.net/project/mingw/MinGW%20expat/expat-2.0.1-1/libexpat-2.0.1-1-mingw32-dev.tar.gz?use_mirror=heanet'
  mkdir expat
  cd expat
  tar xzf ../libexpat-2.0.1-1-mingw32-dll-1.tar.gz
  tar xzf ../libexpat-2.0.1-1-mingw32-dev.tar.gz
  cp -R . $HOME/mingw

2.3.4. zlib

Get precompiled zlib binaries for mingw from
http://sourceforge.net/projects/mingw/files/ You need both the
"libz...-dll-..." and the "libz...-dev-..." packages.

  cd $HOME/mingw/src
  wget 'http://downloads.sourceforge.net/project/mingw/MinGW%20zlib/zlib-1.2.3-1-mingw32/libz-1.2.3-1-mingw32-dll-1.tar.gz?use_mirror=heanet' \
    'http://downloads.sourceforge.net/project/mingw/MinGW%20zlib/zlib-1.2.3-1-mingw32/libz-1.2.3-1-mingw32-dev.tar.gz?use_mirror=heanet'
  mkdir zlib
  cd zlib
  tar xzf ../libz-1.2.3-1-mingw32-dll-1.tar.gz
  tar xzf ../libz-1.2.3-1-mingw32-dev.tar.gz
  cp -R . $HOME/mingw

2.3.5. iconv

Get precompiled iconv binaries for mingw from
http://sourceforge.net/projects/mingw/files/ You need the
"libiconv...-dll-...", the "libiconv...-dev-..." and the
"libcharset...-dll-..." packages.

  cd $HOME/mingw/src
  wget 'http://downloads.sourceforge.net/project/mingw/MinGW%20libiconv/libiconv-1.13.1-1/libiconv-1.13.1-1-mingw32-dll-2.tar.lzma?use_mirror=heanet' \
    'http://downloads.sourceforge.net/project/mingw/MinGW%20libiconv/libiconv-1.13.1-1/libcharset-1.13.1-1-mingw32-dll-1.tar.lzma?use_mirror=heanet' \
    'http://downloads.sourceforge.net/project/mingw/MinGW%20libiconv/libiconv-1.13.1-1/libiconv-1.13.1-1-mingw32-dev.tar.lzma?use_mirror=heanet'
  mkdir iconv
  cd iconv
  lzma -d < ../libiconv-1.13.1-1-mingw32-dll-2.tar.lzma | tar xf -
  lzma -d < ../libcharset-1.13.1-1-mingw32-dll-1.tar.lzma | tar xf -
  lzma -d < ../libiconv-1.13.1-1-mingw32-dev.tar.lzma | tar xf -
  cp -R . $HOME/mingw

2.3.6. libogg, libvorbis and libFLAC

Get the source code archives from
http://downloads.xiph.org/releases/ogg/
http://downloads.xiph.org/releases/vorbis/
http://downloads.xiph.org/releases/flac/

  cd $HOME/mingw/src
  wget http://downloads.xiph.org/releases/ogg/libogg-1.1.4.tar.gz \
    http://downloads.xiph.org/releases/vorbis/libvorbis-1.2.3.tar.bz2 \
    http://downloads.xiph.org/releases/flac/flac-1.2.1.tar.gz
  tar xzf libogg-1.1.4.tar.gz
  bunzip2 < libvorbis-1.2.3.tar.bz2 | tar xf -
  tar xzf flac-1.2.1.tar.gz
  export CFLAGS=-I$HOME/mingw/include
  export CXXFLAGS=-I$HOME/mingw/include
  export LDFLAGS=-L$HOME/mingw/lib
  cd libogg-1.1.4
  ./configure --host=i586-mingw32msvc --prefix=$HOME/mingw
  make
  make install
  cd ../libvorbis-1.2.3
  ./configure --host=i586-mingw32msvc --prefix=$HOME/mingw
  make
  make install
  cd ../flac-1.2.1
  export CFLAGS="-I$HOME/mingw -DSIZE_T_MAX=UINT_MAX"
  export CXXFLAGS="-I$HOME/mingw -DSIZE_T_MAX=UINT_MAX"
  ./configure --host=i586-mingw32msvc --prefix=$HOME/mingw
  make
  make install

2.3.7. boost

Get the Boost source code archive from http://www.boost.org/

Building Boost requires tht you tell the "bjam" build utility which
gcc to use. Note that you also need a working "bjam" binary before
building this library. See section 2.1.3 for details.

  cd $HOME/mingw/src
  wget 'http://downloads.sourceforge.net/project/boost/boost/1.42.0/boost_1_42_0.tar.bz2?use_mirror=heanet'
  bunzip2 < boost_1_42_0.tar.bz2 | tar xf -
  cd boost_1_42_0
  ./bootstrap.sh --with-bjam=/usr/bin/bjam --without-libraries=python,mpi \
    --without-icu --prefix=$HOME/mingw/boost
  echo "using gcc : : i586-mingw32msvc-g++ ;" > user-config.jam
  bjam \
    target-os=windows threading=single threadapi=win32 \
    link=static runtime-link=static variant=release \
    include=$HOME/mingw/include \
    --user-config=user-config.jam --prefix=$HOME/mingw/boost \
    install
  cd $HOME/mingw/boost/lib
  for i in *.lib ; do mv $i $(basename $i .lib).a ; done
  for i in *.a ; do i586-mingw32msvc-ranlib $i ; done

Check if $HOME/prog/mingw/lib contains the filesystem, system and
regex libraries:

  ls $HOME/mingw/boost/lib/libboost_{filesystem,system,regex}*

2.3.8. wxWidgets

Get the full wxWidgets source archive from http://www.wxwidgets.org/

  cd $HOME/mingw/src
  wget 'http://downloads.sourceforge.net/project/wxwindows/wxAll/2.8.10/wxWidgets-2.8.10.tar.bz2?use_mirror=ovh'
  bunzip2 < wxWidgets-2.8.10.tar.bz2 | tar xf -
  cd wxWidgets-2.8.10
  export CFLAGS=-I$HOME/mingw/include
  export CXXFLAGS=-I$HOME/mingw/include
  export LDFLAGS=-L$HOME/mingw/lib
  ./configure --enable-gif --enable-unicode --disable-compat24 --disable-compat26 \
    --host=i586-mingw32msvc --prefix=$HOME/mingw
  make
  make install

2.3.9. gettext (optional)

Get precompiled gettext binaries for mingw from
http://sourceforge.net/projects/mingw/files/ You need both the
"libintl...-dll-..." and the "gettext...-dev-..." packages.

  cd $HOME/mingw/src
  wget 'http://downloads.sourceforge.net/project/mingw/MinGW%20gettext/gettext-0.17-1/libintl-0.17-1-mingw32-dll-8.tar.lzma?use_mirror=heanet' \
    'http://downloads.sourceforge.net/project/mingw/MinGW%20gettext/gettext-0.17-1/gettext-0.17-1-mingw32-dev.tar.lzma?use_mirror=heanet'
  mkdir gettext
  cd gettext
  lzma -d < ../libintl-0.17-1-mingw32-dll-8.tar.lzma | tar xf -
  lzma -d < ../gettext-0.17-1-mingw32-dev.tar.lzma | tar xf -
  cp -R . $HOME/mingw

2.3.10. file/libmagic (optional)

Get precompiled binaries for 'regex' and 'file' for mingw from
http://gnuwin32.sourceforge.net/packages.html You need both the
"...-bin.zip" and the "...-lib.zip" packages.

  cd $HOME/mingw/src
  wget 'http://downloads.sourceforge.net/project/gnuwin32/regex/2.7/regex-2.7-bin.zip' \
    'http://downloads.sourceforge.net/project/gnuwin32/regex/2.7/regex-2.7-lib.zip' \
    'http://downloads.sourceforge.net/project/gnuwin32/file/5.03/file-5.03-bin.zip' \
    'http://downloads.sourceforge.net/project/gnuwin32/file/5.03/file-5.03-lib.zip'
  mkdir file
  cd file
  unzip -o ../regex-2.7-bin.zip
  unzip -o ../regex-2.7-lib.zip
  unzip -o ../file-5.03-bin.zip
  unzip -o ../file-5.03-lib.zip
  cp -R . $HOME/mingw

2.3.11. bzip2 (optional)

Get precompiled bzip2 binaries for mingw from
http://sourceforge.net/projects/mingw/files/ You need both the
"libbz2...-dll-..." and the "libbz2...-dev-..." packages.

  cd $HOME/mingw/src
  wget 'http://downloads.sourceforge.net/project/mingw/MinGW%20bzip2/release%201.0.5-2/libbz2-1.0.5-2-mingw32-dll-2.tar.gz' \
   'http://downloads.sourceforge.net/project/mingw/MinGW%20bzip2/release%201.0.5-2/bzip2-1.0.5-2-mingw32-dev.tar.gz'
  mkdir libbz2
  cd libbz2
  tar xzf ../libbz2-1.0.5-2-mingw32-dll-2.tar.gz
  tar xzf ../bzip2-1.0.5-2-mingw32-dev.tar.gz
  perl -pi -e 'if (m/Core.*low.*level.*library.*functions/) {
      $_ .= qq|
#undef BZ_API
#undef BZ_EXTERN
#define BZ_API(func) func
#define BZ_EXTERN extern
|;
    }
    $_' include/bzlib.h
  cp -R . $HOME/mingw

2.3.12. mkvtoolnix itself

Change back into the mkvtoolnix source code directory and execute the
following commands:

  ./configure \
    --host=i586-mingw32msvc \
    --with-extra-includes=$HOME/mingw/include \
    --with-extra-libs=$HOME/mingw/lib \
    --with-boost=$HOME/mingw/boost \
    --with-wx-config=$HOME/mingw/bin/wx-config
  make

You're done.

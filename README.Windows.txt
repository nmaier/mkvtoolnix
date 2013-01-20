Building MKVToolNix 6.0.0 on Windows
====================================

There is currently only one supported way to build MKVToolNix for
Windows: on Linux using a mingw cross compiler. It is known that you
can also build it on Windows itself with the mingw gcc compiler, but
that's not supported officially as I don't have such a setup myself.

Earlier versions could still be built with Microsoft's Visual Studio /
Visual C++ programs, and those steps were described here as
well. However, current MKVToolNix versions require many features of
the new C++11 standard that even the newest C++ compilers from
Microsoft simply don't support. Therefore the old project files that
enabled building with Visual Studio 8 or newer were removed.

Section 1 -- Building with a mingw cross compiler
-------------------------------------------------

1.1. Preparations

You will need:

- a mingw cross compiler
- roughly 2 GB of free space available

Earlier versions of this document described in painful details how to
build each library. Luckily there's the "mingw-cross-env" project at
http://mingw-cross-env.nongnu.org/ that provides an easy-to-use way of
setting up everything we need.

Unfortunately current versions of mingw-cross-env use a compiler
version that is known to cause issues upon runtime. Therefore the
author maintains his own fork that is known to work. In order to
retrieve that fork you need `git`. Then to the following:

  git clone https://github.com/mbunkus/mxe $HOME/mingw-cross-env

The rest of this guide assumes that you've unpacked mingw-cross-env
into the directory $HOME/mingw-cross-env.

1.2. Automatic build script

mkvtoolnix contains a script that can download, compile and install
all required libraries into the directory $HOME/mingw-cross-env.

If the script does not work or you want to do everything yourself
you'll find instructions for manual compilation in section 1.3.

1.2.1. Script configuration

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

1.2.2. Execution

From the mkvtoolnix source directory run:

  ./winbuild/setup_cross_compilation_env.sh

If everything works fine you'll end up with a configured mkvtoolnix
source tree. You just have to run 'drake' afterwards.

1.3. Manual installation

First you will need the mingw-cross-env build scripts. Get them by
downloading them (see above) and unpacking them into
$HOME/mingw-cross-env.

Next, build the required libraries:

  cd $HOME/mingw-cross-env
  make gcc w32api mingwrt gettext boost bzip2 curl file flac libiconv \
    lzo ogg pthreads vorbis wxwidgets zlib

Append the installation directory to your PATH variable:

  export PATH=$PATH:$HOME/mingw-cross-env/usr/bin
  hash -r

Finally, configure mkvtoolnix:

  cd $HOME/path/to/mkvtoolnix-source
  ./configure \
    --host=i686-pc-mingw32 \
    --with-boost=$HOME/mingw-cross-env/usr/i686-pc-mingw32 \
    --with-wx-config=$HOME/mingw-cross-env/usr/bin/i686-pc-mingw32-wx-config

If everything works then build it:

  ./drake

You're done.

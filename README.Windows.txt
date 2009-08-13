mkvtoolnix 2.9.8 and Windows
----------------------------

---[ NOTE 1 ]---------------------------------------------------------
Versions after 0.8.1 require a new runtime DLL archive. Please download
it from http://www.bunkus.org/videotools/mkvtoolnix/

Thanks.
---[ NOTE 1 ]---------------------------------------------------------

---[ NOTE 2 ]---------------------------------------------------------
THIS FILE IS OUTDATED!
I don't use cygwin anymore but mingw. Unfortunately compilation of
all required libraries is FAR from easy with mingw as almost every
package needs some kind of patch of specialized Makefile.

I'll update this file with build instructions for mingw when I
find the time to do so. They're basically the instructions below
with some additional steps here and there.
---[ NOTE 2 ]--------------------------------------------------------

Since 2003-05-09 it is possible to compile mkvtoolnix under the cygwin
environment under Windows. These are some very short build and
installation instructions. If you just need some binaries then get
them from http://www.bunkus.org/videotools/mkvtoolnix/

1) Download cygwin from http://www.cygwin.com/ and install it. Be sure
   to include all relevant stuff like the auto* packages and all iconv
   packages.
2) Download libogg and libvorbis from http://www.xiph.org/ Get the
   source packages, not the binaries for Windows. They should be named
   libogg-1.0.tar.gz and libvorbis-1.0.tar.gz or something like that
   (don't mind the actual version number, it might be higher than
   1.0).
3) In the cygwin bash execute the following commands which will build
   the Ogg and Vorbis libraries:

cd /usr/local/src
tar xvzf /where/i/put/libogg-1.0.tar.gz
cd libogg-1.0
./configure
make
make install

cd /usr/local/src
tar xvzf /where/i/put/libvorbis-1.0.tar.gz
cd libvorbis-1.0
./configure --with-ogg=/usr/local
make
make install

4) Get current versions of libebml and libmatroska (either from CVS or
   a proper release). For the CVS version do the following:

cd /usr/local/src
cvs -d :ext:anonymous@matroska.corecodec.org:/cvsroot/matroska \
  co libebml
cd libebml/make/linux
make lib install

cd /usr/local/src
cvs -d :ext:anonymous@matroska.corecodec.org:/cvsroot/matroska \
  co libmatroska
cd libmatroska/make/linux
make LIBEBML_INCLUDE_DIR=/usr/local/include/ebml lib install

   The same applies to the releases. Of course here you have to replace
   the 'cvs -d ...' with 'tar xvzf libebml-...', and you have to
   include the proper version number in the cd commands.

5) Download and install wxWindows (http://www.wxwindows.org/) if you
   want a GUI for mkvinfo. Make sure that mkvtoolnix' configure can
   find the wx-config script.

6) Get the mkvtoolnix sources either from CVS or a release (note that
   0.3.2 is the first version that can be compiled under cygwin). For
   the CVS version do the following:

cd /usr/local/src
cvs -d :pserver:anonymous@bunkus.org:/home/cvs co mkvtoolnix
cd mkvtoolnix
./autogen.sh
./configure --with-ogg-prefix=/usr/local \
  --with-vorbis-prefix=/usr/local
make
make install

   If you use a release version the steps are very similar:

cd /usr/local/src
tar xvjf /where/i/put/mkvtoolnix-x.y.z.tar.bz2
cd mkvtoolnix-x.y.z
./configure --with-ogg-prefix=/usr/local \
  --with-vorbis-prefix=/usr/local
make
make install

7) Be happy and drink some milk. Moooo!

Bug reports
-----------

If you're sure you've found a bug - e.g. if one of my programs crashes
with an obscur error message, or if the resulting file is missing part
of the original data, then by all means submit a bug report.

I use Anthill (http://www.bunkus.org/anthill/index.php) as my bug
database. You can submit your bug reports there. Please be as verbose
as possible - e.g. include the command line, if you use Windows or
Linux etc.pp. If at all possible please include sample files as well
so that I can reproduce the issue. If they are larger than 1M then
please upload them somewhere and post a link in the Anthill bug
report.

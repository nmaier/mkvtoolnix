mkvtoolnix 0.3.3 and Windows
----------------------------

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

5) Get the mkvtoolnix sources either from CVS or a release (note that
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

6) Be happy and drink some milk. Moooo!

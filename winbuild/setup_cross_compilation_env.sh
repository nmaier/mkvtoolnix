#!/bin/bash

# Creates a tree with all the required libraries for use with the
# mingw cross compiler. The libraries are compiled appropriately.
# Read the file "README.Windows.txt" for instructions.

#
# SETUP -- adjust these variables if neccessary
#

BJAM=bjam
HOST=i586-mingw32msvc
MINGW_PREFIX=${HOST}-
INSTALL_DIR=$HOME/2
SOURCEFORGE_MIRROR=heanet
PARALLEL=2

#
# END OF SETUP -- usually no need to change anything else
#

# Variables
AR=${MINGW_PREFIX}ar
CC=${MINGW_PREFIX}gcc
CXX=${MINGW_PREFIX}gcc
RANLIB=${MINGW_PREFIX}ranlib

ID_BIN=${INSTALL_DIR}/bin
ID_INCLUDE=${INSTALL_DIR}/include
ID_LIB=${INSTALL_DIR}/lib
ID_BOOST=${INSTALL_DIR}/boost
SRC_DIR=${INSTALL_DIR}/src
LOG_DIR=${INSTALL_DIR}/log

# Package versions
BOOST_VER=1_42_0
BZIP2_VER=1.0.5-1
EXPAT_VER=2.0.1-1
FLAC_VER=1.2.1
GETTEXT_VER=0.17-1
ICONV_VER=1.13
LIBEBML_VER=0.7.8
LIBMATROSKA_VER=0.8.1
MAGIC_VER=5.03-1
MSYS_VER=1.0.11
OGG_VER=1.1.4
VORBIS_VER=1.2.3
WXWIDGETS_VER=2.8.10
ZLIB_VER=1.2.3-1

# Important environment variables
function set_xyzflags {
  export CFLAGS="-I${ID_INCLUDE}$@"
  export CXXFLAGS="-I${ID_INCLUDE}$@"
  export LDFLAGS="-L${ID_LIB}$@"
  export MAKEFLAGS="-j${PARALLEL}"
}

# Functions
function fail {
  echo failed
  exit 1
}

function create_directories {
  echo Preparing directory tree
  for dir in $INSTALL_DIR $ID_BIN $ID_INCLUDE $ID_LIB $SRC_DIR $LOG_DIR ; do
    test -d $dir || mkdir $dir
  done
}

function install_libebml {
  local log=$LOG_DIR/libebml.log
  echo Installing libebml
  test -f $ID_LIB/libebml.a -a -f $ID_INCLUDE/ebml/EbmlElement.h && return

  cd $SRC_DIR || exit 1
  if [ ! -f libebml-${LIBEBML_VER}.tar.bz2 ]; then
    wget http://dl.matroska.org/downloads/libebml/libebml-${LIBEBML_VER}.tar.bz2 >> $log 2>&1 || fail
  fi
  rm -rf libebml-${LIBEBML_VER}
  bunzip2 < libebml-${LIBEBML_VER}.tar.bz2 | tar xf - >> $log 2>&1 || fail
  cd libebml-${LIBEBML_VER}/make/linux >> $log 2>&1 || fail
  perl -pi -e 's/error/info/' Makefile >> $log 2>&1 || fail
  make CXX=${CXX} AR="${AR} rcvu" RANLIB=${RANLIB} SHARED=no staticlib >> $log 2>&1 || fail
  cp libebml.a ${ID_LIB}/ >> $log 2>&1 || fail
  cp -R ../../ebml ${ID_INCLUDE}/ebml >> $log 2>&1 || fail
}

function install_libmatroska {
  local log=$LOG_DIR/libmatroska.log
  echo Installing libmatroska
  test -f $ID_LIB/libmatroska.a -a -f $ID_INCLUDE/matroska/KaxCluster.h && return

  cd $SRC_DIR || exit 1
  if [ ! -f libmatroska-${LIBMATROSKA_VER}.tar.bz2 ]; then
    wget http://dl.matroska.org/downloads/libmatroska/libmatroska-${LIBMATROSKA_VER}.tar.bz2 >> $log 2>&1 || fail
  fi
  rm -rf libmatroska-${LIBMATROSKA_VER}
  bunzip2 < libmatroska-${LIBMATROSKA_VER}.tar.bz2 | tar xf - >> $log 2>&1 || fail
  cd libmatroska-${LIBMATROSKA_VER}/make/linux >> $log 2>&1 || fail
  perl -pi -e 's/error/info/' Makefile >> $log 2>&1 || fail
  make CXX=${CXX} AR="${AR} rcvu" RANLIB=${RANLIB} SHARED=no EBML_DIR=${SRC_DIR}/libebml-${LIBEBML_VER} staticlib >> $log 2>&1 || fail
  cp libmatroska.a ${ID_LIB}/ >> $log 2>&1 || fail
  cp -R ../../matroska ${ID_INCLUDE}/matroska >> $log 2>&1 || fail
}

function install_expat {
  local log=$LOG_DIR/expat.log
  echo Installing expat
  test -f $ID_LIB/libexpat.a -a -f $ID_INCLUDE/expat.h && return

  local dll_tar=libexpat-${EXPAT_VER}-mingw32-dll-1.tar.gz
  local dev_tar=libexpat-${EXPAT_VER}-mingw32-dev.tar.gz
  local base_url=http://downloads.sourceforge.net/project/mingw/MinGW%20expat/expat-${EXPAT_VER}

  cd $SRC_DIR || exit 1
  if [ ! -f ${dll_tar} ]; then
    wget "${base_url}/${dll_tar}?use_mirror=${SOURCEFORGE_MIRROR}" >> $log 2>&1 || fail
  fi
  if [ ! -f ${dev_tar} ]; then
    wget "${base_url}/${dev_tar}?use_mirror=${SOURCEFORGE_MIRROR}" >> $log 2>&1 || fail
  fi

  test -d expat || { mkdir expat >> $log 2>&1 || fail ; }
  cd expat >> $log 2>&1 || fail
  tar xzf ../${dll_tar} >> $log 2>&1 || fail
  tar xzf ../${dev_tar} >> $log 2>&1 || fail
  cp -R . $INSTALL_DIR >> $log 2>&1 || fail
}

function install_zlib {
  local log=$LOG_DIR/zlib.log
  echo Installing zlib
  test -f $ID_LIB/libz.a -a -f $ID_INCLUDE/zlib.h && return

  local dll_tar=libz-${ZLIB_VER}-mingw32-dll-1.tar.gz
  local dev_tar=libz-${ZLIB_VER}-mingw32-dev.tar.gz
  local base_url=http://downloads.sourceforge.net/project/mingw/MinGW%20zlib/zlib-${ZLIB_VER}-mingw32

  cd $SRC_DIR || exit 1
  if [ ! -f ${dll_tar} ]; then
    wget "${base_url}/${dll_tar}?use_mirror=${SOURCEFORGE_MIRROR}" >> $log 2>&1 || fail
  fi
  if [ ! -f ${dev_tar} ]; then
    wget "${base_url}/${dev_tar}?use_mirror=${SOURCEFORGE_MIRROR}" >> $log 2>&1 || fail
  fi

  test -d zlib || { mkdir zlib >> $log 2>&1 || fail ; }
  cd zlib >> $log 2>&1 || fail
  tar xzf ../${dll_tar} >> $log 2>&1 || fail
  tar xzf ../${dev_tar} >> $log 2>&1 || fail
  cp -R . $INSTALL_DIR >> $log 2>&1 || fail
}

function install_iconv {
  local log=$LOG_DIR/iconv.log
  echo Installing iconv
  test -f $ID_LIB/libiconv.dll.a -a -f $ID_INCLUDE/iconv.h && return

  local dll_tar=libiconv-${ICONV_VER}-mingw32-dll-2.tar.gz
  local dev_tar=libiconv-${ICONV_VER}-mingw32-dev.tar.gz
  local base_url=http://downloads.sourceforge.net/project/mingw/MinGW%20libiconv/release%20${ICONV_VER}

  cd $SRC_DIR || exit 1
  if [ ! -f ${dll_tar} ]; then
    wget "${base_url}/${dll_tar}?use_mirror=${SOURCEFORGE_MIRROR}" >> $log 2>&1 || fail
  fi
  if [ ! -f ${dev_tar} ]; then
    wget "${base_url}/${dev_tar}?use_mirror=${SOURCEFORGE_MIRROR}" >> $log 2>&1 || fail
  fi

  test -d iconv || { mkdir iconv >> $log 2>&1 || fail ; }
  cd iconv >> $log 2>&1 || fail
  tar xzf ../${dll_tar} >> $log 2>&1 || fail
  tar xzf ../${dev_tar} >> $log 2>&1 || fail
  cp -R . $INSTALL_DIR >> $log 2>&1 || fail
}

function install_ogg {
  local log=$LOG_DIR/ogg.log
  echo Installing ogg
  test -f $ID_LIB/libogg.a -a -f $ID_INCLUDE/ogg/ogg.h && return

  local dev_tar=libogg-${OGG_VER}.tar.gz
  local base_url=http://downloads.xiph.org/releases/ogg

  cd $SRC_DIR || exit 1
  if [ ! -f ${dev_tar} ]; then
    wget "${base_url}/${dev_tar}" >> $log 2>&1 || fail
  fi

  rm -rf libogg-${OGG_VER} >> $log 2>&1 || fail
  tar xzf ${dev_tar} >> $log 2>&1 || fail
  cd libogg-${OGG_VER} >> $log 2>&1 || fail
  ./configure --host=${HOST} --prefix=${INSTALL_DIR} >> $log 2>&1 || fail
  make >> $log 2>&1 || fail
  make install >> $log 2>&1 || fail
}

function install_vorbis {
  local log=$LOG_DIR/vorbis.log
  echo Installing vorbis
  test -f $ID_LIB/libvorbis.a -a -f $ID_INCLUDE/vorbis/codec.h && return

  local dev_tar=libvorbis-${VORBIS_VER}.tar.gz
  local base_url=http://downloads.xiph.org/releases/vorbis

  cd $SRC_DIR || exit 1
  if [ ! -f ${dev_tar} ]; then
    wget "${base_url}/${dev_tar}" >> $log 2>&1 || fail
  fi

  rm -rf libvorbis-${VORBIS_VER} >> $log 2>&1 || fail
  tar xzf ${dev_tar} >> $log 2>&1 || fail
  cd libvorbis-${VORBIS_VER} >> $log 2>&1 || fail
  ./configure --host=${HOST} --prefix=${INSTALL_DIR} >> $log 2>&1 || fail
  make >> $log 2>&1 || fail
  make install >> $log 2>&1 || fail
}

function install_flac {
  local log=$LOG_DIR/flac.log
  echo Installing flac
  test -f $ID_LIB/libFLAC.a -a -f $ID_INCLUDE/FLAC/format.h && return

  local dev_tar=flac-${FLAC_VER}.tar.gz
  local base_url=http://downloads.xiph.org/releases/flac

  cd $SRC_DIR || exit 1
  if [ ! -f ${dev_tar} ]; then
    wget "${base_url}/${dev_tar}" >> $log 2>&1 || fail
  fi

  rm -rf flac-${FLAC_VER} >> $log 2>&1 || fail
  tar xzf ${dev_tar} >> $log 2>&1 || fail
  cd flac-${FLAC_VER} >> $log 2>&1 || fail
  set_xyzflags " -DSIZE_T_MAX=UINT_MAX"
  ./configure --host=${HOST} --prefix=${INSTALL_DIR} >> $log 2>&1 || fail
  make >> $log 2>&1 || fail
  make install >> $log 2>&1 || fail
  set_xyzflags
}

function install_file {
  local log=$LOG_DIR/file.log
  echo Installing file/magic
  test -f $ID_LIB/libmagic.a -a -f $ID_INCLUDE/magic.h && return

  local dll_tar=libmagic-${MAGIC_VER}-msys-${MSYS_VER}-dll-1.tar.lzma
  local dev_tar=libmagic-${MAGIC_VER}-msys-${MSYS_VER}-dev.tar.lzma
  local base_url=http://downloads.sourceforge.net/project/mingw/MSYS%20file/file-${MAGIC_VER}

  cd $SRC_DIR || exit 1
  if [ ! -f ${dll_tar} ]; then
    wget "${base_url}/${dll_tar}?use_mirror=${SOURCEFORGE_MIRROR}" >> $log 2>&1 || fail
  fi
  if [ ! -f ${dev_tar} ]; then
    wget "${base_url}/${dev_tar}?use_mirror=${SOURCEFORGE_MIRROR}" >> $log 2>&1 || fail
  fi

  test -d file || { mkdir file >> $log 2>&1 || fail ; }
  cd file >> $log 2>&1 || fail
  lzma -d < ../${dll_tar} | tar xf - >> $log 2>&1 || fail
  lzma -d < ../${dev_tar} | tar xf - >> $log 2>&1 || fail
  cp -R . $INSTALL_DIR >> $log 2>&1 || fail
}

function install_gettext {
  local log=$LOG_DIR/gettext.log
  echo Installing gettext
  test -f $ID_LIB/libintl.a -a -f $ID_INCLUDE/libintl.h && return

  local dev_tar=gettext-${GETTEXT_VER}-msys-${MSYS_VER}-dev.tar.lzma
  local base_url=http://downloads.sourceforge.net/project/mingw/MSYS%20gettext/gettext-${GETTEXT_VER}

  cd $SRC_DIR || exit 1
  if [ ! -f ${dev_tar} ]; then
    wget "${base_url}/${dev_tar}?use_mirror=${SOURCEFORGE_MIRROR}" >> $log 2>&1 || fail
  fi

  test -d gettext || { mkdir gettext >> $log 2>&1 || fail ; }
  cd gettext >> $log 2>&1 || fail
  lzma -d < ../${dev_tar} | tar xf - >> $log 2>&1 || fail
  cp -R . $INSTALL_DIR >> $log 2>&1 || fail
}

function install_bzip2 {
  local log=$LOG_DIR/bzip2.log
  echo Installing bzip2
  test -f $ID_LIB/libbz2.a -a -f $ID_INCLUDE/bzlib.h && return

  local dll_tar=libbz2-${BZIP2_VER}-msys-${MSYS_VER}-dll-1.tar.gz
  local dev_tar=libbz2-${BZIP2_VER}-msys-${MSYS_VER}-dev.tar.gz
  local base_url=http://downloads.sourceforge.net/project/mingw/MSYS%20bzip2/bzip2-${BZIP2_VER}

  cd $SRC_DIR || exit 1
  if [ ! -f ${dll_tar} ]; then
    wget "${base_url}/${dll_tar}?use_mirror=${SOURCEFORGE_MIRROR}" >> $log 2>&1 || fail
  fi
  if [ ! -f ${dev_tar} ]; then
    wget "${base_url}/${dev_tar}?use_mirror=${SOURCEFORGE_MIRROR}" >> $log 2>&1 || fail
  fi

  test -d file || { mkdir file >> $log 2>&1 || fail ; }
  cd file >> $log 2>&1 || fail
  tar xzf ../${dll_tar} >> $log 2>&1 || fail
  tar xzf ../${dev_tar} >> $log 2>&1 || fail
  perl -pi -e 'if (m/Core.*low.*level.*library.*functions/) {
      $_ .= qq|
#undef BZ_API
#undef BZ_EXTERN
#define BZ_API(func) func
#define BZ_EXTERN extern
|;
    }
    $_' include/bzlib.h >> $log 2>&1 || fail
  cp -R . $INSTALL_DIR >> $log 2>&1 || fail
}

function install_wxwidgets {
  local log=$LOG_DIR/wxwidgets.log
  echo Installing wxwidgets
  test -f $ID_LIB/wxbase28u_gcc_custom.dll -a -f $ID_INCLUDE/wx-2.8/wx/init.h && return

  local dev_tar=wxWidgets-${WXWIDGETS_VER}.tar.bz2
  local base_url=http://downloads.sourceforge.net/project/wxwindows/wxAll/${WXWIDGETS_VER}

  cd $SRC_DIR || exit 1
  if [ ! -f ${dev_tar} ]; then
    wget "${base_url}/${dev_tar}?use_mirror=${SOURCEFORGE_MIRROR}" >> $log 2>&1 || fail
  fi

  if [ ! -d wxWidgets-${WXWIDGETS_VER} ]; then
    bunzip2 < ${dev_tar} | tar xf - >> $log 2>&1 || fail
  fi
  cd wxWidgets-${WXWIDGETS_VER} >> $log 2>&1 || fail
  if [ ! -f config.log ]; then
    ./configure --enable-gif --enable-unicode --disable-compat24 --disable-compat26 \
      --host=${HOST} --prefix=${INSTALL_DIR} >> $log 2>&1 || fail
  fi
  make >> $log 2>&1 || fail
  make install >> $log 2>&1 || fail
}

function install_boost {
  local log=$LOG_DIR/boost.log
  echo Installing boost
  test -f $ID_BOOST/lib/libboost_filesystem.a -a -f $ID_BOOST/include/boost/format.hpp && return

  local dev_tar=boost_${BOOST_VER}.tar.bz2
  local base_url=http://downloads.sourceforge.net/project/boost/boost/${BOOST_VER//_/.}

  cd $SRC_DIR || exit 1
  if [ ! -f ${dev_tar} ]; then
    wget "${base_url}/${dev_tar}?use_mirror=${SOURCEFORGE_MIRROR}" >> $log 2>&1 || fail
  fi

  local dir_name=boost_${BOOST_VER}
  if [ ! -d ${dir_name} ]; then
    bunzip2 < ${dev_tar} | tar xf - >> $log 2>&1 || fail
  fi
  cd ${dir_name} >> $log 2>&1 || fail

  ./bootstrap.sh --with-bjam=/usr/bin/bjam --without-libraries=python,mpi \
    --without-icu --prefix=${ID_BOOST} >> $log 2>&1 || fail
  echo "using gcc : : ${MINGW_PREFIX}g++ ;" > user-config.jam
  bjam \
    target-os=windows threading=single threadapi=win32 \
    link=static runtime-link=static variant=release \
    include=${ID_INCLUDE} \
    --user-config=user-config.jam --prefix=${ID_BOOST} \
    -j ${PARALLEL} install >> $log 2>&1 || fail
  cd ${ID_BOOST}/lib >> $log 2>&1 || fail
  for i in *.lib ; do mv $i $(basename $i .lib).a ; done
  for i in *.a ; do ${RANLIB} $i ; done
}

function create_run_configure_script {
  echo Creating \'run_configure.sh\' script
  cat > run_configure.sh <<EOF
#!/bin/bash

./configure \\
  --host=${HOST} \\
  --with-extra-includes=${ID_INCLUDE} \\
  --with-extra-libs=${ID_LIB} \\
  --with-boost=${ID_BOOST} \\
  --with-wx-config=${ID_BIN}/wx-config "\$@"

exit \$?
EOF
  chmod 755 run_configure.sh
}

function configure_mkvtoolnix {
  echo Configuring mkvtoolnix
  local log=${LOG_DIR}/configure.log
  ./run_configure.sh >> $log 2>&1
  local result=$?

  echo
  if [ $result -eq 0 ]; then
    echo 'Configuration went well. Congratulations. You can now run "make".'
    echo "You can find the output of configure in the file ${log}."
  else
    echo "Configuration failed. Take a look at the files"
    echo "${log} and, if that doesn't help, ./config.log."
  fi

  echo
  echo 'If you need to re-configure mkvtoolnix then you can do that with'
  echo 'the script ./run_configure.sh. Any parameter you pass to run_configure.sh'
  echo 'will be passed to ./configure as well.'
}

# main

set_xyzflags
create_directories
install_libebml
install_libmatroska
install_expat
install_zlib
install_iconv
install_ogg
install_vorbis
install_flac
install_gettext
install_file
install_bzip2
install_wxwidgets
install_boost
create_run_configure_script
configure_mkvtoolnix

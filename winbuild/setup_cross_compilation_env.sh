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
BZIP2_VER=1.0.5-2
EXPAT_VER=2.0.1-1
FLAC_VER=1.2.1
GETTEXT_VER=0.17-1
ICONV_VER=1.13.1-1
LIBEBML_VER=0.7.8
LIBMATROSKA_VER=0.8.1
FILE_VER=5.03
OGG_VER=1.1.4
REGEX_VER=2.7
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

function dl_sf {
  local base_url="$1"
  shift

  while [ "$1" != "" ]; do
    if [ ! -f "$1" ]; then
      wget --passive-ftp "${base_url}/${1}?use_mirror=${SOURCEFORGE_MIRROR}" >> ${LOG} 2>&1 || fail
    fi
    shift
  done
}

function dl {
  local base_url="$1"
  shift

  while [ "$1" != "" ]; do
    if [ ! -f "$1" ]; then
      wget --passive-ftp "${base_url}/${1}" >> ${LOG} 2>&1 || fail
    fi
    shift
  done
}

function setup_dir {
  local dir="$1"

  test -d "${dir}" || { mkdir "${dir}" >> ${LOG} 2>&1 || fail ; }
  cd "${dir}" >> ${LOG} 2>&1 || fail
}

function remove_unpack_cd {
  local dir="$1"
  local file="$2"

  rm -rf ${dir} >> ${LOG} 2>&1 || fail
  unpack ${file}
  cd ${dir} >> ${LOG} 2>&1 || fail
}

function goto_src {
  cd ${SRC_DIR} >> ${LOG} 2>&1 || fail
}

function copy_files {
  cp -R . ${INSTALL_DIR} >> ${LOG} 2>&1 || fail
}

function unpack {
  while [ "${1}" != "" ]; do
    local file="${1}"
    local ext="${file##*.}"
    local decompressor

    if [ ! -f "${file}" -a -f "../${file}" ]; then
      file="../${file}"
    fi

    case "${ext}" in
      gz)
        decompressor=gunzip
        ;;
      bz2)
        decompressor=bunzip2
        ;;
      lzma)
        decompressor="lzma -d"
        ;;
      zip)
        decompressor="unzip -o"
        ;;
      *)
        echo Unknown compression type "$ext"
        exit 5
        ;;
    esac

    case "${ext}" in
      zip)
        ${decompressor} "${file}" >> ${LOG} 2>&1 || fail
        ;;
      *)
        ${decompressor} < "${file}" 2>> ${LOG} | tar xf - >> ${LOG} 2>&1 || fail
        ;;
    esac

    shift
  done
}

function init {
  local module="$1"
  LOG=${LOG_DIR}/${module}.log
  echo Installing ${module}
}

function run_configure {
  ./configure --host=${HOST} --prefix=${INSTALL_DIR} "$@" >> ${LOG} 2>&1 || fail
}

function make_and_install {
  make >> $LOG 2>&1 || fail
  make install >> $LOG 2>&1 || fail
}

function create_directories {
  echo Preparing directory tree
  for dir in ${INSTALL_DIR} ${ID_BIN} ${ID_INCLUDE} ${ID_LIB} ${SRC_DIR} ${LOG_DIR} ; do
    test -d $dir || mkdir $dir
  done
}

function install_libebml {
  init ebml
  test -f $ID_LIB/libebml.a -a -f $ID_INCLUDE/ebml/EbmlElement.h && return

  local dir=libebml-${LIBEBML_VER}
  local file=${dir}.tar.bz2

  goto_src
  dl http://dl.matroska.org/downloads/libebml ${file}
  rm -rf ${dir}
  unpack ${file}
  cd ${dir}/make/linux >> ${LOG} 2>&1 || fail
  perl -pi -e 's/error/info/' Makefile >> ${LOG} 2>&1 || fail
  make CXX=${CXX} AR="${AR} rcvu" RANLIB=${RANLIB} SHARED=no staticlib >> ${LOG} 2>&1 || fail
  cp libebml.a ${ID_LIB}/ >> ${LOG} 2>&1 || fail
  cp -R ../../ebml ${ID_INCLUDE}/ebml >> ${LOG} 2>&1 || fail
}

function install_libmatroska {
  init matroska
  test -f $ID_LIB/libmatroska.a -a -f $ID_INCLUDE/matroska/KaxCluster.h && return

  local dir=libmatroska-${LIBMATROSKA_VER}
  local file=${dir}.tar.bz2

  goto_src
  dl http://dl.matroska.org/downloads/libmatroska ${file}
  rm -rf ${dir}
  unpack ${file}
  cd ${dir}/make/linux >> ${LOG} 2>&1 || fail
  perl -pi -e 's/error/info/' Makefile >> ${LOG} 2>&1 || fail
  make CXX=${CXX} AR="${AR} rcvu" RANLIB=${RANLIB} SHARED=no EBML_DIR=${SRC_DIR}/libebml-${LIBEBML_VER} staticlib >> ${LOG} 2>&1 || fail
  cp libmatroska.a ${ID_LIB}/ >> ${LOG} 2>&1 || fail
  cp -R ../../matroska ${ID_INCLUDE}/matroska >> ${LOG} 2>&1 || fail
}

function install_expat {
  init expat
  test -f $ID_LIB/libexpat.a -a -f $ID_INCLUDE/expat.h && return

  local dll_tar=libexpat-${EXPAT_VER}-mingw32-dll-1.tar.gz
  local dev_tar=libexpat-${EXPAT_VER}-mingw32-dev.tar.gz

  goto_src
  dl_sf http://downloads.sourceforge.net/project/mingw/MinGW%20expat/expat-${EXPAT_VER} ${dll_tar} ${dev_tar}
  setup_dir expat
  unpack ${dll_tar} ${dev_tar}
  copy_files
}

function install_zlib {
  init zlib
  test -f $ID_LIB/libz.a -a -f $ID_INCLUDE/zlib.h && return

  local dll_tar=libz-${ZLIB_VER}-mingw32-dll-1.tar.gz
  local dev_tar=libz-${ZLIB_VER}-mingw32-dev.tar.gz

  goto_src
  dl_sf http://downloads.sourceforge.net/project/mingw/MinGW%20zlib/zlib-${ZLIB_VER}-mingw32 ${dll_tar} ${dev_tar}
  setup_dir zlib
  unpack ${dll_tar} ${dev_tar}
  copy_files
}

function install_iconv {
  init iconv
  test -f $ID_LIB/libiconv.dll.a -a -f $ID_LIB/libcharset.dll.a -a -f $ID_INCLUDE/iconv.h && return

  local dll_tar=libiconv-${ICONV_VER}-mingw32-dll-2.tar.lzma
  local dll2_tar=libcharset-${ICONV_VER}-mingw32-dll-1.tar.lzma
  local dev_tar=libiconv-${ICONV_VER}-mingw32-dev.tar.lzma

  goto_src
  dl_sf http://downloads.sourceforge.net/project/mingw/MinGW%20libiconv/libiconv-${ICONV_VER} ${dll_tar} ${dll2_tar} ${dev_tar}
  setup_dir iconv
  unpack ${dll_tar} ${dll2_tar} ${dev_tar}
  copy_files
}

function install_ogg {
  init ogg
  test -f $ID_LIB/libogg.a -a -f $ID_INCLUDE/ogg/ogg.h && return

  local dir=libogg-${OGG_VER}
  local dev_tar=${dir}.tar.gz

  goto_src
  dl http://downloads.xiph.org/releases/ogg ${dev_tar}

  remove_unpack_cd ${dir} ${dev_tar}
  run_configure
  make_and_install
}

function install_vorbis {
  init vorbis
  test -f $ID_LIB/libvorbis.a -a -f $ID_INCLUDE/vorbis/codec.h && return

  local dir=libvorbis-${VORBIS_VER}
  local dev_tar=${dir}.tar.gz

  goto_src
  dl http://downloads.xiph.org/releases/vorbis ${dev_tar}

  remove_unpack_cd ${dir} ${dev_tar}
  run_configure
  make_and_install
}

function install_flac {
  init flac
  test -f $ID_LIB/libFLAC.a -a -f $ID_INCLUDE/FLAC/format.h && return

  local dir=flac-${FLAC_VER}
  local dev_tar=${dir}.tar.gz

  goto_src
  dl http://downloads.xiph.org/releases/flac ${dev_tar}
  remove_unpack_cd ${dir} ${dev_tar}
  set_xyzflags " -DSIZE_T_MAX=UINT_MAX"
  run_configure
  make_and_install
  set_xyzflags
}

function install_regex {
  init regex
  test -f $ID_LIB/libregex.dll.a -a -f $ID_INCLUDE/regex.h && return

  local bin_zip=regex-${REGEX_VER}-bin.zip
  local dev_zip=regex-${REGEX_VER}-lib.zip

  goto_src
  dl_sf http://downloads.sourceforge.net/project/gnuwin32/regex/${REGEX_VER} ${bin_zip} ${dev_zip}
  setup_dir regex
  unpack ${bin_zip} ${dev_zip}
  copy_files
}

function install_file {
  init file
  test -f $ID_LIB/libmagic.dll.a -a -f $ID_INCLUDE/magic.h && return

  local dll_zip=file-${FILE_VER}-bin.zip
  local dev_zip=file-${FILE_VER}-lib.zip

  goto_src
  dl_sf http://downloads.sourceforge.net/project/gnuwin32/file/${FILE_VER} ${dll_zip} ${dev_zip}
  setup_dir file
  unpack ${dll_zip} ${dev_zip}
  copy_files
}

function install_gettext {
  init gettext
  test -f $ID_LIB/libintl.dll.a -a -f $ID_INCLUDE/libintl.h && return

  local dll_tar=libintl-${GETTEXT_VER}-mingw32-dll-8.tar.lzma
  local dev_tar=gettext-${GETTEXT_VER}-mingw32-dev.tar.lzma

  goto_src
  dl_sf http://downloads.sourceforge.net/project/mingw/MinGW%20gettext/gettext-${GETTEXT_VER} ${dll_tar} ${dev_tar}
  setup_dir gettext
  unpack ${dll_tar} ${dev_tar}
  copy_files
}

function install_bzip2 {
  init bzip2
  test -f $ID_LIB/libbz2.a -a -f $ID_INCLUDE/bzlib.h && return

  local dll_tar=libbz2-${BZIP2_VER}-mingw32-dll-2.tar.gz
  local dev_tar=bzip2-${BZIP2_VER}-mingw32-dev.tar.gz

  goto_src
  dl_sf http://downloads.sourceforge.net/project/mingw/MinGW%20bzip2/release%20${BZIP2_VER} ${dll_tar} ${dev_tar}
  setup_dir bzip2
  unpack ${dll_tar} ${dev_tar}
  perl -pi -e 'if (m/Core.*low.*level.*library.*functions/) {
      $_ .= qq|
#undef BZ_API
#undef BZ_EXTERN
#define BZ_API(func) func
#define BZ_EXTERN extern
|;
    }
    $_' include/bzlib.h >> ${LOG} 2>&1 || fail
  copy_files
}

function install_wxwidgets {
  init wxwidgets
  test -f $ID_LIB/wxbase28u_gcc_custom.dll -a -f $ID_INCLUDE/wx-2.8/wx/init.h && return

  local dir=wxWidgets-${WXWIDGETS_VER}
  local dev_tar=${dir}.tar.bz2

  goto_src
  dl_sf http://downloads.sourceforge.net/project/wxwindows/wxAll/${WXWIDGETS_VER} ${dev_tar}
  remove_unpack_cd ${dir} ${dev_tar}
  run_configure --enable-gif --enable-unicode --disable-compat24 --disable-compat26
  make_and_install
}

function install_boost {
  init boost
  test -f $ID_BOOST/lib/libboost_filesystem.a -a -f $ID_BOOST/include/boost/format.hpp && return

  local dir=boost_${BOOST_VER}
  local dev_tar=${dir}.tar.bz2

  goto_src
  dl_sf http://downloads.sourceforge.net/project/boost/boost/${BOOST_VER//_/.} ${dev_tar}
  remove_unpack_cd ${dir} ${dev_tar}

  ./bootstrap.sh --with-bjam=/usr/bin/bjam --without-libraries=python,mpi \
    --without-icu --prefix=${ID_BOOST} >> ${LOG} 2>&1 || fail
  echo "using gcc : : ${MINGW_PREFIX}g++ ;" > user-config.jam
  bjam \
    target-os=windows threading=single threadapi=win32 \
    link=static runtime-link=static variant=release \
    include=${ID_INCLUDE} \
    --user-config=user-config.jam --prefix=${ID_BOOST} \
    -j ${PARALLEL} install >> ${LOG} 2>&1 || fail
  cd ${ID_BOOST}/lib >> ${LOG} 2>&1 || fail
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
  ./run_configure.sh >> ${LOG} 2>&1
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
install_regex
install_file
install_bzip2
install_wxwidgets
install_boost
create_run_configure_script
configure_mkvtoolnix

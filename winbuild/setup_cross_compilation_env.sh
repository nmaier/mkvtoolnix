#!/bin/bash

set -e

# Creates a tree with all the required libraries for use with the
# mingw cross compiler. The libraries are compiled appropriately.
# Read the file "README.Windows.txt" for instructions.

#
# SETUP -- adjust these variables if neccessary.
# You can also override them from the command line:
# INSTALL_DIR=/opt/mingw ./setup_cross_compilation_env.sh
#

TARGET=${TARGET:-i686-pc-mingw32}
INSTALL_DIR=${INSTALL_DIR:-$HOME/mingw-cross-env}
# Leave this empty if you want the script to use all of your CPU
# cores.
PARALLEL=${PARALLEL:-$(( $(awk '/^core id/ { print $4 }' /proc/cpuinfo | sort | tail -n 1) + 2 ))}

#
# END OF SETUP -- usually no need to change anything else
#

SRCDIR=$(pwd)
LOGFILE=$(mktemp)

function update_mingw_cross_env {
  if [[ ! -d $INSTALL_DIR ]]; then
    echo Retrieving the mingw-cross-env build scripts >> $LOGFILE
    hg clone http://hg.savannah.nongnu.org/hgweb/mingw-cross-env $INSTALL_DIR >> $LOGFILE 2>&1
  else
    echo Updating the mingw-cross-env build scripts >> $LOGFILE
    cd $INSTALL_DIR
    hg update >> $LOGFILE 2>&1
  fi
}

function create_run_configure_script {
  cd $SRCDIR

  echo Creating \'run_configure.sh\' script
  cat > run_configure.sh <<EOF
#!/bin/bash

export PATH=$PATH:${INSTALL_DIR}/usr/${TARGET}/bin
hash -r

./configure \\
  --host=${TARGET} \\
  --with-boost="${INSTALL_DIR}/usr/${TARGET}" \\
  --with-wx-config="${INSTALL_DIR}/usr/${TARGET}/bin/${TARGET}-wx-config" \\
  "\$@"

exit \$?
EOF
  chmod 755 run_configure.sh
}

function configure_mkvtoolnix {
  cd $SRCDIR

  echo Running configure.
  ./run_configure.sh >> $LOGFILE 2>&1
  local result=$?

  echo
  if [ $result -eq 0 ]; then
    echo 'Configuration went well. Congratulations. You can now run "drake"'
    echo 'after adding the mingw cross compiler installation directory to your PATH:'
    echo '  export PATH=$PATH:'${INSTALL_DIR}'/usr/'${TARGET}'/bin'
    echo '  hash -r'
    echo '  ./drake'
  else
    echo "Configuration failed. Look at ${LOGFILE} as well as"
    echo "at ./config.log for hints as to why."
  fi

  echo
  echo 'If you need to re-configure mkvtoolnix then you can do that with'
  echo 'the script ./run_configure.sh. Any parameter you pass to run_configure.sh'
  echo 'will be passed to ./configure as well.'
}

function build_base {
  echo Building the cross-compiler itself
  cd ${INSTALL_DIR}
  make TARGET=${TARGET} JOBS=${PARALLEL} gcc w32api mingwrt >> $LOGFILE 2>&1
}

function build_libraries {
  echo Building the required libraries
  cd ${INSTALL_DIR}
  make TARGET=${TARGET} JOBS=${PARALLEL} gettext boost bzip2 curl flac libiconv ogg pthreads vorbis wxwidgets zlib >> $LOGFILE 2>&1
}

# main

echo "Cross-compiling mkvtoolnix. Log output can be found in ${LOGFILE}"
update_mingw_cross_env
build_base
build_libraries
create_run_configure_script
configure_mkvtoolnix

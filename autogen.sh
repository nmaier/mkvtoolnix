#!/bin/sh
# Run this to set up the build system: configure, makefiles, etc.
# (based on the version in enlightenment's cvs)

# For MinGW I use my very own Makefile system. So just copy them over
# to Makefile for convenience's sake.
if gcc -v 2>&1 | grep -i mingw > /dev/null 2> /dev/null; then
  echo Detected MinGW. Will copy the Makefile,mingw to Makefile and
  echo make some adjustments.
  echo ''

  for i in `find -name Makefile.mingw`; do
    n=`echo $i | sed 's/\.mingw//'`
    echo "Creating $n from $i"
    sed -e "s/Makefile.mingw/Makefile/g" < $i > $n
  done

  if test "x$1" = "x"; then
    if [ ! -f Makefile.options ]; then
      echo "Creating Makefile.options from Makefile.mingw.options"
      cp Makefile.mingw.options Makefile.options
    else
      echo 'Not overwriting Makefile.options.'
    fi
    # Extract the version number from os.h
    VERSION=`grep '# define VERSION' src/common/os.h | sed -e 's;# define VERSION ;;' -e 's;";;g'`
    echo "Creating config.h from config.h.mingw (version is $VERSION)"
    sed -e 's/#define VERSION.*/#define VERSION "'$VERSION'"/' < config.h.mingw > config.h
    echo ''
    echo 'Creating dependencies (calling "make depend")'
    echo ''
    make depend
  else
    echo 'Not creating config.h.'
    echo 'Not creating the dependencies.'
  fi
  echo ''

  echo 'Done with the preparations. Please review and edit the'
  echo 'settings in Makefile.options. Then run "make".'

  exit $?
fi

package="mkvtoolnix"

olddir=`pwd`
srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

cd "$srcdir"
DIE=0

(autoconf --version) < /dev/null > /dev/null 2>&1 || {
  echo
  echo "You must have autoconf installed to compile $package."
  echo "Download the appropriate package for your distribution,"
  echo "or get the source tarball at ftp://ftp.gnu.org/pub/gnu/"
  DIE=1
}

(autoheader --version) < /dev/null > /dev/null 2>&1 || {
  echo
  echo "You must have autoheader installed to compile $package."
  echo "Download the appropriate package for your distribution,"
  echo "or get the source tarball at ftp://ftp.gnu.org/pub/gnu/"
  DIE=1
}

# (automake --version) < /dev/null > /dev/null 2>&1 || {
#   echo
#   echo "You must have automake installed to compile $package."
#   echo "Download the appropriate package for your system,"
#   echo "or get the source from one of the GNU ftp sites"
#   echo "listed in http://www.gnu.org/order/ftp.html"
#   DIE=1
# }

# (libtool --version) < /dev/null > /dev/null 2>&1 || {
#   echo
#   echo "You must have libtool installed to compile $package."
#   echo "Download the appropriate package for your system,"
#   echo "or get the source from one of the GNU ftp sites"
#   echo "listed in http://www.gnu.org/order/ftp.html"
#   DIE=1
# }

if test "$DIE" -eq 1; then
        exit 1
fi

export WANT_AUTOCONF_2_5=1
AUTOCONFVER=`autoconf --version | head -n 1 | sed 's;[^0-9\.];;g'`
case $AUTOCONFVER in
  2.1*)
    echo autoconf 2.5 or later is required to build mkvtoolnix.
    echo "Download the appropriate package for your distribution,"
    echo "or get the source tarball at ftp://ftp.gnu.org/pub/gnu/"
    exit 1
    ;;
esac

echo "Generating configuration files for $package, please wait...."

rm -f config.h config.h.in stamp-h1 &> /dev/null

#echo "  aclocal $ACLOCAL_FLAGS" && aclocal $ACLOCAL_FLAGS
echo "  autoheader" && autoheader
#echo "  libtoolize --automake" && libtoolize --automake
echo "  autoconf" && autoconf
#echo "  automake --add-missing --copy" && automake --add-missing --copy

echo
echo "You can run './configure' now. If you need dependencies then"
echo "run 'make depend' afterwards."
cd $olddir

#!/bin/sh
# Run this to set up the build system: configure, makefiles, etc.
# (based on the version in enlightenment's cvs)

# For MinGW I use my very own Makefile system. So just copy them over
# to Makefile for convenience's sake.
if gcc -v 2>&1 | grep -i mingw > /dev/null 2> /dev/null; then
  echo Detected MinGW. Will copy the Makefile,mingw to Makefile and
  echo make some adjustments.
  echo ''
  if gcc -o ___getcwd contrib/getcwd.c ; then
    REALCWD=`./___getcwd`
    echo Automatically patching Makefile.mingw.options with the
    echo real top dir: $REALCWD
    if grep ^TOP Makefile.mingw.options > /dev/null 2> /dev/null; then
      sed "s/^TOP.=/TOP = $REALCWD/" < Makefile.mingw.options > mf-tmp
    else
      echo "# TOP dir set automatically by autogen.sh" > mf-tmp
      echo "TOP = $REALCWD" >> mf-tmp
    fi
    mv mf-tmp Makefile.mingw.options
  else
    echo Could not compile a test program for getting the
    echo top level directory. Set it yourself in Makefile.mingw.options
  fi
  rm -f ___getcwd.*
  echo ''

  for i in `find -name Makefile.mingw`; do
    n=`echo $i | sed 's/\.mingw//'`
    echo "Creating $n from $i"
    sed -e "s/Makefile.mingw.common/Makefile.common/g" < $i > $n
  done
  echo "Creating Makefile.common from Makefile.mingw.common"
  sed -e "s!-f Makefile.mingw!!g" < Makefile.mingw.common > Makefile.common

  if test "x$1" = "x"; then
    echo "Creating config.h from config.h.mingw"
    cp config.h.mingw config.h
    echo ''
    echo 'Creating dependencies (calling "make depend")'
    echo ''
    make depend
  else
    echo 'Not creating config.h.'
    echo 'Not creating the dependencies.'
  fi

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

(automake --version) < /dev/null > /dev/null 2>&1 || {
        echo
        echo "You must have automake installed to compile $package."
	echo "Download the appropriate package for your system,
	echo "or get the source from one of the GNU ftp sites"
	echo "listed in http://www.gnu.org/order/ftp.html"
        DIE=1
}

0 && (libtool --version) < /dev/null > /dev/null 2>&1 || {
	echo
	echo "You must have libtool installed to compile $package."
	echo "Download the appropriate package for your system,
	echo "or get the source from one of the GNU ftp sites"
	echo "listed in http://www.gnu.org/order/ftp.html"
	DIE=1
}

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

echo "  aclocal $ACLOCAL_FLAGS" && aclocal $ACLOCAL_FLAGS
echo "  autoheader" && autoheader
#echo "  libtoolize --automake" && libtoolize --automake
echo "  autoconf" && autoconf
echo "  automake --add-missing --copy" && automake --add-missing --copy

cd $olddir

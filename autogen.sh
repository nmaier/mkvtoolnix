#!/bin/sh
# Run this to set up the build system: configure, makefiles, etc.
# (based on the version in enlightenment's cvs)

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
# Ignore automake errors. We need config.sub and config.guess
# which are copied by automake, but we don't use automake for
# managing Makefiles.
echo "  automake --add-missing --copy" && automake --add-missing --copy 2> /dev/null

echo
echo "You can run './configure' followed by 'rake' now."
cd $olddir

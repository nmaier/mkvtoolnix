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
    echo Using autoconf-2.1 style files.
    cp acinclude-2.1.m4 acinclude.m4
    ;;
  *)
    echo Using autoconf-2.5 style files.
    cp acinclude-2.5.m4 acinclude.m4
    ;;
esac

echo "Generating configuration files for $package, please wait...."

echo "  aclocal $ACLOCAL_FLAGS" && aclocal $ACLOCAL_FLAGS
#echo "  autoheader" && autoheader
#echo "  libtoolize --automake" && libtoolize --automake
echo "  autoconf" && autoconf
echo "  automake --add-missing --copy" && automake --add-missing --copy

cd $olddir

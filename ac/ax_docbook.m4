# It's just rude to go over the net to build
XSLTPROC_FLAGS=--nonet
DOCBOOK_ROOT=
for i in /usr/share/xml/docbook/stylesheet/xsl/nwalsh /usr/share/xml/docbook/stylesheet/nwalsh `ls -d /usr/share/xml/docbook/xsl-stylesheets* 2> /dev/null`; do
  if test -d "$i" && test -d "$i/manpages"; then
    DOCBOOK_ROOT="$i/manpages"
  fi
done

AC_PATH_PROG(XSLTPROC, xsltproc)

if test "$XSLTPROC" != ""; then
  DOCBOOK_MANPAGES_STYLESHEET=$DOCBOOK_ROOT/docbook.xsl
  if test -n "$XSLTPROC"; then
    AC_CACHE_CHECK([whether xsltproc works],
      [ac_cv_xsltproc_works],
      [
        ac_cv_xsltproc_works=no
        $XSLTPROC $XSLTPROC_FLAGS $DOCBOOK_MANPAGES_STYLESHEET >/dev/null 2>/dev/null << END
<?xml version="1.0" encoding='ISO-8859-1'?>
<!DOCTYPE book PUBLIC "-//OASIS//DTD DocBook XML V4.5//EN" "http://www.oasis-open.org/docbook/xml/4.5/docbookx.dtd">
<book id="test">
</book>
END
        if test "$?" = 0; then
          ac_cv_xsltproc_works=yes
        fi
      ])

    XSLTPROC_WORKS=$ac_cv_xsltproc_works
  fi
fi

AC_SUBST(DOCBOOK_ROOT)
AC_SUBST(DOCBOOK_MANPAGES_STYLESHEET)
AC_SUBST(XSLTPROC)
AC_SUBST(XSLTPROC_FLAGS)
AC_SUBST(XSLTPROC_WORKS)

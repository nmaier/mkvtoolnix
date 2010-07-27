AC_MSG_CHECKING(the manpage translation languages to install)
MANPAGES_TRANSLATIONS=""
if test x"$LINGUAS" = x ; then
  for file in $srcdir/doc/man/po4a/po/*.po; do
    MANPAGES_TRANSLATIONS="$MANPAGES_TRANSLATIONS`basename $file .po` "
  done
else
  for i in $LINGUAS; do
    if test -f "$srcdir/doc/man/po4a/po/$i.po"; then
      MANPAGES_TRANSLATIONS="$MANPAGES_TRANSLATIONS$i "
    fi
  done
fi
AC_MSG_RESULT($MANPAGES_TRANSLATIONS)

AC_SUBST(MANPAGES_TRANSLATIONS)

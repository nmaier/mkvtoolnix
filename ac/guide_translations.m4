AC_MSG_CHECKING(the mmg guide translation languages to install)
GUIDE_TRANSLATIONS=""
if test x"$LINGUAS" = x ; then
  for entry in $srcdir/doc/guide/* ; do
    test -d "$entry" && GUIDE_TRANSLATIONS="$GUIDE_TRANSLATIONS`basename $entry` "
  done
else
  for i in $LINGUAS; do
    test -d "$srcdir/doc/guide/$i" && GUIDE_TRANSLATIONS="$GUIDE_TRANSLATIONS$i "
  done
fi
AC_MSG_RESULT($GUIDE_TRANSLATIONS)

AC_SUBST(GUIDE_TRANSLATIONS)

AC_MSG_CHECKING(the mmg guide translation languages to install)
GUIDE_TRANSLATIONS=""
for entry in $srcdir/doc/guide/* ; do
  test -d "$entry" && GUIDE_TRANSLATIONS="$GUIDE_TRANSLATIONS ${entry##*/}"
done
AC_MSG_RESULT($GUIDE_TRANSLATIONS)

AC_SUBST(GUIDE_TRANSLATIONS)

AC_MSG_CHECKING(the mmg guide translation languages to install)
GUIDE_TRANSLATIONS="`find $srcdir/doc/guide -maxdepth 1 -type d -printf '%f\n' | sed -e 1d | tr '\n' ' '`"
AC_MSG_RESULT($GUIDE_TRANSLATIONS)

AC_SUBST(GUIDE_TRANSLATIONS)

dnl
dnl Which translations should be installed?
dnl
dnl   AC_CHECK_FUNCS(gettext, gettext_found=yes, gettext_found=no)
dnl   if test x"$gettext_found" != xyes ; then
dnl     AC_CHECK_LIB(intl, gettext,
dnl                  [ LIBINTL_LIBS="-lintl";
dnl                    gettext_found=yes ], gettext_found=no)
dnl   fi
dnl   if test x"$gettext_found" = xyes ; then
dnl     AC_CHECK_HEADERS(libintl.h, libintl_h_found=yes, libintl_h_found=no)
dnl     if test x"$libintl_h_found" = xyes ; then
dnl       AC_MSG_CHECKING(the translations to install)
dnl       if test x"$LINGUAS" = x ; then
dnl         TRANSLATIONS_POS="`echo po/*.po`"
dnl       else
dnl         for i in $LINGUAS; do
dnl           if test -f "po/$i.po" ; then
dnl             TRANSLATIONS_POS="$TRANSLATIONS_POS po/$i.po"
dnl           fi
dnl         done
dnl       fi
dnl       TRANSLATIONS="`echo "$TRANSLATIONS_POS" | \
dnl         sed -e 's/po\///g' -e 's/\.po//g'`"
dnl       AC_MSG_RESULT($TRANSLATIONS)
dnl     fi
dnl     opt_features_yes="$opt_features_yes\n   * translations (gettext)"
dnl   else
dnl     opt_features_no="$opt_features_yes\n   * translations (gettext)"
dnl   fi

AC_SUBST(LIBINTL_LIBS)
AC_SUBST(TRANSLATIONS_POS)
AC_SUBST(TRANSLATIONS)

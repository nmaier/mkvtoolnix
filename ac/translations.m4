dnl
dnl Which translations should be installed?
dnl
AC_ARG_WITH([gettext], AC_HELP_STRING([--without-gettext], [do not build with gettext support]),
            [ with_gettext=${withval} ], [ with_gettext=yes ])
if test "x$with_gettext" != "xno"; then
  AC_CHECK_FUNCS(gettext, gettext_found=yes, gettext_found=no)
fi
if test x"$gettext_found" != xyes ; then
  AC_CHECK_LIB(intl, gettext,
               [ LIBINTL_LIBS="-lintl";
                 gettext_found=yes ],
               [ gettext_found=no ],
               [ -liconv ])
fi

if test x"$gettext_found" = xyes ; then
  AC_CHECK_HEADERS(libintl.h, libintl_h_found=yes, libintl_h_found=no)
  if test x"$libintl_h_found" = xyes ; then
    AC_MSG_CHECKING(the translations to install)
    if test x"$LINGUAS" = x ; then
      TRANSLATIONS_POS="`echo po/*.po`"
    else
      for i in $LINGUAS; do
        if test -f "po/$i.po" ; then
          TRANSLATIONS_POS="$TRANSLATIONS_POS po/$i.po"
        fi
      done
    fi
    TRANSLATIONS="`echo "$TRANSLATIONS_POS" | \
      sed -e 's/po\///g' -e 's/\.po//g'`"
    AC_MSG_RESULT($TRANSLATIONS)
  fi
  opt_features_yes="$opt_features_yes\n   * translations (gettext)"
else
  opt_features_no="$opt_features_no\n   * translations (gettext)"
fi

AC_SUBST(LIBINTL_LIBS)
AC_SUBST(TRANSLATIONS)

dnl
dnl Extra include and library dirs that someone would like to specify.
dnl
AC_ARG_WITH(extra-includes,
  AC_HELP_STRING([--with-extra-includes=DIR],[Path to other include directories separated by ';']),,
  with_extra_include_given=no)
AC_ARG_WITH(extra-libs,
  AC_HELP_STRING([--with-extra-libs=DIR],[Path to other library directories separated by ';']),,
  with_extra_libs_given=no)
  EXTRA_CFLAGS=

  USER_CFLAGS="$CFLAGS"
  USER_CPPFLAGS="$CPPFLAGS"
  USER_CXXFLAGS="$CXXFLAGS"
  USER_LDFLAGS="$LDFLAGS"

  if test "$with_extra_includes" != ""; then
    DIRS=`echo $with_extra_includes | cut -d '=' -f 2 | sed 's,;, -I,g'`
    EXTRA_CFLAGS="-I$DIRS"
    CFLAGS="$CFLAGS -I$DIRS"
    CXXFLAGS="$CXXFLAGS -I$DIRS"
    CPPFLAGS="$CPPFLAGS -I$DIRS"
  fi

  EXTRA_LDFLAGS=
  if test "$with_extra_libs" != ""; then
    DIRS=`echo $with_extra_libs | cut -d '=' -f 2 | sed 's,;, -L,g'`
    EXTRA_LDFLAGS="-L$DIRS"
    LDFLAGS="$LDFLAGS -L$DIRS"
  fi

  LDFLAGS_RPATHS=
dnl  # Don't use rpaths. Most users and distros consider them evil.
dnl  if test "$MINGW" != "1"; then
dnl    LDFLAGS_RPATHS=`echo " $LDFLAGS" | sed 's: -L: -Wl,--rpath :g'`
dnl  fi

AC_SUBST(EXTRA_CFLAGS)
AC_SUBST(EXTRA_LDFLAGS)

AC_SUBST(USER_CFLAGS)
AC_SUBST(USER_CPPFLAGS)
AC_SUBST(USER_CXXFLAGS)
AC_SUBST(USER_LDFLAGS)
AC_SUBST(LDFLAGS_RPATHS)

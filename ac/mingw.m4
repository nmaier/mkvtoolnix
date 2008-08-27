dnl
dnl Check for mingw
dnl
AC_CACHE_CHECK([if being compiled with mingw32],
  [ac_cv_mingw32],[
  if test "x`$CXX --version | grep -i mingw`" = "x" ; then
    ac_cv_mingw32=no
  else
    ac_cv_mingw32=yes
  fi])

  if test "x$ac_cv_mingw32" = "xyes"; then
    export MINGW=1
    MINGW_GUIAPP=-mwindows
    LIBMTXCOMMONDLL=1
    EXEEXT=.exe
  else
    LIBMTXCOMMONDLL=0
  fi

AC_SUBST(MINGW)
AC_SUBST(MINGW_LIBS)
AC_SUBST(MINGW_GUIAPP)
AC_SUBST(LIBMTXCOMMONDLL)
AC_SUBST(EXEEXT)

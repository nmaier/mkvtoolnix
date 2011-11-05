dnl
dnl Check for mingw
dnl
AC_CACHE_CHECK([if being compiled with mingw32],
  [ac_cv_mingw32],[
  case $host in
    *mingw*)
      ac_cv_mingw32=yes
      ;;
    *)
      ac_cv_mingw32=no
      ;;
  esac])

  if test "x$ac_cv_mingw32" = "xyes"; then
    export MINGW=1
    MINGW_GUIAPP=-mwindows
    EXEEXT=.exe

    AC_CHECK_TOOL(WINDRES, windres, :)
  fi

AC_SUBST(MINGW)
AC_SUBST(MINGW_LIBS)
AC_SUBST(MINGW_GUIAPP)
AC_SUBST(EXEEXT)

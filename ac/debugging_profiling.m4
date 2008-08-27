dnl
dnl Debugging and profiling options
dnl
AC_ARG_ENABLE([debug],
  AC_HELP_STRING([--enable-debug],[compile with debug information (no)]))
  if test x"$enable_debug" = x"yes"; then
    dnl debug information
    DEBUG_CFLAGS="-g -DDEBUG"
    OPTIMIZATION_CFLAGS=""
  else
    DEBUG_CFLAGS=""
    OPTIMIZATION_CFLAGS="-O3"
  fi
AC_ARG_ENABLE([profiling],
  AC_HELP_STRING([--enable-profiling],[compile with profiling information (no)]))
  if test x"$enable_profiling" = x"yes"; then
    dnl profiling information
    PROFILING_CFLAGS="-pg"
    PROFILING_LIBS="-pg"
  else
    PROFILING_CFLAGS=""
    PROFILING_LIBS=""
  fi

AC_SUBST(DEBUG_CFLAGS)
AC_SUBST(PROFILING_CFLAGS)
AC_SUBST(PROFILING_LIBS)
AC_SUBST(OPTIMIZATION_CFLAGS)

dnl
dnl Debugging, profiling and optimization options
dnl
DEBUG_CFLAGS=""
OPTIMIZATION_CFLAGS="-O3"
PROFILING_CFLAGS=""
PROFILING_LIBS=""

AC_ARG_ENABLE([optimization],
  AC_HELP_STRING([--enable-optimization],[compile with optimization: -O3 (yes)]),
  , [ enable_optimization=no ])
AC_ARG_ENABLE(
  [debug],
  AC_HELP_STRING([--enable-debug],[compile with debug information (no)]),
  [
    if test x"$enable_debug" = xyes ; then
      DEBUG_CFLAGS="-g -DDEBUG"
      if test x"$enable_optimization" = x"no"; then
        OPTIMIZATION_CFLAGS=""
      fi
    fi
  ])
AC_ARG_ENABLE(
  [profiling],
  AC_HELP_STRING([--enable-profiling],[compile with profiling information (no)]),
  [
    PROFILING_CFLAGS="-pg"
    PROFILING_LIBS="-pg"
  ])

AC_SUBST(DEBUG_CFLAGS)
AC_SUBST(PROFILING_CFLAGS)
AC_SUBST(PROFILING_LIBS)
AC_SUBST(OPTIMIZATION_CFLAGS)

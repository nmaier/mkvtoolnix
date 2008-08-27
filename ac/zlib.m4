dnl
dnl Check for zlib
dnl
  AC_CHECK_LIB(z, zlibVersion,
               [ ZLIB_LIBS="-lz"
                 zlib_found=yes ],
               [ zlib_found=no ],)
  if test "$zlib_found" = "no"; then
    AC_MSG_ERROR([Could not find the zlib library])
  fi
  AC_CHECK_HEADERS(zlib.h, , zlib_found=no)

AC_SUBST(ZLIB_LIBS)

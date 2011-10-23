dnl
dnl Check for libgnurx on mingw
dnl
if test "x$ac_cv_mingw32" = "xyes"; then
  AC_CHECK_LIB(gnurx, regexec, [ gnurx_found=yes ], [ gnurx_found=no ])
fi

if test "x$gnurx_found" = xyes ; then
  GNURX_LIBS=-lgnurx
fi
AC_SUBST(GNURX_LIBS)

dnl
dnl Check for zlib
dnl

PKG_CHECK_MODULES([ZLIB],[zlib],[zlib_found=yes])

if test x"$zlib_found" != xyes; then
  AC_MSG_ERROR([Could not find the zlib library])
fi

dnl
dnl Check for libmagic
dnl

magic_mingw_libs=""
if test "x$ac_cv_mingw32" = "xyes"; then
  magic_mingw_libs="-lshlwapi"
fi

AC_CHECK_LIB(magic, magic_open, [ magic_found=yes ], [ magic_found=no ], [-lz $GNURX_LIBS $magic_mingw_libs])

if test "x$magic_found" = "xyes" ; then
  AC_CHECK_HEADERS([magic.h])
  if test "x$ac_cv_header_magic_h" = "xyes" ; then
    MAGIC_LIBS="-lmagic -lz $GNURX_LIBS $magic_mingw_libs"
    opt_features_yes="$opt_features_yes\n   * libMagic file type detection"
  else
    opt_features_no="$opt_features_no\n   * libMagic file type detection"
  fi
else
  opt_features_no="$opt_features_no\n   * libMagic file type detection"
fi

AC_SUBST(MAGIC_LIBS)

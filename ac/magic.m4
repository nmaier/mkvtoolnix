dnl
dnl Check for libmagic
dnl
AC_CHECK_LIB(magic, magic_open,
             [ MAGIC_LIBS="-lmagic -lz"
               magic_found=yes ],
             [ magic_found=no ],
             -lz)

if test "x$magic_found" = "xyes" ; then
  AC_CHECK_HEADERS([magic.h])
  if test "x$ac_cv_header_magic_h" = "xyes" ; then
    opt_features_yes="$opt_features_yes\n   * libMagic file type detection"
  else
    opt_features_no="$opt_features_no\n   * libMagic file type detection"
  fi
else
  opt_features_no="$opt_features_no\n   * libMagic file type detection"
fi

AC_SUBST(MAGIC_LIBS)

dnl
dnl Check for libbz2
dnl
  AC_ARG_ENABLE([bz2],
    AC_HELP_STRING([--enable-bz2],[compile in support for BZ2 compression (auto)]))
  if test x"$enable_bz2" = x"yes" -o x"$enable_bz2" = "x"; then
    AC_CHECK_LIB(bz2, BZ2_bzCompress,
                 [ BZ2_LIBS="-lbz2"
                   bz2_found=yes ],
                 [ bz2_found=no ],)
    if test "$bz2_found" = "yes"; then
      AC_CHECK_HEADERS(bzlib.h, , bz2_found=no)
    fi
  fi
  if test x"$bz2_found" = xyes ; then
    opt_features_yes="$opt_features_yes\n   * BZ2 compression"
  else
    opt_features_no="$opt_features_no\n   * BZ2 compression"
  fi

AC_SUBST(BZ2_LIBS)

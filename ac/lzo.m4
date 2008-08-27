dnl
dnl Check for liblzo
dnl
  AC_ARG_ENABLE([lzo],
    AC_HELP_STRING([--enable-lzo],[compile in support for LZO compression (auto)]))
  if test "x$enable_lzo" != "xno"; then
    AC_CHECK_LIB(lzo2, lzo1x_1_compress,
                 [ LZO_LIBS="-llzo2"
                   lzo_found=yes ],
                 [ AC_CHECK_LIB(lzo, lzo1x_1_compress,
                                [ LZO_LIBS="-llzo"
                                  lzo_found=yes ],
                                [ lzo_found=no ],)
                 ],)
    if test "x$lzo_found" = "xyes"; then
      AC_CHECK_HEADERS([lzo1x.h lzo/lzo1x.h])
      if test "x$ac_cv_header_lzo1x_h" != "xyes" -a "x$ac_cv_header_lzo_lzo1x_h" != "xyes"; then
        lzo_found=no
      fi
    fi
  fi
  if test x"$lzo_found" = xyes ; then
    opt_features_yes="$opt_features_yes\n   * LZO compression"
    AC_DEFINE([HAVE_LZO], [1], [Define this if LZO compression is present])
  else
    opt_features_no="$opt_features_no\n   * LZO compression"
  fi

AC_SUBST(LZO_LIBS)

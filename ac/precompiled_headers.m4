dnl
dnl Precompiled headers
dnl

AC_ARG_ENABLE([precompiled_headers],
  AC_HELP_STRING([--enable-precompiled-headers],[enable the generation and use of precompiled headers (auto)]),
  [],
  [enable_precompiled_headers=auto])

if test x"$enable_precompiled_headers" = x"auto"; then
  if ! check_version 4.1.0 $ac_cv_gcc_version ; then
    enable_precompiled_headers=no
  else
    enable_precompiled_headers=yes
  fi
fi

if test x"$enable_precompiled_headers" = x"yes"; then
  AC_DEFINE(USE_PRECOMPILED_HEADERS, 1, [Define if precompiled headers are generated and used])
  USE_PRECOMPILED_HEADERS=yes
  opt_features_yes="$opt_features_yes\n   * pre-compiled headers"
else
  opt_features_no="$opt_features_no\n   * pre-compiled headers"
fi

AC_SUBST(USE_PRECOMPILED_HEADERS)

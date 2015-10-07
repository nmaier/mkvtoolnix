dnl
dnl Check for libcurl
dnl

AC_ARG_WITH([curl], AC_HELP_STRING([--without-curl], [do not build with CURL support]),
            [ with_curl=${withval} ], [ with_curl=yes ])
if test "x$with_curl" != "xno"; then
  PKG_CHECK_MODULES([CURL], [libcurl], [curl_found=yes])
fi

if test "$curl_found" = "yes"; then
  opt_features_yes="$opt_features_yes\n   * online update checks (via libcurl)"
  AC_DEFINE(HAVE_CURL_EASY_H, 1, [define if libcurl is found via pkg-config])
else
  opt_features_no="$opt_features_no\n   * online update checks (via libcurl)"
  CURL_CFLAGS=""
  CURL_LIBS=""
fi

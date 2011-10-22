dnl
dnl Check for libcurl
dnl

PKG_CHECK_MODULES([CURL], [libcurl], [curl_found=yes])

if test "$curl_found" = "yes"; then
  opt_features_yes="$opt_features_yes\n   * online update checks (via libcurl)"
else
  opt_features_no="$opt_features_no\n   * online update checks (via libcurl)"
  CURL_CFLAGS=""
  CURL_LIBS=""
fi

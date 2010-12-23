dnl
dnl Check for libcurl
dnl
AC_CHECK_LIB(curl, curl_easy_init,
             [ CURL_LIBS="-lcurl"
               curl_found=yes ],
             [ curl_found=no ],)
if test "$curl_found" = "yes"; then
  AC_CHECK_HEADERS(curl/easy.h, , curl_found=no, [#include <curl/curl.h>])
fi

if test "$curl_found" = "yes"; then
  opt_features_yes="$opt_features_yes\n   * online update checks (via libcurl)"
else
  opt_features_no="$opt_features_no\n   * online update checks (via libcurl)"
  CURL_LIBS=""
fi

AC_SUBST(CURL_LIBS)

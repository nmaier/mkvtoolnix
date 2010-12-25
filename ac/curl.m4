dnl
dnl Check for libcurl
dnl

continue_curl_check=1

AC_ARG_WITH(curl_config,
  AC_HELP_STRING([--with-curl-config=prog],[use prog instead of looking for curl-config]),
  [ CURL_CONFIG="$with_curl_config" ],)
AC_PATH_PROG(CURL_CONFIG, curl-config, no, $PATH:/usr/local/bin)
if test x"$CURL_CONFIG" = "xno" ; then
  echo "*** Not checking for curl: curl-config not found"
  continue_curl_check=0
elif test ! -x "$CURL_CONFIG" ; then
  echo "*** Not checking for curl: file '"$CURL_CONFIG"' not executable"
  continue_curl_check=0
fi

if test x"$continue_curl_check" = x1 ; then
  CURL_CFLAGS=`$CURL_CONFIG --cflags`
  CURL_LIBS=`$CURL_CONFIG --libs`
  ac_save_CXXFLAGS="$CXXFLAGS"
  ac_save_LIBS="$LIBS"
  CXXFLAGS="$CXXFLAGS $CURL_CFLAGS"
  LIBS="$LIBS $CURL_LIBS"
  AC_CHECK_LIB(curl, curl_easy_init,
               [ curl_found=yes ],
               [ curl_found=no ],)
  if test "$curl_found" = "yes"; then
    AC_CHECK_HEADERS(curl/easy.h, , curl_found=no, [#include <curl/curl.h>])
  fi
  CXXFLAGS="$ac_save_CXXFLAGS"
  LIBS="$ac_save_LIBS"
fi

if test "$curl_found" = "yes"; then
  opt_features_yes="$opt_features_yes\n   * online update checks (via libcurl)"
else
  opt_features_no="$opt_features_no\n   * online update checks (via libcurl)"
  CURL_CFLAGS=""
  CURL_LIBS=""
fi

AC_SUBST(CURL_CFLAGS)
AC_SUBST(CURL_LIBS)

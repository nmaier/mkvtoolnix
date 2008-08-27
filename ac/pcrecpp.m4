dnl
dnl Check for libpcre
dnl
AC_PATH_PROG(PCRE_CONFIG, pcre-config, no, $PATH:/usr/local/bin)
if test x"$PCRE_CONFIG" = "xno" ; then
  AC_MSG_ERROR([Cound not find the pcre-config program])
fi

PCRE_CFLAGS="`$PCRE_CONFIG --cflags`"
PCRE_LIBS="`$PCRE_CONFIG --libs` -lpcrecpp"

AC_CACHE_CHECK([for libpcrecpp],[ac_cv_pcre_links],[
  ac_save_LIBS="$LIBS"
  ac_save_CXXFLAGS="$CXXFLAGS"
  LIBS="$LIBS $PCRE_LIBS"
  CXXFLAGS="$CXXFLAGS $PCRE_CFLAGS"

  AC_LANG_PUSH(C++)
  AC_TRY_LINK([
#include <pcrecpp.h>
    ],
    [pcrecpp::RE re("test");],
    ac_cv_pcre_links=yes,
    ac_cv_pcre_links=no)
  AC_LANG_POP

  LIBS="$ac_save_LIBS"
  CXXFLAGS="$ac_save_CXXFLAGS"

])

if test x"$ac_cv_pcre_links" = "xno"; then
  AC_MSG_ERROR([Could not find the PCRE C++ library])
fi

AC_SUBST(PCRE_CFLAGS)
AC_SUBST(PCRE_LIBS)

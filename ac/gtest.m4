AC_DEFUN([AX_GTEST],[
  GTEST_TYPE=no

  AC_LANG_PUSH(C++)
  CPPFLAGS_SAVED="$CPPFLAGS"

  AC_CHECK_LIB(gtest_main,main)
  AC_CHECK_HEADERS(gtest/gtest.h)

  if test x$ac_cv_header_gtest_gtest_h = xyes && test x$ac_cv_lib_gtest_main_main = xyes; then
    GTEST_TYPE=system

  elif test -d lib/gtest/include && test -d lib/gtest/src ; then
    AC_MSG_CHECKING(for internal gtest)
    AC_CACHE_VAL(ax_cv_gtest_internal,[
      CPPFLAGS="$CPPFLAGS_SAVED -Ilib/gtest/include"
      AC_TRY_COMPILE([#include <gtest/gtest.h>],,ax_cv_gtest_internal=yes,ax_cv_gtest_internal=no)
    ])
    AC_MSG_RESULT($ax_cv_gtest_internal)

    if test x$ax_cv_gtest_internal=yes; then
      GTEST_TYPE=internal
    fi
  fi

  AC_LANG_POP
  CPPFLAGS="$CPPFLAGS_SAVED"

  AC_SUBST(GTEST_TYPE)
])

AX_GTEST

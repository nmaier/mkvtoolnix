AC_DEFUN([AX_CXX_STD_CXX0X_FLAG],[
  AC_CACHE_CHECK([for support for the "-std=c++0x" flag], [ax_cv_std_cxx0x_flag],[

    CXXFLAGS_SAVED=$CXXFLAGS
    CXXFLAGS="$CXXFLAGS -std=c++0x"
    export CXXFLAGS

    AC_LANG_PUSH(C++)
    AC_TRY_COMPILE(
      [],
      [true;],
      [ax_cv_std_cxx0x_flag="yes"],
      [ax_cv_std_cxx0x_flag="no"])
    AC_LANG_POP

    CXXFLAGS="$CXXFLAGS_SAVED"
  ])

  STD_CXX0X=""
  if test x"$ax_cv_std_cxx0x_flag" = xyes ; then
    STD_CXX0X=-std=c++0x
  fi
  AC_SUBST(STD_CXX0X)
])

AC_DEFUN([AX_CXX11_INITIALIZER_LISTS],[
  AC_CACHE_CHECK([for support for C++11 feature "initializer lists"], [ax_cv_cxx11_initializer_lists],[

    CXXFLAGS_SAVED=$CXXFLAGS
    CXXFLAGS="$CXXFLAGS $STD_CXX0X"
    export CXXFLAGS

    AC_LANG_PUSH(C++)
    AC_TRY_COMPILE(
      [
#include <string>
#include <vector>
],
      [std::vector<std::string> listy = { "asd", "qwe", "123" };],
      [ax_cv_cxx11_initializer_lists="yes"],
      [ax_cv_cxx11_initializer_lists="no"])
    AC_LANG_POP

    CXXFLAGS="$CXXFLAGS_SAVED"
  ])

  if ! test x"$ax_cv_cxx11_initializer_lists" = xyes ; then
    missing_cxx11_features="$missing_cxx11_features\n  * initializer lists"
  fi
])

AC_DEFUN([AX_CXX11_RANGE_BASED_FOR],[
  AC_CACHE_CHECK([for support for C++11 feature "range-based 'for'"], [ax_cv_cxx11_range_based_for],[

    CXXFLAGS_SAVED=$CXXFLAGS
    CXXFLAGS="$CXXFLAGS $STD_CXX0X"
    export CXXFLAGS

    AC_LANG_PUSH(C++)
    AC_TRY_COMPILE(
      [
#include <string>
#include <vector>
],[
std::string total;
std::vector<std::string> listy = { "asd", "qwe", "123" };
for (std::string &s : listy)
  total += s;
],
      [ax_cv_cxx11_range_based_for="yes"],
      [ax_cv_cxx11_range_based_for="no"])
    AC_LANG_POP

    CXXFLAGS="$CXXFLAGS_SAVED"
  ])

  if ! test x"$ax_cv_cxx11_range_based_for" = xyes ; then
    missing_cxx11_features="$missing_cxx11_features\n  * range-based 'for'"
  fi
])

AC_DEFUN([AX_CXX11_RIGHT_ANGLE_BRACKETS],[
  AC_CACHE_CHECK([for support for C++11 feature "right angle brackets"], [ax_cv_cxx11_right_angle_brackets],[

    CXXFLAGS_SAVED=$CXXFLAGS
    CXXFLAGS="$CXXFLAGS $STD_CXX0X"
    export CXXFLAGS

    AC_LANG_PUSH(C++)
    AC_TRY_COMPILE(
      [
#include <map>
#include <vector>

typedef std::map<int, std::vector<int>> unicorn;
],
      [unicorn charlie;],
      [ax_cv_cxx11_right_angle_brackets="yes"],
      [ax_cv_cxx11_right_angle_brackets="no"])
    AC_LANG_POP

    CXXFLAGS="$CXXFLAGS_SAVED"
  ])

  if ! test x"$ax_cv_cxx11_right_angle_brackets" = xyes ; then
    missing_cxx11_features="$missing_cxx11_features\n  * right angle brackets"
  fi
])

AC_DEFUN([AX_CXX11_AUTO_KEYWORD],[
  AC_CACHE_CHECK([for support for C++11 feature "'auto' keyword"], [ax_cv_cxx11_auto_keyword],[

    CXXFLAGS_SAVED=$CXXFLAGS
    CXXFLAGS="$CXXFLAGS $STD_CXX0X"
    export CXXFLAGS

    AC_LANG_PUSH(C++)
    AC_TRY_COMPILE(
      [
#include <vector>
],[
  std::vector<unsigned int> listy;
  unsigned int sum = 0;
  for (auto i = listy.begin(); i < listy.end(); i++)
    sum += *i;
],
      [ax_cv_cxx11_auto_keyword="yes"],
      [ax_cv_cxx11_auto_keyword="no"])
    AC_LANG_POP

    CXXFLAGS="$CXXFLAGS_SAVED"
  ])

  if ! test x"$ax_cv_cxx11_auto_keyword" = xyes ; then
    missing_cxx11_features="$missing_cxx11_features\n  * 'auto' keyword"
  fi
])

AC_DEFUN([AX_CXX11_LAMBDA_FUNCTIONS],[
  AC_CACHE_CHECK([for support for C++11 feature "lambda functions"], [ax_cv_cxx11_lambda_functions],[

    CXXFLAGS_SAVED=$CXXFLAGS
    CXXFLAGS="$CXXFLAGS $STD_CXX0X"
    export CXXFLAGS

    AC_LANG_PUSH(C++)
    AC_TRY_COMPILE(
      [
#include <algorithm>
#include <vector>
],
      [
std::vector<unsigned int> listy;
unsigned int sum = 0;
std::for_each(listy.begin(), listy.end(), [&](unsigned int i) { sum += i; });
],
      [ax_cv_cxx11_lambda_functions="yes"],
      [ax_cv_cxx11_lambda_functions="no"])
    AC_LANG_POP

    CXXFLAGS="$CXXFLAGS_SAVED"
  ])

  if ! test x"$ax_cv_cxx11_lambda_functions" = xyes ; then
    missing_cxx11_features="$missing_cxx11_features\n  * lambda functions"
  fi
])

AC_DEFUN([AX_CXX11_NULLPTR],[
  AC_CACHE_CHECK([for support for C++11 feature "nullptr"], [ax_cv_cxx11_nullptr],[

    CXXFLAGS_SAVED=$CXXFLAGS
    CXXFLAGS="$CXXFLAGS $STD_CXX0X"
    export CXXFLAGS

    AC_LANG_PUSH(C++)
    AC_TRY_COMPILE(
      [],
      [unsigned char const *charlie = nullptr;],
      [ax_cv_cxx11_nullptr="yes"],
      [ax_cv_cxx11_nullptr="no"])
    AC_LANG_POP

    CXXFLAGS="$CXXFLAGS_SAVED"
  ])

  if ! test x"$ax_cv_cxx11_nullptr" = xyes ; then
    missing_cxx11_features="$missing_cxx11_features\n  * nullptr"
  fi
])

dnl AC_DEFUN([AX_CXX11_DEF_NAME],[
dnl   AC_CACHE_CHECK([for support for C++11 feature "human"], [ax_cv_cxx11_def_name],[
dnl
dnl     CXXFLAGS_SAVED=$CXXFLAGS
dnl     CXXFLAGS="$CXXFLAGS $STD_CXX0X"
dnl     export CXXFLAGS
dnl
dnl     AC_LANG_PUSH(C++)
dnl     AC_TRY_COMPILE(
dnl       [],
dnl       [],
dnl       [ax_cv_cxx11_def_name="yes"],
dnl       [ax_cv_cxx11_def_name="no"])
dnl     AC_LANG_POP
dnl
dnl     CXXFLAGS="$CXXFLAGS_SAVED"
dnl   ])
dnl
dnl   if ! test x"$ax_cv_cxx11_def_name" = xyes ; then
dnl     missing_cxx11_features="$missing_cxx11_features\n  * human"
dnl   fi
dnl ])

AX_CXX_STD_CXX0X_FLAG
AX_CXX11_INITIALIZER_LISTS
AX_CXX11_RANGE_BASED_FOR
AX_CXX11_RIGHT_ANGLE_BRACKETS
AX_CXX11_AUTO_KEYWORD
AX_CXX11_LAMBDA_FUNCTIONS
AX_CXX11_NULLPTR

if test x"$missing_cxx11_features" != x ; then
  printf "The following features of the C++11 standard are not supported by $CXX:$missing_cxx11_features\n"
  printf "If you are using the GNU C compiler collection (gcc) then you need\n"
  printf "at least v4.6.\n"
  AC_MSG_ERROR([support for required C++11 features incomplete])
fi

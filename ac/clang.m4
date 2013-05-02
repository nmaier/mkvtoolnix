AC_DEFUN([AX_COMPILER_IS_CLANG],[
  AC_CACHE_CHECK([compiler is clang], [ac_cv_compiler_is_clang], [
    if $CXX --version | grep -q -i clang ; then
      ac_cv_compiler_is_clang=yes
      USE_CLANG=yes
    else
      ac_cv_compiler_is_clang=no
    fi
  ])
])

AX_COMPILER_IS_CLANG

QUNUSED_ARGUMENTS=""
WNO_SELF_ASSIGN=""
WNO_MISMATCHED_TAGS=""

if test x"$ac_cv_compiler_is_clang" = xyes; then
  QUNUSED_ARGUMENTS="-Qunused-arguments"
  WNO_SELF_ASSIGN="-Wno-self-assign"
  WNO_MISMATCHED_TAGS="-Wno-mismatched-tags"
fi

AC_SUBST(QUNUSED_ARGUMENTS)
AC_SUBST(WNO_SELF_ASSIGN)
AC_SUBST(WNO_MISMATCHED_TAGS)
AC_SUBST(USE_CLANG)

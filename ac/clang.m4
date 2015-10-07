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

AC_SUBST(USE_CLANG)

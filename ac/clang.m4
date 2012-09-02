AC_DEFUN([AX_COMPILER_IS_CLANG],[
  AC_CACHE_CHECK([compiler is clang], [ac_cv_compiler_is_clang], [
    if $CXX --version | grep -q -i 'clang.*llvm' ; then
      ac_cv_compiler_is_clang=yes
    else
      ac_cv_compiler_is_clang=no
    fi
  ])
])

AC_DEFUN([AX_QUNUSED_ARGUMENTS_FLAG],[
  AC_CACHE_CHECK([for the compiler flag "-Qunused-arguments"], [ax_cv_qunused_arguments_flag],[

    CXXFLAGS_SAVED=$CXXFLAGS
    CXXFLAGS="$CXXFLAGS -Qunused-arguments"
    export CXXFLAGS

    AC_LANG_PUSH(C++)
    AC_TRY_COMPILE(
      [],
      [1;],
      [ax_cv_qunused_arguments_flag="yes"],
      [ax_cv_qunused_arguments_flag="no"])
    AC_LANG_POP

    CXXFLAGS="$CXXFLAGS_SAVED"
  ])

  QUNUSED_ARGUMENTS=""
  if test x"$ax_cv_qunused_arguments_flag" = xyes ; then
    QUNUSED_ARGUMENTS="-Qunused-arguments"
  fi
  AC_SUBST(QUNUSED_ARGUMENTS)
])

AX_QUNUSED_ARGUMENTS_FLAG
AX_COMPILER_IS_CLANG

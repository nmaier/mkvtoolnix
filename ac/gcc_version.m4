dnl
dnl Check the gcc version for the genreation of dependecy information
dnl
AC_CACHE_CHECK([gcc version],
  [ac_cv_gcc_version],[
  ac_cv_gcc_version="`$CXX -dumpversion`"
  ])

case "$ac_cv_gcc_version" in
  2.*)
    GCC_DEP_STYLE=v2
    ;;
  3.*)
    GCC_DEP_STYLE=v3
    ;;
  *)
    GCC_DEP_STYLE=v3
    ;;
esac

AC_SUBST(GCC_DEP_STYLE)

QUNUSED_ARGUMENTS=""
WNO_SELF_ASSIGN=""
WNO_MISMATCHED_TAGS=""
WLOGICAL_OP=""

if test x"$ac_cv_compiler_is_clang" = xyes; then
  QUNUSED_ARGUMENTS="-Qunused-arguments"
  WNO_SELF_ASSIGN="-Wno-self-assign"
  WNO_MISMATCHED_TAGS="-Wno-mismatched-tags"

elif check_version 4.8.0 $ac_cv_gcc_version ; then
  WLOGICAL_OP="-Wlogical-op"
fi

AC_SUBST(QUNUSED_ARGUMENTS)
AC_SUBST(WNO_SELF_ASSIGN)
AC_SUBST(WNO_MISMATCHED_TAGS)
AC_SUBST(WLOGICAL_OP)

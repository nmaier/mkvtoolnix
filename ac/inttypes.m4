dnl
dnl Test if the 64bit integer types are available, and if not if they
dnl can be typedef'ed manually.
dnl
AC_MSG_CHECKING(for int64_t)
AC_LANG_PUSH(C++)
AC_CACHE_VAL(ac_cv_has_int64_t,[
  AC_TRY_COMPILE([
#if HAVE_INTTYPES_H
# include <inttypes.h>
#endif
#if HAVE_STDINT_H
# include <stdint.h>
#endif
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
    ],
    [int64_t foo;],
    ac_cv_has_int64_t=yes,
    ac_cv_has_int64_t=no)
  ])
AC_MSG_RESULT($ac_cv_has_int64_t)

AC_MSG_CHECKING(for uint64_t)
AC_CACHE_VAL(ac_cv_has_uint64_t,[
  AC_TRY_COMPILE([
#if HAVE_INTTYPES_H
# include <inttypes.h>
#endif
#if HAVE_STDINT_H
# include <stdint.h>
#endif
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
    ],
    [int64_t foo;],
    ac_cv_has_uint64_t=yes,
    ac_cv_has_uint64_t=no)
  ])
AC_MSG_RESULT($ac_cv_has_uint64_t)
AC_LANG_POP

AC_CHECK_SIZEOF(int)
AC_CHECK_SIZEOF(long)
AC_CHECK_SIZEOF(long long)

if test x$ac_cv_has_int64_t = "xyes" ; then
  TYPE64="int64_t"
else
  case 8 in
    $ac_cv_sizeof_int) TYPE64="int";;
    $ac_cv_sizeof_long) TYPE64="long";;
    $ac_cv_sizeof_long_long) TYPE64="long long";;
  esac
  AC_DEFINE(HAVE_NO_INT64_T, 1, [int64_t does not exist])
  AC_DEFINE_UNQUOTED(INT64_TYPE, $TYPE64,
                     [the type to define int64_t manually])
fi

if test x$ac_cv_has_uint64_t = "xyes" ; then
  TYPEU64="int64_t"
else
  case 8 in
    $ac_cv_sizeof_int) TYPEU64="unsigned int";;
    $ac_cv_sizeof_long) TYPEU64="unsigned long";;
    $ac_cv_sizeof_long_long) TYPEU64="unsigned long long";;
  esac
  AC_DEFINE(HAVE_NO_UINT64_T, 1, [uint64_t does not exist])
  AC_DEFINE_UNQUOTED(UINT64_TYPE, $TYPEU64,
                     [the type to define uint64_t manually])
fi

if test -z "$TYPE64"; then
  AC_MSG_ERROR(No 64 bit type found on this platform!)
fi
if test -z "$TYPEU64"; then
  AC_MSG_ERROR(No unsigned 64 bit type found on this platform!)
fi

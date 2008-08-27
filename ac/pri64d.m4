dnl
dnl Verify that PRId64 and PRIu64 are available
dnl
AC_MSG_CHECKING(for PRId64 and PRIu64)
AC_CACHE_VAL(ac_cv_has_prix64,[
  AC_TRY_COMPILE([
#define __STDC_FORMAT_MACROS
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
    [char *dummy = "text " PRId64 " text " PRIu64;],
    ac_cv_has_prix64=yes,
    ac_cv_has_prix64=no)
  ])
AC_MSG_RESULT($ac_cv_has_prix64)

if test x$ac_cv_has_prix64 = "xno" ; then
  echo "*** On your system the #define PRId64 and/or PRIu64 was not found."
  echo "*** These are required for compilation. Please contact the author."
  exit 1
fi

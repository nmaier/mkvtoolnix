dnl
dnl Check for posix_fadvise and its definitions
dnl
AC_ARG_ENABLE(posix-fadvise,
  AC_HELP_STRING([--disable-posix-fadvise],[do not use posix_fadvise (auto)]),,
  [enable_posix_fadvise=yes])

if test x"$enable_posix_fadvise" != "xno" ; then 
  AC_CACHE_CHECK([for posix_fadvise], [ac_cv_posix_fadvise],[
    ac_cv_posix_fadvise="no"
    AC_LANG_PUSH(C++)
    AC_TRY_COMPILE([
#include <fcntl.h>
      ],[
        posix_fadvise(0, 0, 0, POSIX_FADV_WILLNEED);
        posix_fadvise(0, 0, 0, POSIX_FADV_DONTNEED);
      ],[ac_cv_posix_fadvise="yes"])
    AC_LANG_POP
  ])
  if test x"$ac_cv_posix_fadvise" = "xyes" ; then
    AC_DEFINE([HAVE_POSIX_FADVISE], 1, [define if posix_advise and its definitions are available])
  fi
fi

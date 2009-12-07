AC_CACHE_CHECK([for ioctl & TIOCGWINSZ], [ac_cv_tiocgwinsz],[
  ac_cv_tiocgwinsz="no"
  AC_LANG_PUSH(C++)
  AC_TRY_COMPILE([
      #include <sys/ioctl.h>
    ],[
      struct winsize ws;
      ioctl(0, TIOCGWINSZ, &ws);
    ],[ac_cv_tiocgwinsz="yes"])
  AC_LANG_POP
])
if test x"$ac_cv_posix_fadvise" = "xyes" ; then
  AC_DEFINE([HAVE_TIOCGWINSZ], 1, [define if ioctl & TIOCGWINSZ are available])
fi
